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
	if (this->fsm_timer)
		this->fsm_timer->cancel(true);
	if (this->send_request_timer)
		this->send_request_timer->cancel(true);
	// Callbacks are not called.
}

void ELMQuiesce::elm_powered_on() {
	MutexGuard<true> lock(this->mutex);
	/* It can occur that while we are in the process of quiescing, the shelf
	 * manager chooses to send a "Set Power Level: 1" command.  If we reset our
	 * state machine based on the ELM being powered on, therefore, we can end up
	 * in a state where the callbacks are not called, and should not be called,
	 * but in which calling them would produce undesirable effects. (Responding
	 * to "Set Power Level: 1", by immediately terminating a quiesce and
	 * powering off regardless of actual safety.)
	 *
	 * In order to protect higher level FSMs, we must sustain our guarantee that
	 * the callbacks will be called.  As such, we will NOT reset our FSM here if
	 * it is already active.  If we were in the process of quiescing the ELM, we
	 * cannot terminate that process on the ELM side and leave the system in a
	 * useful state.  We will therefore simply choose to allow any quiesce in
	 * progress to complete naturally.
	 */
	// this->elm_powered_off();
	if (this->quiesce_request_timestamp) {
		// A quiesce is already in progress.
		// We won't mess with the startup timestamp under these conditions.
	}
	else {
		this->startup_timestamp = get_tick64();
	}
};

void ELMQuiesce::elm_powered_off() {
	MutexGuard<true> lock(this->mutex);
	// The ELM is now known to be COLD.
	// We will fail any quiesce in progress, since we recieved no confirmation
	// from the ELM.
	this->quiesceComplete(false);
	this->startup_timestamp = 0;
};

// #define DEBUG_QUIESCE_FSM
void ELMQuiesce::quiesce_fsm() {
	MutexGuard<true> lock(this->mutex);
	if (this->fsm_timer) {
		// Clear any pre-existing timer.
		// This is fine to do with sync=false if we're running from it.
		this->fsm_timer->cancel(false);
		this->fsm_timer = nullptr;
	}
	if (this->quiesce_request_timestamp == 0)
		return;
	// We have a request.
	uint64_t now = get_tick64();
	AbsoluteTimeout next_time(UINT64_MAX);
#ifdef DEBUG_QUIESCE_FSM
	this->logtree->log(std::string("this->startup_timestamp = ") + std::to_string(this->startup_timestamp) + std::string("; this->startup_allowance = ") + std::to_string(this->startup_allowance), LogTree::LOG_DIAGNOSTIC);
	this->logtree->log(std::string("this->quiesce_request_timestamp = ") + std::to_string(this->quiesce_request_timestamp) + std::string("; this->acknowledgement_timeout = ") + std::to_string(this->acknowledgement_timeout), LogTree::LOG_DIAGNOSTIC);
	this->logtree->log(std::string("this->quiesce_acknowledgement_timestamp = ") + std::to_string(this->quiesce_acknowledgement_timestamp) + std::string("; this->quiesce_timeout = ") + std::to_string(this->quiesce_timeout), LogTree::LOG_DIAGNOSTIC);
#endif
	uint64_t startup_deadline = this->startup_timestamp + this->startup_allowance;
	uint64_t acknowledgement_deadline = this->quiesce_request_timestamp + this->acknowledgement_timeout;
	uint64_t completion_deadline = this->quiesce_acknowledgement_timestamp + this->quiesce_timeout;
#ifdef DEBUG_QUIESCE_FSM
	this->logtree->log(std::string("Startup deadline: ") + std::to_string(startup_deadline - now), LogTree::LOG_DIAGNOSTIC);
	this->logtree->log(std::string("Acknowledgement deadline: ") + std::to_string(acknowledgement_deadline - now), LogTree::LOG_DIAGNOSTIC);
	this->logtree->log(std::string("Completion deadline: ") + std::to_string(completion_deadline - now), LogTree::LOG_DIAGNOSTIC);
#endif
	if (this->quiesce_acknowledgement_timestamp == 0) {
		// We have an unacknowledged request.
		if (startup_deadline <= now && acknowledgement_deadline <= now) {
			// They've had their chance.
			this->logtree->log("The ELM failed to acknowledge our quiescence request.", LogTree::LOG_NOTICE);
			this->quiesceComplete(false);
			this->quiesce_request_timestamp = 0;
			return;
		}
		else {
			// They still have time.  But how much?
			// We're resending our request in another timer.
			if (startup_deadline > acknowledgement_deadline)
				next_time.setAbsTimeout(startup_deadline);
			else
				next_time.setAbsTimeout(acknowledgement_deadline);
		}
	}
	else {
		// We have an acknowledged request.
		if (completion_deadline <= now) {
			// They've had plenty of time.
			this->logtree->log("The ELM failed to quiesce in a timely manner.", LogTree::LOG_NOTICE);
			this->quiesceComplete(false);
			this->quiesce_request_timestamp = 0;
			return;
		}
		else {
			// Give them time...
			next_time.setAbsTimeout(completion_deadline);
		}
	}
#ifdef DEBUG_QUIESCE_FSM
	this->logtree->log(std ::string("Advancing Quiesce FSM: ") + std::to_string(next_time.getTimeout()), LogTree::LOG_DIAGNOSTIC);
#endif
	if (next_time.getTimeout64() != UINT64_MAX) {
		this->fsm_timer = std::make_shared<TimerService::Timer>(
				[this]() -> void { this->quiesce_fsm(); }, next_time);
		TimerService::globalTimer(TASK_PRIORITY_SERVICE).submit(this->fsm_timer);
	}
}

