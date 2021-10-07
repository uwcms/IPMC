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

/**
 * This library contains several useful classes and functions
 * to use in tasks and other thread related code.
 */

#ifndef SRC_COMMON_ZYNQIPMC_LIBS_THREADING_H_
#define SRC_COMMON_ZYNQIPMC_LIBS_THREADING_H_

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
#include <libs/backtrace/backtrace.h>

// Declare a few custom task related exceptions:
DEFINE_GENERIC_EXCEPTION(deadlock_error, std::logic_error)
DEFINE_GENERIC_EXCEPTION(thread_create_error, std::runtime_error)

/**
 * A class which allows for absolute timeout tracking in a wraparound-aware manner.
 *
 * @warning This class is not ISR safe and contains no internal locking.
 */
class AbsoluteTimeout {
public:
	/**
	 * Set the timeout based on a relative time from now.
	 *
	 * @param relative_timeout The relative timeout to be converted to absolute form.
	 */
	AbsoluteTimeout(TickType_t relative_timeout) { this->setTimeout(relative_timeout); };
	AbsoluteTimeout(uint64_t relative_timeout) { this->setTimeout(relative_timeout); };
	virtual ~AbsoluteTimeout() { };

	/**
	 * Get the current remaining timeout.
	 *
	 * @note If you have specified a timeout that is longer than `TickType_t-1`, you
	 *       may have to block multiple times before the true timeout is expired.
	 *
	 * @return A correct relative timeout for this object from the current point in time.
	 */
	TickType_t getTimeout() const;
	inline uint64_t getTimeout64() const { return this->timeout64; };

	/**
	 * Set or reset the timeout based on a relative time from now.
	 * @param relative_timeout The relative timeout to be converted to absolute form.
	 */
	void setTimeout(TickType_t relative_timeout);
	void setTimeout(uint64_t relative_timeout);
	inline void setAbsTimeout(uint64_t abs_timeout) { this->timeout64 = abs_timeout; };
	inline void setAbsTimeout(const AbsoluteTimeout &abs_timeout) { this->timeout64 = abs_timeout.timeout64; };

	/// Comparison Operators
	///@{
	bool operator==(const AbsoluteTimeout &b) const { return this->timeout64 == b.timeout64; };
	bool operator!=(const AbsoluteTimeout &b) const { return this->timeout64 != b.timeout64; };
	bool operator<(const AbsoluteTimeout &b)  const { return this->timeout64 <  b.timeout64; };
	bool operator<=(const AbsoluteTimeout &b) const { return this->timeout64 <= b.timeout64; };
	bool operator>(const AbsoluteTimeout &b)  const { return this->timeout64 >  b.timeout64; };
	bool operator>=(const AbsoluteTimeout &b) const { return this->timeout64 >= b.timeout64; };
	///@}

protected:
	uint64_t timeout64; ///< The 64bit timeout.
};

/**
 * A wait list allowing multiple tasks to block until signaled.
 *
 * \tparam REARMING If true, the `.wake()` only affects previous `.join()`s.
 *                  If false, the first `.wake()` affects all subsequent `.join()`s.
 */
