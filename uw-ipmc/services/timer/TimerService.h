/*
 * TimerService.h
 *
 *  Created on: Jan 11, 2019
 *      Author: jtikalsky
 */

#ifndef SRC_COMMON_UW_IPMC_SERVICES_TIMER_TIMERSERVICE_H_
#define SRC_COMMON_UW_IPMC_SERVICES_TIMER_TIMERSERVICE_H_

#include <functional>
#include <list>
#include <libs/ThreadingPrimitives.h>
#include <FreeRTOS.h>

class TimerService {
public:
	TimerService(BaseType_t input_queue_length=8);
	virtual ~TimerService();

	class Timer final {
	public:
		Timer(std::function<void(void)> func, AbsoluteTimeout when, uint64_t rearm_every=0) :
			func(func), next(when), rearm_every(rearm_every), cancelled(false) { };
		const std::function<void(void)> func; ///< Function to call on trigger
		AbsoluteTimeout next; ///< The next time the timer will trigger
		uint64_t rearm_every; ///< If nonzero, rearm the timer for +rearm_every ticks from next trigger.
		bool cancelled; ///< true if this timer is cancelled and should not trigger
	};
	void submit(std::shared_ptr<Timer> &timer);
	void start(const std::string &thread_name, BaseType_t thread_priority, BaseType_t stack_words=0);

	static TimerService& global_timer(BaseType_t process_priority=0);
protected:
	/**
	 * An input queue for new timers to be registered.
	 *
	 * This queue transfers pointers to new'd shared pointers to `Timer`s.
	 */
	QueueHandle_t input_queue;
	SemaphoreHandle_t mutex; ///< A mutex guarding the internal data structures.
	std::list< std::shared_ptr<Timer> > timers; ///< The actual timers.
	void run_thread();
	static TimerService *global_timers[configMAX_PRIORITIES]; ///< Global timers
};

#endif /* SRC_COMMON_UW_IPMC_SERVICES_TIMER_TIMERSERVICE_H_ */