void ELMQuiesce::recv(const uint8_t *content, size_t size) {
	MutexGuard<true> lock(this->mutex);
	std::string message((const char *)content, size);
	if (message == "QUIESCE_ACKNOWLEDGED") {
		if (this->logtree)
			this->logtree->log("The ELM acknowledged our request to quiesce.", LogTree::LOG_INFO);

		if (this->send_request_timer) {
			// We don't need to keep sending requests anymore.
			this->send_request_timer->cancel(false);
			this->send_request_timer = nullptr;
		}
		this->quiesce_acknowledgement_timestamp = get_tick64();
		this->quiesce_fsm(); // Reset the FSM timer by running it.
	}
	else if (message == "QUIESCE_COMPLETE") {
		this->quiesceComplete(true);
	}
}

void ELMQuiesce::quiesce(QuiesceCompleteCallback callback) {
	MutexGuard<true> lock(this->mutex);
	uint64_t now = get_tick64();

	if (now <= this->startup_timestamp + this->panic_window) {
		if (this->logtree)
			this->logtree->log("Assuming the ELM is already (still) quiescent.  We're still within the panic window.", LogTree::LOG_INFO);
		callback(true);
		return;
	}

	// We'll always accept callbacks.
	this->callbacks.push_back(callback);

	if (this->quiesce_request_timestamp != 0) {
		// A quiesce is already in progress.  We'll service your callback though.
		this->logtree->log("Quiescence was requested, but quiescence is already in progress.", LogTree::LOG_DIAGNOSTIC);
		return;
	}
	this->quiesce_request_timestamp = now;
	this->quiesce_acknowledgement_timestamp = 0;

	if (this->logtree)
		this->logtree->log("Asking the ELM to quiesce.", LogTree::LOG_INFO);

	this->send_request_timer = std::make_shared<TimerService::Timer>([this]() -> void {
		this->send("QUIESCE_NOW");
	}, (TickType_t)0, (TickType_t)pdMS_TO_TICKS(1000));
	TimerService::globalTimer(TASK_PRIORITY_SERVICE).submit(this->send_request_timer);

	this->quiesce_fsm(); // Start the timer by running the FSM.  It's idempotent.
}

void ELMQuiesce::quiesceComplete(bool successful) {
	MutexGuard<true> lock(this->mutex);

	if (this->logtree) {
		if (this->quiesce_request_timestamp != 0) {
			if (successful)
				this->logtree->log("The ELM has quiesced at our request.",
						LogTree::LOG_INFO);
			else
				this->logtree->log("The ELM has failed to quiesce.",
						LogTree::LOG_WARNING);
		}
		else if (successful)
			this->logtree->log("The ELM has quiesced of its own accord.", LogTree::LOG_INFO);
	}

	if (this->fsm_timer) {
		this->fsm_timer->cancel(false);
		this->fsm_timer = nullptr;
	}
	if (this->send_request_timer) {
		this->send_request_timer->cancel(false);
		this->send_request_timer = nullptr;
	}
	this->quiesce_request_timestamp = 0;
	this->quiesce_acknowledgement_timestamp = 0;

	/* If we register a quiesce callback that includes shutting off payload
	 * power, that will call elm_powered_off, which will call
	 * quiesceComplete(false).  The callback list must be cleared by this point.
	 * Therefore, we will run the callbacks from a copy, having cleared this to
	 * begin with.
	 *
	 * This didn't fail before, for reasons probably related to the finer
	 * details of list iteration.  It is explicit now.
	 */
	std::list<QuiesceCompleteCallback> callbacks = this->callbacks;
	this->callbacks.clear();
	for (auto callback : callbacks)
		callback(successful);
	this->callbacks.clear();
};

bool ELMQuiesce::quiesceInProgress() {
	MutexGuard<true> lock(this->mutex);
	return (bool)this->quiesce_request_timestamp;
};

namespace {
class QuiesceCommand : public CommandParser::Command {
public:
	QuiesceCommand(ELMQuiesce &quiesce) : quiesce(quiesce) { };

	virtual std::string getHelpText(const std::string &command) const {
		return command + "\n"
				"\n"
				"Ask the ELM to quiesce.\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		if (this->quiesce.quiesceInProgress()) {
			console->write("The ELM has already been asked to quiesce. (We'll still let you know when it's finished.)");
		}
		this->quiesce.quiesce([console](bool success) -> void {
			if (success)
				console->write("The ELM has quiesced.");
			else
				console->write("The ELM has failed to quiesce.");
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
