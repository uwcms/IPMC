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
#include <FreeRTOS.h>
#include <semphr.h>
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
	safe_init_static_mutex(StatCounter::mutex, false);
	MutexGuard<false> lock(StatCounter::mutex, true);
	if (!StatCounter::registry)
		StatCounter::registry = new std::map< std::string, StatCounter* >();
	StatCounter::registry->insert(std::make_pair(this->name, this));
}

StatCounter::~StatCounter() {
	MutexGuard<false> lock(StatCounter::mutex, true);
	StatCounter::registry->erase(this->name);
}


/**
 * Retrieve the current value of the counter.
 *
 * \note ISR safe.
 *
 * @return The current counter value.
 */
uint64_t StatCounter::get() {
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
uint64_t StatCounter::fast_get() {
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

// Static storage locations for StatCounter static variables.
SemaphoreHandle_t StatCounter::mutex;
std::map< std::string, StatCounter* > * volatile StatCounter::registry = NULL;
