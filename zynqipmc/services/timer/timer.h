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

#ifndef SRC_COMMON_ZYNQIPMC_SERVICES_TIMER_TIMER_H_
#define SRC_COMMON_ZYNQIPMC_SERVICES_TIMER_TIMER_H_

#include <functional>
#include <list>
#include <libs/threading.h>

/**
 * Utility service that can launch timers.
 */
class TimerService final {
public:
	TimerService(BaseType_t input_queue_length=8);
	virtual ~TimerService();

	class Timer final {
	public:
		/**
		 * Create a new timer within the timer service.
		 * @param func The function/lambda that will get executed when the timer triggers.
		 * @param when Whent he callback should be executed, check AbsoluteTimeout.
		 * @param rearm_every When the timer should rearm itself, default is zero (never).
		 */
		Timer(std::function<void(void)> func, AbsoluteTimeout when, uint64_t rearm_every = 0) :
			next(when), kRearmEvery(rearm_every), func(func), cancelled(false) {
			this->mutex = xSemaphoreCreateRecursiveMutex();
		};
		~Timer() { this->cancel(); vSemaphoreDelete(this->mutex); };

		//!  Run the timer, if and only if it has not been cancelled yet.
		void run();

		/**
		 * Cancel the timer.
		 *
		 * @param synchronous wait for cancellation (and active run completion)
		 */
		void cancel(bool synchronous = true);

		//! Check if the timer was canceled.
		bool isCancelled() const { return this->cancelled; };

		friend class TimerService;

	private:
		AbsoluteTimeout next;					///< The next time the timer will trigger
		const uint64_t kRearmEvery;				///< If nonzero, rearm the timer for +rearm_every ticks from next trigger.
		SemaphoreHandle_t mutex;				///< A mutex protecting run, ensuring no races within cancellation.
		const std::function<void(void)> func;	///< Function to call on trigger.
		bool cancelled;							///< true if this timer is cancelled and should not trigger
	};

	/**
	 * Submit a Timer to the service.
	 *
	 * @param timer The timer to submit.
	 */
	void submit(std::shared_ptr<Timer> &timer);

	/**
	 * Start the timer thread.
	 *
	 * @param thread_name The name for the timer thread.
	 * @param thread_priority The priority for the timer thread.
	 * @param stack_words The stack size, passed directly to UWThreadCreate.
	 */
	void start(const std::string &thread_name, BaseType_t thread_priority, BaseType_t stack_words = 0);

	/**
	 * Retrieve a global instance of a TimerService at the specified thread priority.
	 *
	 * @param process_priority The thread priority for which the timer service is desired (0 = caller priority).
	 * @return The global timer thread for the specified priority.
	 * @throw std::logic_error if invalid priority.
	 */
	static TimerService& globalTimer(BaseType_t process_priority = 0);

protected:
	/**
	 * An input queue for new timers to be registered.
	 *
	 * This queue transfers pointers to new'd shared pointers to `Timer`s.
	 */
	QueueHandle_t input_queue;
	SemaphoreHandle_t mutex; ///< A mutex guarding the internal data structures.
	std::list< std::shared_ptr<Timer> > timers; ///< The actual timers.

	void runThread();

	static TimerService *global_timers[configMAX_PRIORITIES]; ///< Global timers.
};

#endif /* SRC_COMMON_ZYNQIPMC_SERVICES_TIMER_TIMER_H_ */
