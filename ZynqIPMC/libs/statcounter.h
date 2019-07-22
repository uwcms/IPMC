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

#ifndef SRC_COMMON_ZYNQIPMC_LIBS_STATCOUNTER_H_
#define SRC_COMMON_ZYNQIPMC_LIBS_STATCOUNTER_H_

#include <FreeRTOS.h>
#include <semphr.h>
#include <string>
#include <map>
#include <services/console/CommandParser.h>

/**
 * A class representing a statistic/event counter.
 *
 * These counters are automatically registered in a global registry under their
 * supplied names, to allow for easy access from diagnostic systems (not yet
 * implemented).
 */
class StatCounter final {
public:
	/**
	 * Instantiate a new stat counter.  It will automatically be registered in the
	 * global StatCounter registry.
	 *
	 * @note StatCounter names should be in a reverse dotted format:
	 *       `mymodule.myinstance.mystat` or similar.
	 *
	 * @param name The name of this StatCounter
	 */
	StatCounter(std::string name);
	~StatCounter();

	StatCounter(StatCounter const &) = delete;     ///< Class is not assignable.
	void operator=(StatCounter const &x) = delete; ///< Class is not copyable.

	/**
	 * Retrieve the current value of the counter.
	 *
	 * @note ISR safe.
	 *
	 * @return The current counter value.
	 */
	uint64_t get() const;

	/**
	 * Retrieve the current value of the counter without a critical section.
	 *
	 * @note ISR safe.
	 *
	 * @note This value may occasionally be wildly incorrect as ARM32 has no atomic
	 *       64 bit copy instruction.
	 *
	 * @return The current counter value.
	 */
	uint64_t fastGet() const;

	/**
	 * Set the counter value and return the old value.
	 *
	 * @note ISR safe.
	 *
	 * @param val The new counter value.
	 * @return    The old counter value.
	 */
	uint64_t set(uint64_t val);

	/**
	 * Increment the counter.
	 *
	 * @note ISR safe.
	 *
	 * @note In case of overflow, the counter will be set to UINT64_MAX.
	 *
	 * @param inc The amount to increment by.
	 * @return    The previous counter value.
	 */
	uint64_t increment(uint64_t inc=1);

	/**
	 * Decrement the counter.
	 *
	 * @note ISR safe.
	 *
	 * @note In case of underflow, the counter will be set to 0.
	 *
	 * @param dec The amount to decrement by.
	 * @return    The previous counter value.
	 */
	uint64_t decrement(uint64_t dec=1);

	/**
	 * Set the counter value to the higher of the provided and current value.
	 *
	 * @note ISR safe.
	 *
	 * @param val The value to set the counter to, if it is higher.
	 * @return    The previous counter value.
	 */
	uint64_t highWater(uint64_t val);

	/**
	 * Set the counter value to the lower of the provided and current value.
	 *
	 * @note ISR safe.
	 *
	 * @param val The value to set the counter to, if it is higher.
	 * @return    The previous counter value.
	 */
	uint64_t lowWater(uint64_t val);

    /**
     * Retrieve the StatCounter registry (recursive) mutex, which must be held to
     * ensure that the pointers in a copy retrieved with StatCounter::get_registry()
     * remain valid.
     *
     * @return The StatCounter registry mutex.
     */
    static SemaphoreHandle_t getRegistryMutex();

    /**
     * Retrieve a COPY of the global StatCounter registry usable for iteration, etc.
     *
     * @warning You must hold the StatCounter registry mutex to call this function
     *          or use its result.
     *
     * @return A copy of the global StatCounter registry.
     */
    static std::map< std::string, StatCounter* > getRegistry();

	static void registerConsoleCommands(CommandParser &parser, const std::string &prefix="");
	static void deregisterConsoleCommands(CommandParser &parser, const std::string &prefix="");

private:
	class Stats; ///< Console command to check all StatCounter objects.

	const std::string kName; ///< The name of the StatCounter.
    volatile uint64_t count; ///< The count counted.
	static SemaphoreHandle_t mutex; ///< A mutex to protect the registry.
	static std::map< std::string, StatCounter* > * volatile registry; ///< A global registry of Stat Counters.
};

#endif /* SRC_COMMON_ZYNQIPMC_LIBS_STATCOUNTER_H_ */
