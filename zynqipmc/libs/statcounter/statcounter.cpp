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

#include "statcounter.h"
#include <libs/printf.h>
#include <libs/threading.h>
#include <services/console/consolesvc.h>

StatCounter::StatCounter(std::string name)
  : kName(stdsprintf("%s@%08x", name.c_str(), reinterpret_cast<unsigned int>(this))), count(0) {
	safe_init_static_mutex(StatCounter::mutex, true);
	MutexGuard<true> lock(StatCounter::mutex, true);
	if (!StatCounter::registry)
		StatCounter::registry = new std::map< std::string, StatCounter* >();
	StatCounter::registry->insert(std::make_pair(this->kName, this));
}

StatCounter::~StatCounter() {
	MutexGuard<true> lock(StatCounter::mutex, true);
	StatCounter::registry->erase(this->kName);
}

uint64_t StatCounter::get() const {
	CriticalGuard critical(true);
	return this->count;
}

uint64_t StatCounter::fastGet() const {
	return this->count;
}

uint64_t StatCounter::set(uint64_t val) {
	CriticalGuard critical(true);
	uint64_t ret = this->count;
	this->count = val;
	return ret;
}

uint64_t StatCounter::increment(uint64_t inc) {
	CriticalGuard critical(true);
	uint64_t ret = this->count;
	if ((UINT64_MAX - inc) <= this->count)
		this->count = UINT64_MAX;
	else
		this->count += inc;
	return ret;
}

uint64_t StatCounter::decrement(uint64_t dec) {
	CriticalGuard critical(true);
	uint64_t ret = this->count;
	if (dec > this->count)
		this->count = 0;
	else
		this->count -= dec;
	return ret;
}

uint64_t StatCounter::highWater(uint64_t val) {
	CriticalGuard critical(true);
	uint64_t ret = this->count;
	if (this->count < val)
		this->count = val;
	return ret;
}

uint64_t StatCounter::lowWater(uint64_t val) {
	CriticalGuard critical(true);
	uint64_t ret = this->count;
	if (this->count > val)
		this->count = val;
	return ret;
}

SemaphoreHandle_t StatCounter::getRegistryMutex() {
	safe_init_static_mutex(StatCounter::mutex, true);
	return StatCounter::mutex;
}

std::map< std::string, StatCounter* > StatCounter::getRegistry() {
	safe_init_static_mutex(StatCounter::mutex, true);
	configASSERT(xSemaphoreGetMutexHolder(StatCounter::mutex) == xTaskGetCurrentTaskHandle());
	MutexGuard<true> lock(StatCounter::mutex, true);
	if (!StatCounter::registry)
		StatCounter::registry = new std::map< std::string, StatCounter* >();
	return std::map< std::string, StatCounter *>(*StatCounter::registry);
}

// Static storage locations for StatCounter static variables.
SemaphoreHandle_t StatCounter::mutex;
std::map< std::string, StatCounter* > * volatile StatCounter::registry = nullptr;

class StatCounter::Stats : public CommandParser::Command {
public:
	static std::string strip_address(const std::string &str) {
		return str.substr(0, str.find_last_of('@'));
	}

	virtual std::string getHelpText(const std::string &command) const {
		return stdsprintf(
				"%s [pattern]\n"
				"\n"
				"Retrieves the values of all matching stat counters.\n"
				"\n"
				"Patterns may be an exact stat counter name or end with \"*\".\n"
				"Without a pattern, it displays all stat counters.\n", command.c_str());
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		std::string out;
		std::string pattern;

		if (!parameters.parseParameters(1, true, &pattern))
			pattern = "*";

		bool exact = true;
		if (pattern.size() && pattern[pattern.size()-1] == '*') {
			pattern.pop_back();
			exact = false;
		}

		MutexGuard<true> lock(StatCounter::getRegistryMutex(), true);
		std::map<std::string, StatCounter*> registry = StatCounter::getRegistry();
		for (auto &entry : registry) {
			if (exact && strip_address(entry.first) != pattern)
				continue;
			else if (strip_address(entry.first).substr(0, pattern.size()) != pattern)
				continue;
			out += stdsprintf("%-60s %20llu\n", strip_address(entry.first).c_str(), entry.second->get());
		}
		console->write(out);
	}

	virtual std::vector<std::string> complete(const CommandParser::CommandParameters &parameters) const {
		if (parameters.cursor_parameter != 1)
			return std::vector<std::string>(); // Can't help you.

		std::vector<std::string> out;
		std::string pattern;

		parameters.parseParameters(1, true, &pattern);

		MutexGuard<true> lock(StatCounter::getRegistryMutex(), true);
		std::map<std::string, StatCounter*> registry = StatCounter::getRegistry();
		for (auto &entry : registry) {
			if (strip_address(entry.first).substr(0, pattern.size()) != pattern)
				continue;
			out.push_back(strip_address(entry.first));
		}
		return std::move(out);
	};
};

void StatCounter::registerConsoleCommands(CommandParser &parser, const std::string &prefix) {
	parser.registerCommand(prefix + "stats", std::make_shared<StatCounter::Stats>());
}

void StatCounter::deregisterConsoleCommands(CommandParser &parser, const std::string &prefix) {
	parser.registerCommand(prefix + "stats", nullptr);
}
