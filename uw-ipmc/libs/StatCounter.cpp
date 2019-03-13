/*
 * StatCounter.cpp
 *
 *  Created on: Dec 5, 2017
 *      Author: jtikalsky
 */

#include <IPMC.h>
#include <libs/StatCounter.h>
#include <libs/ThreadingPrimitives.h>
#include <libs/printf.h>
#include <services/console/ConsoleSvc.h>
#include <FreeRTOS.h>
#include <semphr.h>
#include <queue.h>
#include <task.h>
#include <string>
#include <map>

/**
 * Instantiate a new stat counter.  It will automatically be registered in the
 * global StatCounter registry.
 *
 * \note StatCounter names should be in a reverse dotted format:
 *       `mymodule.myinstance.mystat` or similar.
 *
 * \param name The name of this StatCounter
 */
StatCounter::StatCounter(std::string name)
  : name(stdsprintf("%s/%08x", name.c_str(), reinterpret_cast<unsigned int>(this))), count(0) {
	safe_init_static_mutex(StatCounter::mutex, true);
	MutexGuard<true> lock(StatCounter::mutex, true);
	if (!StatCounter::registry)
		StatCounter::registry = new std::map< std::string, StatCounter* >();
	StatCounter::registry->insert(std::make_pair(this->name, this));
}

StatCounter::~StatCounter() {
	MutexGuard<true> lock(StatCounter::mutex, true);
	StatCounter::registry->erase(this->name);
}


/**
 * Retrieve the current value of the counter.
 *
 * \note ISR safe.
 *
 * @return The current counter value.
 */
uint64_t StatCounter::get() const {
	CriticalGuard critical(true);
	return this->count;
}

/**
 * Retrieve the current value of the counter without a critical section.
 *
 * \note ISR safe.
 *
 * \note This value may occasionally be wildly incorrect as ARM32 has no atomic
 *       64 bit copy instruction.
 *
 * @return The current counter value.
 */
uint64_t StatCounter::fast_get() const {
	return this->count;
}

/**
 * Set the counter value and return the old value.
 *
 * \note ISR safe
 *
 * @param val The new counter value.
 * @return    The old counter value.
 */
uint64_t StatCounter::set(uint64_t val) {
	CriticalGuard critical(true);
	uint64_t ret = this->count;
	this->count = val;
	return ret;
}

/**
 * Increment the counter.
 *
 * \note ISR safe
 *
 * \note In case of overflow, the counter will be set to UINT64_MAX.
 *
 * @param inc The amount to increment by.
 * @return    The previous counter value.
 */
uint64_t StatCounter::increment(uint64_t inc) {
	CriticalGuard critical(true);
	uint64_t ret = this->count;
	if ((UINT64_MAX - inc) <= this->count)
		this->count = UINT64_MAX;
	else
		this->count += inc;
	return ret;
}

/**
 * Decrement the counter.
 *
 * \note ISR safe
 *
 * \note In case of underflow, the counter will be set to 0.
 *
 * @param dec The amount to decrement by.
 * @return    The previous counter value.
 */
uint64_t StatCounter::decrement(uint64_t dec) {
	CriticalGuard critical(true);
	uint64_t ret = this->count;
	if (dec > this->count)
		this->count = 0;
	else
		this->count -= dec;
	return ret;
}

/**
 * Set the counter value to the higher of the provided and current value.
 *
 * \note ISR safe
 *
 * @param val The value to set the counter to, if it is higher.
 * @return    The previous counter value.
 */
uint64_t StatCounter::high_water(uint64_t val) {
	CriticalGuard critical(true);
	uint64_t ret = this->count;
	if (this->count < val)
		this->count = val;
	return ret;
}

/**
 * Set the counter value to the lower of the provided and current value.
 *
 * \note ISR safe
 *
 * @param val The value to set the counter to, if it is higher.
 * @return    The previous counter value.
 */
uint64_t StatCounter::low_water(uint64_t val) {
	CriticalGuard critical(true);
	uint64_t ret = this->count;
	if (this->count > val)
		this->count = val;
	return ret;
}

/**
 * Retrieve the StatCounter registry (recursive) mutex, which must be held to
 * ensure that the pointers in a copy retrieved with StatCounter::get_registry()
 * remain valid.
 *
 * @return The StatCounter registry mutex
 */
SemaphoreHandle_t StatCounter::get_registry_mutex() {
	safe_init_static_mutex(StatCounter::mutex, true);
	return StatCounter::mutex;
}

/**
 * Retrieve a COPY of the global StatCounter registry usable for iteration, etc.
 *
 * \warning You must hold the StatCounter registry mutex to call this function
 *          or use its result.
 *
 * @return A copy of the global StatCounter registry
 */
std::map< std::string, StatCounter* > StatCounter::get_registry() {
	safe_init_static_mutex(StatCounter::mutex, true);
	configASSERT(xSemaphoreGetMutexHolder(StatCounter::mutex) == xTaskGetCurrentTaskHandle());
	MutexGuard<true> lock(StatCounter::mutex, true);
	if (!StatCounter::registry)
		StatCounter::registry = new std::map< std::string, StatCounter* >();
	return std::map< std::string, StatCounter *>(*StatCounter::registry);
}

// Static storage locations for StatCounter static variables.
SemaphoreHandle_t StatCounter::mutex;
std::map< std::string, StatCounter* > * volatile StatCounter::registry = NULL;

namespace {
/// A "stats" console command.
class ConsoleCommand_stats : public CommandParser::Command {
public:
	virtual std::string get_helptext(const std::string &command) const {
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

		if (!parameters.parse_parameters(1, true, &pattern))
			pattern = "*";

		bool exact = true;
		if (pattern.size() && pattern[pattern.size()-1] == '*') {
			pattern.pop_back();
			exact = false;
		}

		MutexGuard<true> lock(StatCounter::get_registry_mutex(), true);
		std::map<std::string, StatCounter*> registry = StatCounter::get_registry();
		for (auto &entry : registry) {
			if (exact && entry.first != pattern)
				continue;
			else if (entry.first.substr(0, pattern.size()) != pattern)
				continue;
			out += stdsprintf("%50s %10llu\n", entry.first.c_str(), entry.second->get());
		}
		console->write(out);
	}

	virtual std::vector<std::string> complete(const CommandParser::CommandParameters &parameters) const {
		if (parameters.cursor_parameter != 1)
			return std::vector<std::string>(); // Can't help you.

		std::vector<std::string> out;
		std::string pattern;

		parameters.parse_parameters(1, true, &pattern);

		MutexGuard<true> lock(StatCounter::get_registry_mutex(), true);
		std::map<std::string, StatCounter*> registry = StatCounter::get_registry();
		for (auto &entry : registry) {
			if (entry.first.substr(0, pattern.size()) != pattern)
				continue;
			out.push_back(entry.first);
		}
		return std::move(out);
	};
};
} // anonymous namespace


/**
 * Register console commands related to StatCounters
 */
void StatCounter::register_console_commands(CommandParser &parser, const std::string &prefix) {
	parser.register_command(prefix + "stats", std::make_shared<ConsoleCommand_stats>());
}

/**
 * Unregister console commands related to StatCounters.
 */
void StatCounter::deregister_console_commands(CommandParser &parser, const std::string &prefix) {
	parser.register_command(prefix + "stats", NULL);
}
