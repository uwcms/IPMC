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
class ScopedGuard {
public:
	/**
	 * Instantiate a ScopedGuard
	 *
	 * \warning You MUST keep an object in scope, the following code creates and
	 *          then immediately destroys a ScopedGuard, providing no protection:
	 *          `ScopedGuard(true);`
	 *          The following code creates a ScopedGuard and holds it, providing
	 *          the expected protection:
	 *          `ScopedGuard grd(true);`
	 *
	 * @param immediate If true, acquire the guard immediately, else wait for
	 *                  a call to .acquire()
	 */
	ScopedGuard(/* bool immediate */) : acquired(false) {
		/*
		if (immediate)
			this->acquire();

		 * ...is what I would like to say, but `this->mutex` or the like might
		 * not be instantiated in subclasses yet. We have to do this in each
		 * derived class.  Implementers take note!
		 */
	};
	virtual ~ScopedGuard() {
		/*
		if (this->acquired)
			this->release();

		 * ...is what I would like to say, but dynamic dispatch works
		 * differently in destructors, and it will call ScopedGuard's release. We
		 * have to do this in each derived class.  Implementers take note!
		 */
		if (this->acquired) {
			/*
			throw except::deadlock_error("The ScopedGuard subclass did not properly call release() in its destructor.");

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
	virtual void acquire() = 0;

	/**
	 * Release this guard.
	 *
	 * \note Implementers must set this->acquired appropriately.
	 */
	virtual void release() = 0;

	/**
	 * Determine whether the guard is acquired.
	 * @return true if the guard is acquired, else false
	 */
	virtual bool is_acquired() { return this->acquired; };
protected:
	bool acquired; ///< Indicate whether this ScopedGuard is acquired.
};

DEFINE_GENERIC_EXCEPTION(timeout_error, std::runtime_error)

/**
 * A ScopedGuard servicing FreeRTOS mutexes.
 */
template <bool RecursiveMutex> class MutexGuard : public ScopedGuard {
public:
	/**
	 * Instantiate a MutexGuard
	 *
	 * @param mutex Pre-initialized mutex that will be managed.
	 * @param immediate If true, acquire the mutex immediately.
	 * @param timeout The timeout for immediate acquisition (default 'forever')
	 * @throw except::timeout_error Immediate acquisition timed out.
	 */
	MutexGuard(const SemaphoreHandle_t &mutex, bool immediate, const TickType_t timeout = portMAX_DELAY)
		: mutex(mutex) {
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
	const SemaphoreHandle_t mutex; ///< The mutex managed.
};

/**
 * A ScopedGuard servicing FreeRTOS critical sections.
 */
class CriticalGuard : public ScopedGuard {
public:
	/**
	 * Lock the desired mutex.
	 * @param mutex Pre-initialized mutex that will be managed.
	 * @param immediate If true, acquire the mutex immediately.
	 */
	CriticalGuard(bool immediate) {
		if (immediate)
			this->acquire();
	};
	virtual ~CriticalGuard() {
		if (this->acquired)
			this->release();
	};

	virtual void acquire() {
		if (this->acquired)
			throw except::deadlock_error("Attempted to acquire() a CriticalGuard that was already held.");
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

/**
 * A ScopedGuard temporarily releasing another ScopedGuard.
 *
 * It will remember the is_acquired() of the guard it releases, in case it was
 * not in fact held when the ScopedGuardRelease was acquired.
 */
class ScopedGuardRelease : public ScopedGuard {
public:
	/**
	 * Release the provided guard.
	 * @param guard The ScopedGuard instance that will be managed.
	 * @param immediate If true, release the guard immediately.
	 */
	ScopedGuardRelease(ScopedGuard &guard, bool immediate) : guard(guard) {
		if (immediate)
			this->acquire();
	};
	virtual ~ScopedGuardRelease() {
		if (this->acquired)
			this->release();
	};

	virtual void acquire() {
		if (this->acquired)
			throw except::deadlock_error("Attempted to acquire() a ScopedGuardRelease that was already acquired.");
		this->guard_actually_held = this->guard.is_acquired();
		this->acquired = true;
		if (this->guard_actually_held)
			this->guard.release();
	}

	virtual void release() {
		if (!this->acquired)
			throw except::deadlock_error("Attempted to release() a CriticalGuard that was not held.");
		if (this->guard_actually_held)
			this->guard.acquire();
		this->acquired = false;
	}

protected:
	ScopedGuard &guard; ///< A ScopedGuard instance that we are temporarily releasing.
	bool guard_actually_held; ///< Track whether the guard we're releasing was held to begin with.
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
