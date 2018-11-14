#include <FreeRTOS.h>
#include <libs/ThreadingPrimitives.h>
#include <task.h>
#include <semphr.h>
#include <timers.h>
#include <event_groups.h>
#include <task.h>
#include <IPMC.h>

#include <libs/BackTrace.h>

/**
 * Instantiate an AbsoluteTimeout.
 *
 * \param relative_timeout The relative timeout to be converted to absolute form.
 */
AbsoluteTimeout::AbsoluteTimeout(TickType_t relative_timeout) :
	timeout64(relative_timeout == portMAX_DELAY ? UINT64_MAX : get_tick64()+relative_timeout) {
	// Done.
}

/// \overload
AbsoluteTimeout::AbsoluteTimeout(uint64_t relative_timeout) :
	timeout64(relative_timeout == UINT64_MAX ? UINT64_MAX : get_tick64()+relative_timeout) {
	configASSERT( relative_timeout == UINT64_MAX || (UINT64_MAX - relative_timeout) > get_tick64() ); // Wait past the end of time?  Never!
}

/**
 * Get the current remaining timeout.
 *
 * \note If you have specified a timeout that is longer than `TickType_t-1`, you
 *       may have to block multiple times before the true timeout is expired.
 *
 * \return A correct relative timeout for this object from the current point in time.
 */
TickType_t AbsoluteTimeout::get_timeout() {
	if (this->timeout64 == UINT64_MAX)
		return portMAX_DELAY;

	if (!IN_INTERRUPT())
		taskENTER_CRITICAL();

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
	if (!IN_INTERRUPT())
		taskEXIT_CRITICAL();
	return ret;
}

/**
 * A deleter for use with std::shared_ptr to clean up EventGroupHandle_t.
 *
 * @param eg An EventGroupHandle_t as a void*
 */
static void delete_eventgroup_voidptr(void *eg) {
	vEventGroupDelete((EventGroupHandle_t)eg);
}

WaitList::WaitList()
	: event(NULL) {
	this->mutex = xSemaphoreCreateMutex();
	this->event = std::shared_ptr<void>(xEventGroupCreate(), delete_eventgroup_voidptr);
}

WaitList::~WaitList() {
	this->wake(); // Don't leave anyone waiting on us.
	vSemaphoreDelete(this->mutex);
}

/**
 * Join this waitlist.
 *
 * In order to wait on a waitlist, you must first join it.  The separation of
 * these two operations allows one to join a waitlist while holding various
 * relevant mutexes, but release those mutexes before waiting.
 *
 * \return A WaitList subscription which may be used to wait on this WaitList.
 */
WaitList::Subscription WaitList::join() {
	configASSERT(!IN_INTERRUPT());

	xSemaphoreTake(this->mutex, portMAX_DELAY);
	if (xEventGroupGetBits((EventGroupHandle_t)this->event.get())) // Already spent.
		this->event = std::shared_ptr<void>(xEventGroupCreate(), delete_eventgroup_voidptr);
	std::shared_ptr<void> ret = this->event;
	xSemaphoreGive(this->mutex);
	return Subscription(ret);
}

/**
 * Wait using this WaitList subscription.
 *
 * @param timeout How long to wait.
 * @return true if complete, false if timeout
 *
 * \note This will not refresh the subscription.
 */
bool WaitList::Subscription::wait(TickType_t timeout) {
	configASSERT(!!this->event);
	return xEventGroupWaitBits((EventGroupHandle_t)this->event.get(), 1, 0, pdTRUE, timeout);
}

/**
 * Wake all threads waiting on this WaitList.
 *
 * \note This function is ISR safe.
 */
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


/**
 * trampoline_prepare produces a void* to pass through a FreeRTOS function that
 * requires a callback, such as a thread create or timer function, to be run by
 * #trampoline_launch.
 *
 * \note This involves a heap allocation and therefore is not ISR safe, but is
 *       critical safe. Call #trampoline_cancel to free this if you choose not
 *       to pass it to #trampoline_launch
 *
 * @param cb The void(void) callback to be executed. (Perhaps a lambda or method)
 *           If NULL is supplied, no allocation is made and NULL is returned.
 */
void *trampoline_prepare(std::function<void(void)> cb) {
	configASSERT(!IN_INTERRUPT());
	if (!cb)
		return NULL;
	std::function<void(void)> *cbp = new std::function<void(void)>(cb);
	return cbp;
}

