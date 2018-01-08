#ifndef UW_IPMC_LIBS_THREADINGPRIMITIVES_H_
#define UW_IPMC_LIBS_THREADINGPRIMITIVES_H_

#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>
#include <event_groups.h>
#include <list>
#include <libwrap.h>
#include <memory>

/**
 * A class which allows for absolute timeout tracking in a wraparound-aware manner.
 *
 * \warning This class is not ISR safe and contains no internal locking.
 */
class AbsoluteTimeout {
public:
	AbsoluteTimeout(TickType_t relative_timeout);
	AbsoluteTimeout(uint64_t relative_timeout);
	virtual ~AbsoluteTimeout() { };
	TickType_t get_timeout();
	uint64_t timeout64; ///< The 64bit timeout.
};

/**
 * A wait list allowing multiple tasks to block until signaled.
 */
class WaitList {
public:
	WaitList();
	virtual ~WaitList();

	/**
	 * This class represents a WaitList subscription.
	 *
	 * To wait on this WaitList, call wait().
	 *
	 * \note You do not need to call wait before destructing.
	 *
	 * \note This class is copiable.
	 */
	class Subscription {
	public:
		bool wait(TickType_t timeout = portMAX_DELAY);
		Subscription() : event(NULL) { };
	protected:
		std::shared_ptr<void> event; ///< The eventgroup this subscription is based on.
		/**
		 * Instantiate a subscription from a waitlist.
		 * @param event The event to wait on.
		 */
		Subscription(std::shared_ptr<void> event) : event(event) { };
		friend class WaitList;
	};
	friend class Subscription;

	Subscription join();
	void wake();

protected:
	SemaphoreHandle_t mutex; ///< A mutex protecting the eventgroup pointer.
	std::shared_ptr<void> event; ///< The eventgroup this WaitList is currently based on.
};

/**
 * Determine whether an interrupt is currently executing.
 *
 * This reads ICDABR[0-2] to determine if any interrupt is active.
 *
 * \return true if in an interrupt, else false
 */
static inline bool IN_INTERRUPT() {
	return (*(u32*)0xf8f01300 || *(u32*)0xf8f01304 || *(u32*)0xf8f01308);
}

/**
 * Determine whether we are in a FreeRTOS portENTER_CRITICAL() critical section.
 *
 * \return true if in a critical section, else false
 */
static inline bool IN_CRITICAL() {
	extern volatile uint32_t ulCriticalNesting;
	return !!ulCriticalNesting;
}

static inline uint64_t get_tick64() {
	if (!IN_INTERRUPT())
		taskENTER_CRITICAL();
	extern volatile uint64_t uwipmc_tick64_count;
	uint64_t ret = uwipmc_tick64_count;
	if (!IN_INTERRUPT())
		taskEXIT_CRITICAL();
	return ret;
}

#endif /* UW_IPMC_LIBS_THREADINGPRIMITIVES_H_ */
