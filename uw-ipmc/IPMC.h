/*
 * IPMC.h
 *
 *  Created on: Sep 14, 2017
 *      Author: jtikalsky
 */

#ifndef SRC_IPMC_H_
#define SRC_IPMC_H_

#include "libwrap.h" // Mutex wrappers for various non-reentrant stdlib functions.

/*
 * This macro will handle the automatic (at startup, before main) initialization of static semaphore objects.
 *
 * Parameters:
 *   mutex:  The name of the previously defined SemaphoreHandle_t object.
 *   type:   The semaphore type to create.  A xSemaphoreCreate{TYPE}Static FreeRTOS function must exist.
 *
 * Example:
 *   static SemaphoreHandle_t libwrap_mutex = NULL;
 *   UWIPMC_INITIALIZE_STATIC_SEMAPHORE(libwrap_mutex, Mutex);
 */
#define UWIPMC_INITIALIZE_STATIC_SEMAPHORE(sem, type) \
	static void __uwipmc_initialize_semaphore_ ## sem(void) __attribute__((constructor(0))); \
	static void __uwipmc_initialize_semaphore_ ## sem(void) { \
		static StaticSemaphore_t buf; \
		sem = xSemaphoreCreate ## type ## Static(&buf); \
		configASSERT(sem); \
	}

#endif /* SRC_IPMC_H_ */
