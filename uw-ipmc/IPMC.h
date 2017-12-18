/*
 * IPMC.h
 *
 *  Created on: Sep 14, 2017
 *      Author: jtikalsky
 */

#ifndef SRC_IPMC_H_
#define SRC_IPMC_H_

#include "libwrap.h" // Mutex wrappers for various non-reentrant stdlib functions.
#include "xscugic.h"
#include "xgpiops.h"
#include <libs/LogTree.h>

/**
 * This macro will handle the automatic (at startup, before main) initialization of static semaphore objects.
 *
 * \note This refers to "static memory" not "static linkage".  (i.e. You don't need to use the "static" keyword on your semaphore.
 *
 * \param sem   The name of the previously defined SemaphoreHandle_t object.
 * \param type  The semaphore type to create.  A xSemaphoreCreate{TYPE}Static FreeRTOS function must exist.
 *
 * Example:
 *   static SemaphoreHandle_t libwrap_mutex = NULL;
 *   UWIPMC_INITIALIZE_STATIC_SEMAPHORE(libwrap_mutex, Mutex);
 */
#define UWIPMC_INITIALIZE_STATIC_SEMAPHORE(sem, type) \
	static void __uwipmc_initialize_semaphore_ ## sem(void) __attribute__((constructor(1000))); \
	static void __uwipmc_initialize_semaphore_ ## sem(void) { \
		static StaticSemaphore_t buf; \
		sem = xSemaphoreCreate ## type ## Static(&buf); \
		configASSERT(sem); \
	}

extern "C" {
	extern XScuGic xInterruptController;
}

class PS_UART;
extern PS_UART *uart_ps0;
extern XGpioPs gpiops;

extern LogTree LOG;
extern LogTree::Filter *console_log_filter;
class TraceBuffer;
extern TraceBuffer TRACE;

void driver_init(bool use_pl);
void ipmc_service_init();

u8 ipmi_checksum(const u8* buf, u32 len);

#endif /* SRC_IPMC_H_ */
