/*
 * This file is part of the ZYNQ-IPMC Framework.
 *
 * The ZYNQ-IPMC Framework is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The ZYNQ-IPMC Framework is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the ZYNQ-IPMC Framework.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <core.h>
#include <services/timer/timer.h>
#include <drivers/elm/quiesce/quiesce.h>

ELMQuiesce::~ELMQuiesce() {
	MutexGuard<true> lock(this->mutex);
	if (this->quiesce_timer) {
		this->quiesce_timer->cancel(true);
		if (this->logtree)
			this->logtree->log("ELMQuiesce destroyed while a quiesce was pending.", LogTree::LOG_WARNING);
	}
}

void ELMQuiesce::recv(const uint8_t *content, size_t size) {
	MutexGuard<true> lock(this->mutex);
	std::string message((const char *)content, size);
	if (message == "QUIESCE_SUBSCRIBE") {
		if (this->logtree) {
			if (this->subscribe_timeout.getTimeout() <= 0)
				this->logtree->log("The ELM started a quiesce subscription.", LogTree::LOG_INFO);
			else
				this->logtree->log("The ELM renewed a quiesce subscription.", LogTree::LOG_DIAGNOSTIC);
		}
		this->subscribe_timeout.setTimeout(this->subscribe_validity);
	}
	else if (message == "QUIESCE_UNSUBSCRIBE") {
		this->subscribe_timeout.setAbsTimeout(0);
		if (this->logtree)
			this->logtree->log("The ELM has cancelled its quiesce subscription.", LogTree::LOG_INFO);
	}
	else if (message == "QUIESCE_COMPLETE") {
		this->quiesceComplete(true);
	}
}

void ELMQuiesce::quiesce(QuiesceCompleteCallback callback) {
	MutexGuard<true> lock(this->mutex);
	if (this->subscribe_timeout.getTimeout() == 0) {
		// The ELM is not currently accepting quiescence requests.
		if (this->logtree)
			this->logtree->log("Quiescence was requested, but the ELM is not currently subscribed to quiescence requests.", LogTree::LOG_INFO);
		callback(false);
		return;
	}
	// Okay, not immediately returning, so let's queue this callback.
	this->callbacks.push_back(callback);

	if (this->quiesce_timer) {
		// A quiesce is already in progress.  We'll service your callback though.
		this->logtree->log("Quiescence was requested, but quiesce is already in progress.", LogTree::LOG_DIAGNOSTIC);
		return;
	}

	if (this->logtree)
		this->logtree->log("Asking the ELM to quiesce.", LogTree::LOG_INFO);

	this->quiesce_timer = std::make_shared<TimerService::Timer>([this]() -> void {
		MutexGuard<true> lock(this->mutex);
		for (auto callback : this->callbacks)
			callback(false); // If we got this far, it failed.
		this->callbacks.clear();
		this->quiesce_timer = nullptr;
		if (this->logtree)
			this->logtree->log("The ELM has failed to quiesce.", LogTree::LOG_WARNING);
	}, this->quiesce_timeout);
	TimerService::globalTimer(TASK_PRIORITY_SERVICE).submit(this->quiesce_timer);
	this->send((const uint8_t*)"QUIESCE_NOW", 11);
}

void ELMQuiesce::quiesceComplete(bool successful) {
	MutexGuard<true> lock(this->mutex);
	if (this->quiesce_timer) {
		// If we have a quiesce waiting, complete it.
		this->quiesce_timer->cancel();
		this->quiesce_timer = nullptr;
		if (this->logtree) {
			if (successful)
				this->logtree->log("The ELM has quiesced at our request.",
						LogTree::LOG_INFO);
			else
				this->logtree->log("The ELM has failed to quiesce.",
						LogTree::LOG_WARNING);
		}
		for (auto callback : this->callbacks)
			callback(successful);
		this->callbacks.clear();
	}
	else {
		if (this->logtree && successful)
			this->logtree->log("The ELM has quiesced of its own accord.", LogTree::LOG_INFO);
	}
	this->subscribe_timeout.setAbsTimeout(0); // No longer subscribed.
};

bool ELMQuiesce::quiesceInProgress() {
	MutexGuard<true> lock(this->mutex);
	return (bool)this->quiesce_timer;
};

bool ELMQuiesce::quiesceSubscribed() {
	MutexGuard<true> lock(this->mutex);
	return this->subscribe_timeout.getTimeout() > 0;
}

namespace {
class QuiesceCommand : public CommandParser::Command {
public:
	QuiesceCommand(ELMQuiesce &quiesce) : quiesce(quiesce) { };

	virtual std::string getHelpText(const std::string &command) const {
		return command + " [in|closed|out|open|release|save]\n\n"
				"Set the handle override state.\n"
				"\n"
				"  in/closed: Always treat the handle as if it is closed.\n"
				"  out/open:  Always treat the handle as if it is open.\n"
				"  release:   Always treat the handle according to its actual state.\n"
				"  save:      Write the current value of this setting to persistent storage.\n"
				"             This will cause it to be automatically restored at IPMC startup.\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		if (!this->quiesce.quiesceSubscribed()) {
			console->write("The ELM is not currently subscribed to quiesce requests.");
			return;
		}
		if (this->quiesce.quiesceInProgress()) {
			console->write("The ELM has already been asked to quiesce. (We'll still let you know when it's finished.)");
		}
		this->quiesce.quiesce([console](bool success) -> void {
			if (success)
				console->write("The ELM has quiesced.");
			else
				console->write("The ELM has failed to quiesce within the time limit.");
		});
	}

private:
	ELMQuiesce &quiesce;
};
} // namespace <anonymous>

void ELMQuiesce::registerConsoleCommands(CommandParser &parser, const std::string &prefix) {
	parser.registerCommand(prefix + "quiesce", std::make_shared<QuiesceCommand>(*this));
}

void ELMQuiesce::deregisterConsoleCommands(CommandParser &parser, const std::string &prefix) {
	parser.registerCommand(prefix + "quiesce", nullptr);
}
