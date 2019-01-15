/*
 * TimerService.cpp
 *
 *  Created on: Jan 11, 2019
 *      Author: jtikalsky
 */

#include <services/timer/TimerService.h>
#include <libs/ThreadingPrimitives.h>
#include <FreeRTOS.h>
#include <semphr.h>

/**
 *
 * @param input_queue_length
 */
TimerService::TimerService(BaseType_t input_queue_length) {
	configASSERT(this->mutex = xSemaphoreCreateMutex());
	configASSERT(this->input_queue = xQueueCreate(input_queue_length, sizeof(std::shared_ptr<Timer>*)));
}

TimerService::~TimerService() {
	configASSERT(0); // We don't have a mechanism to stop the thread.
	vQueueDelete(this->input_queue);
	vSemaphoreDelete(this->mutex);
}

/**
 * Submit a Timer to the service.
 *
 * @param timer The timer to submit
 */
void TimerService::submit(std::shared_ptr<Timer> &timer) {
	std::shared_ptr<Timer> *qtmr = new std::shared_ptr<Timer>(timer);
	xQueueSend(this->input_queue, qtmr, portMAX_DELAY);
}

/**
 * Start the timer thread.
 *
 * @param thread_name The name for the timer thread
 * @param thread_priority The priority for the timer thread
 * @param stack_words The stack size, passed directly to UWThreadCreate
 */
void TimerService::start(const std::string &thread_name, BaseType_t thread_priority, BaseType_t stack_words) {
	UWTaskCreate(thread_name, thread_priority, [this]()->void{this->run_thread();}, stack_words);
}

void TimerService::run_thread() {
	AbsoluteTimeout next(portMAX_DELAY); // The next timeout
	while (true) {
		// Wait once, then add any new timers to the set.
		TickType_t timeout = next.get_timeout();
		while (true) {
			std::shared_ptr<Timer> *tptr = NULL;
			if (pdTRUE == xQueueReceive(this->input_queue, &tptr, timeout)) {
				MutexGuard<false> lock(this->mutex, true);
				this->timers.push_back(*tptr);
				delete tptr;
			}
			else {
				break; // Nothing more to receive in this pass.
			}
			timeout = 0; // Don't wait again.
		}

		// Iterate and call & rearm relevant timers.
		MutexGuard<false> lock(this->mutex, true);
		uint64_t now = get_tick64();
		for (auto it = this->timers.begin(), eit = this->timers.end(); it != eit; ) {
			std::shared_ptr<Timer> &timer = *it;
			if (timer->cancelled) {
				// Delete cancelled timers.
				it = this->timers.erase(it);
				continue;
			}
			if (timer->next.timeout64 <= now) {
				lock.release();
				timer->func();
				lock.acquire();
				if (timer->rearm_every) {
					// This timer auto-rearms.
					// We adjust the existing absolute timeout to avoid drift.
					timer->next.timeout64 += timer->rearm_every;
				}
				else {
					// This timer has fired and is finished.
					it = this->timers.erase(it);
					continue;
				}
			}
			next.timeout64 = UINT64_MAX;
			if (timer->next < next)
				next = timer->next;
			it++;
		}
	}
}