template <bool REARMING> class WaitList {
public:
	WaitList();
	virtual ~WaitList();

	/**
	 * This class represents a WaitList subscription.
	 *
	 * To wait on this WaitList, call wait().
	 *
	 * @note You do not need to call wait before destructing.
	 *
	 * @note This class is copiable.
	 */
	class Subscription {
	public:
		Subscription() : event(nullptr) { };

		/**
		 * Wait using this WaitList subscription.
		 *
		 * @param timeout How long to wait.
		 * @return true if complete, false if timeout.
		 *
		 * @note This will not refresh the subscription.
		 */
		bool wait(TickType_t timeout = portMAX_DELAY);

		friend class WaitList;

	protected:
		std::shared_ptr<void> event; ///< The eventgroup this subscription is based on.

		/**
		 * Instantiate a subscription from a waitlist.
		 * @param event The event to wait on.
		 */
		Subscription(std::shared_ptr<void> event) : event(event) { };
	};

	/**
	 * Join this waitlist.
	 *
	 * In order to wait on a waitlist, you must first join it.  The separation of
	 * these two operations allows one to join a waitlist while holding various
	 * relevant mutexes, but release those mutexes before waiting.
	 *
	 * @return A WaitList subscription which may be used to wait on this WaitList.
	 */
	Subscription join();

	/**
	 * Wake all threads waiting on this WaitList.
	 *
	 * @note This function is ISR safe.
	 */
	void wake();

protected:
	SemaphoreHandle_t mutex; ///< A mutex protecting the eventgroup pointer.
	std::shared_ptr<void> event; ///< The eventgroup this WaitList is currently based on.
};

/**
 * An Abstract Base Class for scope guard utilities, such as MutexLock, and
 * CriticalSection.
 */
class ScopedGuard {
public:
	/**
	 * Instantiate a ScopedGuard
	 *
	 * @warning You MUST keep an object in scope, the following code creates and
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
	 * @note Implementers must set this->acquired appropriately.
	 */
	virtual void acquire() = 0;

	/**
	 * Release this guard.
	 *
	 * @note Implementers must set this->acquired appropriately.
	 */
	virtual void release() = 0;

	/**
	 * Determine whether the guard is acquired.
	 * @return true if the guard is acquired, else false.
	 */
	virtual bool isAcquired() { return this->acquired; };

protected:
	bool acquired; ///< Indicate whether this ScopedGuard is acquired.
};

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
	MutexGuard(const SemaphoreHandle_t &mutex, bool immediate = true, const TickType_t timeout = portMAX_DELAY)
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
		if (RecursiveMutex) {
			ret = xSemaphoreTakeRecursive(this->mutex, timeout);
		} else {
			ret = xSemaphoreTake(this->mutex, timeout);
		}

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
		if (RecursiveMutex) {
			ret = xSemaphoreGiveRecursive(this->mutex);
		} else {
			ret = xSemaphoreGive(this->mutex);
		}

		if (ret != pdTRUE)
			throw except::deadlock_error("xSemaphoreGive*() did not return pdTRUE when releasing MutexGuard.");
	}

protected:
	const SemaphoreHandle_t mutex; ///< The mutex managed.
};

// TODO: Revise where this gets used

/**
 * Determine whether an interrupt is currently executing.
 *
 * This reads ICDABR[0-2] to determine if any interrupt is active.
 *
 * @return true if in an interrupt, else false
 */
static inline bool IN_INTERRUPT() {
	return (*(uint32_t*)0xf8f01300 || *(uint32_t*)0xf8f01304 || *(uint32_t*)0xf8f01308);
}

/**
 * Determine whether we are in a FreeRTOS portENTER_CRITICAL() critical section.
 *
 * @return true if in a critical section, else false
 */
static inline bool IN_CRITICAL() {
	extern volatile uint32_t ulCriticalNesting;
	return !!ulCriticalNesting;
}

/**
 * A ScopedGuard servicing FreeRTOS critical sections.
 *
 * @note This guard is ISR safe (it performs no heap allocation and will act as
 *       a NOOP when in an interrupt).
 */
class CriticalGuard : public ScopedGuard {
public:
	/**
	 * Enter a critical section
	 * @param immediate If true, enter the critical section immediately.
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

		if (!IN_INTERRUPT()) portENTER_CRITICAL();

		this->acquired = true;
	}

	virtual void release() {
		if (!this->acquired)
			throw except::deadlock_error("Attempted to release() a CriticalGuard that was not held.");

		this->acquired = false;

		if (!IN_INTERRUPT()) portEXIT_CRITICAL();
	}
};

/**
 * A ScopedGuard servicing FreeRTOS vTaskSuspendAll.
 *
 * @note This guard is ISR safe (it performs no heap allocation and will act as
 *       a NOOP when in an interrupt).
 */
class SuspendGuard : public ScopedGuard {
public:
	/**
	 * Suspend the scheduler.
	 * @param immediate If true, suspend the scheduler immediately.
	 */
	SuspendGuard(bool immediate) {
		if (immediate)
			this->acquire();
	};
	virtual ~SuspendGuard() {
		if (this->acquired)
			this->release();
	};