/**
 * trampoline_launch will accept a void* produced by #trampoline_prepare and
 * execute it, then clean it up.
 *
 * \note After this call, the prepared void* may not be reused.c
 *
 * \note This involves a heap deallocation and therefore is not ISR safe, but is
 *       critical safe.
 *
 * @param voidstar The return value of a trampoline_prepare (or NULL for no-op).
 */
void trampoline_launch_pv(void *voidstar) {
	configASSERT(!IN_INTERRUPT());
	if (!voidstar)
		return;
	std::function<void(void)> *cbp = reinterpret_cast< std::function<void(void)>* >(voidstar);
	if (*cbp)
		(*cbp)();
	delete cbp;
}
/// \overload
void trampoline_launch_pv_x(void *voidstar, BaseType_t ignored) {
	trampoline_launch_pv(voidstar);
}

/**
 * trampoline_multilaunch will accept a void* produced by #trampoline_prepare
 * and execute it, but not clean it up, so it can be reused.
 *
 * \note After this call, the prepared void* is not freed and may be reused.
 *
 * \note This function is ISR safe.
 *
 * @param voidstar The return value of a trampoline_prepare (or NULL for no-op).
 */
void trampoline_multilaunch_pv(void *voidstar) {
	if (!voidstar)
		return;
	std::function<void(void)> *cbp = reinterpret_cast< std::function<void(void)>* >(voidstar);
	if (*cbp)
		(*cbp)();
}
/// \overload
void trampoline_multilaunch_pv_x(void *voidstar, BaseType_t ignored) {
	trampoline_multilaunch_pv(voidstar);
}

/**
 * trampoline_cancel will accept a void* produced by #trampoline_prepare and
 * clean it up.
 *
 * \note After this call, the prepared void* may not be reused.c
 *
 * \note This involves a heap deallocation and therefore is not ISR safe, but is
 *       critical safe.
 *
 * @param voidstar The return value of a trampoline_prepare (or NULL for no-op).
 */
void trampoline_cancel(void *voidstar) {
	configASSERT(!IN_INTERRUPT());
	if (!voidstar)
		return;
	delete reinterpret_cast< std::function<void(void)>* >(voidstar);
}
#include <iostream>
/**
 * Provides the wrapper handling function call and thread cleanup for UWTaskCreate().
 *
 * @param stdfunc_cb The *stdfunction to execute and destroy.
 */
static void uwtask_run(void *stdfunc_cb) {
	std::function<void(void)> *stdfunc = reinterpret_cast< std::function<void(void)>* >(stdfunc_cb);
	try {
		(*stdfunc)();
	} catch (...) {
		TaskHandle_t handler = xTaskGetCurrentTaskHandle();
		char *tskname = pcTaskGetName(handler);

		std::string diag;
		BackTrace* trace = BackTrace::traceException();

		if (trace) {
			// There is a trace available!
			diag += stdsprintf("Uncaught exception '%s' in task '%s':\n", trace->getName().c_str(), tskname);
			diag += trace->toString();
		} else {
			diag += stdsprintf("Uncaught exception in thread '%s'. No trace available.", tskname);
		}

		print(diag.c_str());
	}
	delete stdfunc;
	vTaskDelete(NULL);
}

/**
 * Runs the supplied std::function in a new thread, cleaning up the thread when it
 * returns.
 *
 * @param name The name of the thread.
 * @param priority The task priority.
 * @param stack_words The number of words used for the task stack.
 * @param thread_func The function to run, C function, lambda, bind, method, all are welcome.
 * @return
 */
TaskHandle_t UWTaskCreate(const std::string name, BaseType_t priority, std::function<void(void)> thread_func, BaseType_t stack_words) {
	configASSERT(name.size() < configMAX_TASK_NAME_LEN); // < because we need the '\0' still.
	TaskHandle_t handle = NULL;
	if (pdFAIL == xTaskCreate(
			uwtask_run,
			name.c_str(),
			(stack_words ? stack_words : UWIPMC_STANDARD_STACK_SIZE) + 40 /* UWTaskCreate wrappers/etc consume 40 words of stack */,
			new std::function<void(void)>(thread_func),
			priority,
			&handle))
		return NULL;
	return handle;
}
