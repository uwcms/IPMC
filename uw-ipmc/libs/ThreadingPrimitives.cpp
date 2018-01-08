#include <FreeRTOS.h>
#include <libs/ThreadingPrimitives.h>
#include <task.h>
#include <semphr.h>
#include <timers.h>
#include <event_groups.h>
#include <task.h>
#include <IPMC.h>

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
}
}; // extern "C"