	virtual void acquire() {
		if (this->acquired)

			throw except::deadlock_error("Attempted to acquire() a SuspendGuard that was already held.");
		if (!IN_INTERRUPT())
			vTaskSuspendAll();

		this->acquired = true;
	}

	virtual void release() {
		if (!this->acquired)
			throw except::deadlock_error("Attempted to release() a SuspendGuard that was not held.");

		this->acquired = false;

		if (!IN_INTERRUPT())
			xTaskResumeAll();
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
		if (this->acquired) {
			throw except::deadlock_error("Attempted to acquire() a ScopedGuardRelease that was already acquired.");
		}

		this->guard_actually_held = this->guard.isAcquired();
		this->acquired = true;

		if (this->guard_actually_held) {
			this->guard.release();
		}
	}

	virtual void release() {
		if (!this->acquired) {
			throw except::deadlock_error("Attempted to release() a CriticalGuard that was not held.");
		}

		if (this->guard_actually_held) {
			this->guard.acquire();
		}

		this->acquired = false;
	}

protected:
	ScopedGuard &guard; ///< A ScopedGuard instance that we are temporarily releasing.
	bool guard_actually_held; ///< Track whether the guard we're releasing was held to begin with.
};

// TODO: Is this really necessary??
/**
 * Ensure the provided mutex is initialized and ready for use.
 *
 * @note Not ISR safe (why are you calling it from an ISR?).
 *
 * @param mutex The mutex to ensure initialization of.
 * @param recursive true if the mutex should be recursive else false.
 * @param memory The memory block to allocate the semaphore in, or nullptr for heap.
 */
static inline void safe_init_static_mutex(volatile SemaphoreHandle_t &mutex, bool recursive, StaticSemaphore_t *memory=nullptr) {
	// Fast check.  False negatives possible, but not false positives.
	if (mutex)
		return;

	CriticalGuard critical(true);
	// Slow check.  Clear up prior false negatives.
	if (mutex)
		return;

	if (memory) {
		if (recursive)
			mutex = xSemaphoreCreateRecursiveMutexStatic(memory);
		else
			mutex = xSemaphoreCreateMutexStatic(memory);
	}
	else {
		if (recursive)
			mutex = xSemaphoreCreateRecursiveMutex();
		else
			mutex = xSemaphoreCreateMutex();
	}
}

static inline uint64_t get_tick64() {
	CriticalGuard critical(true);
	extern volatile uint64_t uwipmc_tick64_count;
	return uwipmc_tick64_count;
}

/**
 * Converts a trace into a string.
 * @param trace Trace to be converted
 * @param exception If this is an exception it can be provided here.
 * @param location_description Where it happened, like the task name.
 * @return String with the trace.
 */
std::string renderExceptionReport(const BackTrace *trace, const std::exception *exception, std::string location_description);

/**
 * Runs the supplied std::function in a new thread, cleaning up the thread when it
 * returns.
 *
 * @param name The name of the thread.
 * @param priority The task priority.
 * @param stack_words The number of words used for the task stack.
 * @param thread_func The function to run, C function, lambda, bind, method, all are welcome.
 * @return The handler of the created task.
 */
TaskHandle_t runTask(const std::string& name, BaseType_t priority, std::function<void(void)> thread_func, BaseType_t stack_words=0);

#endif /* SRC_COMMON_ZYNQIPMC_LIBS_THREADING_H_ */
