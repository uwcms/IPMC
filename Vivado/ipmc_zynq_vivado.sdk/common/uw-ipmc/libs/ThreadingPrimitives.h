#ifndef UW_IPMC_LIBS_THREADINGPRIMITIVES_H_
#define UW_IPMC_LIBS_THREADINGPRIMITIVES_H_

#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>
#include <event_groups.h>
#include <list>
#include <memory>
#include <functional>
#include <string>
#include <exception>
#include <libs/except.h>

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

DEFINE_GENERIC_EXCEPTION(deadlock_error, std::logic_error)

/**
 * An Abstract Base Class for scope guard utilities, such as MutexLock, and
 * CriticalSection.
 */
class ScopeGuard {
public:
	/**
	 * Instantiate a ScopeGuard
	 *
	 * \warning You MUST keep an object in scope, the following code creates and
	 *          then immediately destroys a ScopeGuard, providing no protection:
	 *          `ScopeGuard(true);`
	 *          The following code creates a ScopeGuard and holds it, providing
	 *          the expected protection:
	 *          `ScopeGuard grd(true);`
	 *
	 * @param immediate If true, acquire the guard immediately, else wait for
	 *                  a call to .acquire()
	 */
	ScopeGuard(bool immediate) : acquired(false) {
		if (immediate)
			this->acquire();
	};
	virtual ~ScopeGuard() {
		/*
		if (this->acquired)
			this->release();

		 * ...is what I would like to say, but dynamic dispatch works
		 * differently in destructors, and it will call ScopeGuard's release. We
		 * have to do this in each derived class.  Implementers take note!
		 */
		if (this->acquired) {
			/*
			throw except::deadlock_error("The ScopeGuard subclass did not properly call release() in its destructor.");

			 * ...is what I would like to say, but I can't throw in a destructor!
			 * I'll settle for calling abort.
			 */
			abort();
		}
	};

	/**
	 * Acquire this guard.
	 *
	 * \note Implementers must set this->acquired appropriately.
	 */
	virtual void acquire() {
		this->acquired = true;
	}

	/**
	 * Release this guard.
	 *
	 * \note Implementers must set this->acquired appropriately.
	 */
	virtual void release() {
		if (!this->acquired)
			throw except::deadlock_error("Attempted to release a ScopedGuard that is not held.");
		this->acquired = false;
	}

	/**
	 * Determine whether the guard is acquired.
	 * @return true if the guard is acquired, else false
	 */
	virtual bool is_acquired() { return this->acquired; };
protected:
	bool acquired; ///< Indicate whether this ScopeGuard is acquired.
};

DEFINE_GENERIC_EXCEPTION(timeout_error, std::runtime_error)

/**
 * A ScopeGuard servicing FreeRTOS mutexes.
 */
template <bool RecursiveMutex> class MutexGuard : public ScopeGuard {
public:
	/**
	 * Instantiate a MutexGuard
	 *
	 * @param mutex Pre-initialized mutex that will be managed.
	 * @param immediate If true, acquire the mutex immediately.
	 * @param timeout The timeout for immediate acquisition (default 'forever')
	 * @throw except::timeout_error Immediate acquisition timed out.
	 */
	MutexGuard(SemaphoreHandle_t &mutex, bool immediate, const TickType_t timeout = portMAX_DELAY)
		: ScopeGuard(false /* We'll handle acquisition here. */), mutex(mutex) {
		if (immediate)
			this->acquire(timeout);
	};
	virtual ~MutexGuard() {
		if (this->acquired)
			this->release();
	};

	virtual void acquire() {
		this->acquire(portMAX_DELAY);
	}
	/**
	 * Acquire the mutex.
	 * @timeout A timeout for mutex acquisition.
	 * @throw except::timeout_error Acquisition timed out.
	 * @throw except::deadlock_error The MutexGuard was already acquired.
	 */
	virtual void acquire(TickType_t timeout) {
		if (this->acquired)
			throw except::deadlock_error("Attempted to acquire() a MutexGuard that is already held.");
		BaseType_t ret;
		if (RecursiveMutex)
			ret = xSemaphoreTakeRecursive(this->mutex, timeout);
		else
			ret = xSemaphoreTake(this->mutex, timeout);
		if (ret != pdTRUE)
			throw except::timeout_error("Unable to acquire MutexLock in the specified period.");
		this->acquired = true;
	}

	/**
	 * Release the mutex.
	 * @throw except::deadlock_error The MutexGuard was improperly released or release failed.
	 */
	virtual void release() {
		if (!this->acquired)
			throw except::deadlock_error("Attempted to release() a MutexGuard that was not held.");
		this->acquired = false;
		BaseType_t ret;
		if (RecursiveMutex)
			ret = xSemaphoreGiveRecursive(this->mutex);
		else
			ret = xSemaphoreGive(this->mutex);
		if (ret != pdTRUE)
			throw except::deadlock_error("xSemaphoreGive*() did not return pdTRUE when releasing MutexGuard.");
	}
protected:
	SemaphoreHandle_t mutex; ///< The mutex managed.
};

/**
 * A ScopeGuard servicing FreeRTOS critical sections.
 */
class CriticalGuard : public ScopeGuard {
public:
	/**
	 * Lock the desired mutex.
	 * @param mutex Pre-initialized mutex that will be managed.
	 * @param immediate If true, acquire the mutex immediately.
	 */
	CriticalGuard(bool immediate) : ScopeGuard(immediate) { };
	virtual ~CriticalGuard() {
		if (this->acquired)
			this->release();
	};

	virtual void acquire() {
		if (!IN_INTERRUPT())
			portENTER_CRITICAL();
		this->acquired = true;
	}

	virtual void release() {
		if (!this->acquired)
			throw except::deadlock_error("Attempted to release() a CriticalGuard that was not held.");
		this->acquired = false;
		if (!IN_INTERRUPT())
			portEXIT_CRITICAL();
	}
};

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

DEFINE_GENERIC_EXCEPTION(thread_create_error, std::runtime_error)

class BackTrace;
std::string render_exception_report(const BackTrace *trace, const std::exception *exception, std::string location_description);
TaskHandle_t UWTaskCreate(const std::string name, BaseType_t priority, std::function<void(void)> thread_func, BaseType_t stack_words=0);

#endif /* UW_IPMC_LIBS_THREADINGPRIMITIVES_H_ */
