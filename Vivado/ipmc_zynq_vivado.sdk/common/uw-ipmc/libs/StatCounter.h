/*
 * StatCounter.h
 *
 *  Created on: Dec 5, 2017
 *      Author: jtikalsky
 */

#ifndef SRC_COMMON_UW_IPMC_LIBS_STATCOUNTER_H_
#define SRC_COMMON_UW_IPMC_LIBS_STATCOUNTER_H_

#include <FreeRTOS.h>
#include <semphr.h>
#include <string>
#include <map>

/**
 * A class representing a statistic/event counter.
 *
 * These counters are automatically registered in a global registry under their
 * supplied names, to allow for easy access from diagnostic systems (not yet
 * implemented).
 */
class StatCounter {
public:
	const std::string name; ///< The name of the StatCounter.
	StatCounter(std::string name);
	virtual ~StatCounter();

	uint64_t get();
	uint64_t fast_get();
	uint64_t set(uint64_t val);
	uint64_t increment(uint64_t inc=1);
	uint64_t decrement(uint64_t dec=1);
	uint64_t high_water(uint64_t val);
	uint64_t low_water(uint64_t val);

    StatCounter(StatCounter const &) = delete;     ///< Class is not assignable.
    void operator=(StatCounter const &x) = delete; ///< Class is not copyable.

protected:
    volatile uint64_t count; ///< The count counted.

	static SemaphoreHandle_t mutex; ///< A mutex to protect the registry.
	static std::map< std::string, StatCounter* > * volatile registry; ///< A global registry of Stat Counters.
};

#endif /* SRC_COMMON_UW_IPMC_LIBS_STATCOUNTER_H_ */
