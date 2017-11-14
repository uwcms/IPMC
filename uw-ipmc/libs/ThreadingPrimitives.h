#ifndef UW_IPMC_LIBS_THREADINGPRIMITIVES_H_
#define UW_IPMC_LIBS_THREADINGPRIMITIVES_H_

#include <FreeRTOS.h>
#include <semphr.h>
#include <list>
#include <libwrap.h>

/**
 * A class which allows for absolute timeout tracking in a wraparound-aware manner.
 *
 * \warning This class is not ISR safe and contains no internal locking.
 */
class AbsoluteTimeout: public GenericBase {
public:
	AbsoluteTimeout(TickType_t relative_timeout);
	virtual ~AbsoluteTimeout();
	TickType_t get_timeout();
protected:
	TickType_t last;      ///< Record the last checked time to allow wraparound calculations.
	TickType_t remaining; ///< Record the remaining timeout, to allow expiry calculations.
};

/**
 * A wait list allowing multiple tasks to block until signaled.
 *
 * \warning This object cannot be deleted while there are tasks waiting on it.
 *          Specifically this means that all tasks must have returned from
 *          #wait(), and no task may have an outstanding subscription to this
 *          waitlist.
 *
 * \warning This object cannot be destroyed from the FreeRTOS timer thread if
 *          there could be any outstanding #wakeFromISR(), as the destructor
 *          will sleep one or more ticks until they are complete, since it is
 *          unsafe to deallocate this object's memory while there are
 *          outstanding references, and it is not possible to cancel pended
 *          functions.
 */
class WaitList: public GenericBase {
public:
	WaitList();
	virtual ~WaitList();
	typedef SemaphoreHandle_t Subscription_t; ///< An opaque type representing a waitlist membership.
	Subscription_t join();
	bool wait(Subscription_t subscription, TickType_t timeout = portMAX_DELAY);
	void wake(s32 waiters=-1);
	void wakeFromISR(BaseType_t * const pxHigherPriorityTaskWoken, s32 waiters=-1);

    WaitList(WaitList const &) = delete;        ///< Class is not assignable.
    void operator=(WaitList const &x) = delete; ///< Class is not copyable.

    volatile int _interrupt_pend_count; ///< \protected An internal counter of outstanding deferred wakes FromISR, used for safe destruct assertion.
protected:
    SemaphoreHandle_t mutex; ///< A mutex protecting the internal state.
	std::list<SemaphoreHandle_t, FreeRTOS_Allocator<SemaphoreHandle_t>> waitlist; ///< A list of current waitlist subscriptions.
};

/**
 * A synchronization primitive modeled after python's threading.Event.
 *
 * This primitive consists of a flag that can be set, cleared or waited on.
 *
 * When this flag is true, wait() does not block.
 * When this flag is false, wait() blocks until it is true or timeout.
 *
 * This primitive allows multiple processes to block on one status.
 *
 * \warning This object cannot be deleted while there are tasks waiting on it.
 *
 * \warning This object cannot be destroyed from the FreeRTOS timer thread if
 *          there could be any outstanding setFromISR/clearFromISR, as the
 *          destructor will sleep one or more ticks until they are complete,
 *          since it is unsafe to deallocate this object's memory while there
 *          are outstanding references, and it is not possible to cancel pended
 *          functions.
 */
class Event: public GenericBase {
public:
	Event(bool initial_state = false);
	void set();
	void setFromISR(BaseType_t * const pxHigherPriorityTaskWoken);
	void clear();
	void clearFromISR(BaseType_t * const pxHigherPriorityTaskWoken);
	bool get();
	bool wait(TickType_t timeout = portMAX_DELAY);
	virtual ~Event();

    Event(Event const &) = delete;           ///< Class is not assignable.
    void operator=(Event const &x) = delete; ///< Class is not copyable.

	void _complete_setFromISR();   ///< \protected Internal.
	void _complete_clearFromISR(); ///< \protected Internal.
protected:
	SemaphoreHandle_t mutex; ///< A mutex to protect the flag.
	bool flag;               ///< The flag indicating the event's state.
	WaitList waitlist;       ///< A wait list for processes blocking on this event.
	volatile int interrupt_pend_count; ///< An internal counter of outstanding deferred sets/clears FromISR, used for safe destruct.
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

#endif /* UW_IPMC_LIBS_THREADINGPRIMITIVES_H_ */
