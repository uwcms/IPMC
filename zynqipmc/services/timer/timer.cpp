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

#include "timer.h"
#include <libs/threading.h>
#include <libs/printf.h>

TimerService *TimerService::global_timers[configMAX_PRIORITIES];

TimerService& TimerService::globalTimer(BaseType_t process_priority) {
	if (process_priority == 0)
		process_priority = uxTaskPriorityGet(nullptr);

	if (process_priority >= configMAX_PRIORITIES)
		throw std::logic_error(stdsprintf("A timer service cannot be instantiated for invalid priority %lu.", process_priority));

	if (!TimerService::global_timers[process_priority]) {
		bool created = false;

		SuspendGuard guard(true);
		if (!TimerService::global_timers[process_priority]) {
			TimerService::global_timers[process_priority] = new TimerService();
			created = true;
		}
		guard.release();

		if (created) {
			// We have to do this outside of the SuspendGuard, since printf can contend a lock internally.
			TimerService::global_timers[process_priority]->start(stdsprintf("Timer%lu", process_priority), process_priority);
		}
	}
	return *TimerService::global_timers[process_priority];
}

TimerService::TimerService(BaseType_t input_queue_length) {
	configASSERT(this->mutex = xSemaphoreCreateMutex());
	configASSERT(this->input_queue = xQueueCreate(input_queue_length, sizeof(std::shared_ptr<Timer>*)));
}

TimerService::~TimerService() {
	configASSERT(0); // We don't have a mechanism to stop the thread.
	vQueueDelete(this->input_queue);
	vSemaphoreDelete(this->mutex);
}

void TimerService::submit(std::shared_ptr<Timer> &timer) {
	std::shared_ptr<Timer> *qtmr = new std::shared_ptr<Timer>(timer);
	xQueueSend(this->input_queue, &qtmr, portMAX_DELAY);
}

void TimerService::start(const std::string &thread_name, BaseType_t thread_priority, BaseType_t stack_words) {
	runTask(thread_name, thread_priority, [this]()->void{this->runThread();}, stack_words);
}

void TimerService::runThread() {
	AbsoluteTimeout next(portMAX_DELAY); // The next timeout
	while (true) {
		// Wait once, then add any new timers to the set.
		TickType_t timeout = next.getTimeout();
		while (true) {
			std::shared_ptr<Timer> *tptr = nullptr;
			if (pdTRUE == xQueueReceive(this->input_queue, &tptr, timeout)) {
				MutexGuard<false> lock(this->mutex, true);
				this->timers.push_back(*tptr);
				delete tptr;
			} else {
				break; // Nothing more to receive in this pass.
			}

			timeout = 0; // Don't wait again.
		}

		// Iterate and call & rearm relevant timers.
		MutexGuard<false> lock(this->mutex, true);
		uint64_t now = get_tick64();
		next.setAbsTimeout(UINT64_MAX);
		for (auto it = this->timers.begin(), eit = this->timers.end(); it != eit; ) {
			std::shared_ptr<Timer> &timer = *it;
			if (timer->isCancelled()) {
				// Delete cancelled timers.
				it = this->timers.erase(it);
				continue;
			}

			if (timer->next.getTimeout64() <= now) {
				lock.release();
				timer->run();
				lock.acquire();
				if (timer->kRearmEvery) {
					// This timer auto-rearms.
					// We adjust the existing absolute timeout to avoid drift.
					timer->next.setAbsTimeout(timer->next.getTimeout64() + timer->kRearmEvery);
				} else {
					// This timer has fired and is finished.
					it = this->timers.erase(it);
					continue;
				}
			}

			if (timer->next < next)
				next = timer->next;

			it++;
		}
	}
}

void TimerService::Timer::run() {
	MutexGuard<true> lock(this->mutex, true);
	if (!this->cancelled)
		this->func();
}

void TimerService::Timer::cancel(bool synchronous) {
	MutexGuard<true> lock(this->mutex, synchronous);
	this->cancelled = true;
}
