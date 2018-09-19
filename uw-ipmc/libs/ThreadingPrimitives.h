#ifndef UW_IPMC_LIBS_THREADINGPRIMITIVES_H_
#define UW_IPMC_LIBS_THREADINGPRIMITIVES_H_

#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>
#include <event_groups.h>
#include <list>
#include <memory>
#include <functional>

/**
 * Given a pre-existing mutex, the MutexLock class automatically locks when created and releases it
 * when scope is lost.
 */
class MutexLock {
public:
	/**
	 * Lock the desired mutex.
	 * @param m Pre-initialized mutex that will be locked.
	 * @param t Timeout if required, default is portMAX_DELAY, which will wait forever.
	 * @note If a timeout is defined then isLocked should be used after construction.
	 */
	inline MutexLock(SemaphoreHandle_t &m, const TickType_t t = portMAX_DELAY) : mutex(m) { this->locked = xSemaphoreTake(this->mutex, t); };
	inline ~MutexLock() { if (this->locked) xSemaphoreGive(this->mutex); };
	///! Returns true if the mutex successfully locked, only required when timeout is set.
	inline bool isLocked() { return locked; };
private:
	SemaphoreHandle_t mutex;
	bool locked;
};

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
	/// Comparison Operators
	///@{
	bool operator==(const AbsoluteTimeout &b) const { return this->timeout64 == b.timeout64; };
	bool operator!=(const AbsoluteTimeout &b) const { return this->timeout64 != b.timeout64; };
	bool operator<(const AbsoluteTimeout &b)  const { return this->timeout64 <  b.timeout64; };
	bool operator<=(const AbsoluteTimeout &b) const { return this->timeout64 <= b.timeout64; };
	bool operator>(const AbsoluteTimeout &b)  const { return this->timeout64 >  b.timeout64; };
	bool operator>=(const AbsoluteTimeout &b) const { return this->timeout64 >= b.timeout64; };
	///@}
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

void *trampoline_prepare(std::function<void(void)> cb);
extern "C" {
	void trampoline_launch_pv(void *voidstar);
	void trampoline_launch_pv_x(void *voidstar, BaseType_t ignored);
	void trampoline_multilaunch_pv(void *voidstar);
	void trampoline_multilaunch_pv_x(void *voidstar, BaseType_t ignored);
	void trampoline_cancel(void *voidstar);
}

TaskHandle_t UWTaskCreate(const std::string name, BaseType_t priority, std::function<void(void)> thread_func, BaseType_t stack_words=0);

#endif /* UW_IPMC_LIBS_THREADINGPRIMITIVES_H_ */
