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

#include "threading.h"
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <timers.h>
#include <event_groups.h>
#include <task.h>
#include <core.h>
#include <drivers/tracebuffer/tracebuffer.h>
#include <libs/printf.h>
#include <libs/except.h>
#include <libs/threading.h>
#include <zynqipmc_config.h>

TickType_t AbsoluteTimeout::getTimeout() {
	if (this->timeout64 == UINT64_MAX)
		return portMAX_DELAY;

	CriticalGuard critical(true);
	uint64_t now64 = get_tick64();
	TickType_t ret;
	if (this->timeout64 <= now64) {
		ret = 0; // Expired.
	}
	else if (this->timeout64 - now64 > portMAX_DELAY) {
		ret = portMAX_DELAY-1; // Don't block forever, but block as long as we can.
	}
	else {
		ret = this->timeout64 - now64; // Block until timeout, it's within TickType_t.
	}
	return ret;
}

void AbsoluteTimeout::setTimeout(TickType_t relative_timeout) {
	if (relative_timeout == portMAX_DELAY)
		this->setTimeout(UINT64_MAX);
	else
		this->setTimeout(static_cast<uint64_t>(relative_timeout));
}

void AbsoluteTimeout::setTimeout(uint64_t relative_timeout) {
	if (relative_timeout == UINT64_MAX) {
		CriticalGuard critical(true);
		this->timeout64 = UINT64_MAX;
		return;
	}
	CriticalGuard critical(true);
	uint64_t now = get_tick64();
	if ((UINT64_MAX - relative_timeout) <= now) // Wait past the end of time?  Never!
		throw std::domain_error("We can't wait that long.  Please choose a time shorter than the life of the sun.");
	this->timeout64 = now + relative_timeout;
}

/**
 * A deleter for use with std::shared_ptr to clean up EventGroupHandle_t.
 *
 * @param eg An EventGroupHandle_t as a void*
 */
static void deleteEventgroupVoidptr(void *eg) {
	vEventGroupDelete((EventGroupHandle_t)eg);
}

WaitList::WaitList()
	: event(nullptr) {
	this->mutex = xSemaphoreCreateMutex();
	this->event = std::shared_ptr<void>(xEventGroupCreate(), deleteEventgroupVoidptr);
}

WaitList::~WaitList() {
	this->wake(); // Don't leave anyone waiting on us.
	vSemaphoreDelete(this->mutex);
}

WaitList::Subscription WaitList::join() {
	configASSERT(!IN_INTERRUPT());

	xSemaphoreTake(this->mutex, portMAX_DELAY);
	if (xEventGroupGetBits((EventGroupHandle_t)this->event.get())) // Already spent.
		this->event = std::shared_ptr<void>(xEventGroupCreate(), deleteEventgroupVoidptr);
	std::shared_ptr<void> ret = this->event;
	xSemaphoreGive(this->mutex);
	return Subscription(ret);
}

bool WaitList::Subscription::wait(TickType_t timeout) {
	configASSERT(!!this->event);
	return xEventGroupWaitBits((EventGroupHandle_t)this->event.get(), 1, 0, pdTRUE, timeout);
}

void WaitList::wake() {
	if (IN_INTERRUPT()) {
		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		xEventGroupSetBitsFromISR((EventGroupHandle_t)this->event.get(), 1, &xHigherPriorityTaskWoken);
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
	else {
		xEventGroupSetBits((EventGroupHandle_t)this->event.get(), 1);
	}
}

/// A custom 64 bit tick counter.  Access only through get_tick64().
volatile uint64_t uwipmc_tick64_count = 0;

extern "C" {
/**
 * Hook the FreeRTOS tick timer to increment our own 64bit tick counter.
 */
void vApplicationTickHook() {
	uwipmc_tick64_count++;

	// Also update current time, if previously updated by NTP
	extern volatile uint64_t _time_in_us;
	_time_in_us += 1000000 / configTICK_RATE_HZ;
}
}; // extern "C"

extern "C" {
void __real_print( const char8 *ptr);
};
#include <iostream>

std::string renderExceptionReport(const BackTrace *trace, const std::exception *exception, std::string location_description) {
	if (location_description.size())
		location_description.insert(location_description.begin(), ' ');
	std::string diag = "Uncaught exception";
	if (trace) {
		// There is a trace available!
		diag += " ";
		if (exception)
			diag += trace->getName() + "(\"" + exception->what() + "\")" + location_description;
		else
			diag += "'" + trace->getName() + "'" + location_description;
		diag += ":\n" + trace->toString();
	} else {
		if (exception)
			diag += std::string(" [std::exception](\"") + exception->what() + "\")" + location_description;
		else
			diag += location_description;
		diag += ". No trace available.";
	}
	return diag;
}

static void printException(const std::exception *exception) {
	TaskHandle_t handler = xTaskGetCurrentTaskHandle();
	std::string tskname = "unknown_task";
	if (handler)
		tskname = pcTaskGetName(handler);

	BackTrace* trace = BackTrace::traceException();

	std::string diag = renderExceptionReport(trace, exception, std::string("in task '") + tskname + "'");

	/* Put it through the trace facility, so regardless of our ability to
	 * put it through the standard log paths, it gets trace logged.
	 */
	std::string log_facility = stdsprintf("ipmc.unhandled_exception.%s", tskname.c_str());
	TRACE.log(log_facility.c_str(), log_facility.size(), LogTree::LOG_CRITICAL, diag.c_str(), diag.size());

	// Put it directly to the UART console, for the same reason.
	std::string wnl_diag = diag;
	windows_newline(wnl_diag);
	__real_print(wnl_diag.c_str());

	// Put it through the standard log system.
	LOG[tskname].log(diag, LogTree::LOG_CRITICAL);
}

/**
 * Provides the wrapper handling function call and thread cleanup for UWTaskCreate().
 *
 * @param stdfunc_cb The *stdfunction to execute and destroy.
 */
static void _runTask(void *stdfunc_cb) {
	std::function<void(void)> *stdfunc = reinterpret_cast< std::function<void(void)>* >(stdfunc_cb);
	try {
		(*stdfunc)();
	}
	catch (const std::exception &e) {
		printException(&e);
	}
	catch (...) {
		printException(nullptr);
	}
	delete stdfunc;
	vTaskDelete(nullptr);
}

TaskHandle_t runTask(const std::string& name, BaseType_t priority, std::function<void(void)> thread_func, BaseType_t stack_words) {
	if (name.size() >= configMAX_TASK_NAME_LEN) // < because we need the '\0' still.
		throw std::domain_error(stdsprintf("The name \"%s\" (%u) is longer than the maximum thread name length (%u).", name.c_str(), name.size(), configMAX_TASK_NAME_LEN));

	TaskHandle_t handle = nullptr;
	if (pdFAIL == xTaskCreate(
			_runTask,
			name.c_str(),
			(stack_words ? stack_words : ZYNQIPMC_BASE_STACK_SIZE) + 40 /* UWTaskCreate wrappers/etc consume 40 words of stack */,
			new std::function<void(void)>(thread_func),
			priority,
			&handle))
		throw except::thread_create_error(std::string("Unable to create thread \"")+name+"\", xTaskCreate returned pdFAIL.");

	return handle;
}
