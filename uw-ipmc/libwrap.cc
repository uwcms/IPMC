/*
 * libwrap.c
 *
 *  Created on: Sep 14, 2017
 *      Author: jtikalsky
 */

#define SRC_LIBWRAP_C_
#include <libwrap.h> // auto-included from IPMC.h, but include first here to ensure there are no failed hidden dependencies in libwrap.h
#include <IPMC.h>
#include <drivers/ps_uart/PSUART.h>
#include <libs/StatCounter.h>

#include <FreeRTOS.h>
#include <semphr.h>
#include <IPMC.h>

#include <stdio.h>
#include <stdarg.h>

static SemaphoreHandle_t libwrap_mutex = NULL;
UWIPMC_INITIALIZE_STATIC_SEMAPHORE(libwrap_mutex, Mutex);

int _uwipmc_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    int ret = _uwipmc_vprintf(format, args);
    va_end(args);
    return ret;
}

int _uwipmc_sprintf(char *str, const char *format, ...) {
	va_list args;
	va_start(args, format);
	xSemaphoreTake(libwrap_mutex, portMAX_DELAY);
	int ret = vsprintf(str, format, args);
	xSemaphoreGive(libwrap_mutex);
	va_end(args);
	return ret;
}

int _uwipmc_snprintf(char *str, size_t size, const char *format, ...) {
	va_list args;
	va_start(args, format);
	xSemaphoreTake(libwrap_mutex, portMAX_DELAY);
	int ret = vsnprintf(str, size, format, args);
	xSemaphoreGive(libwrap_mutex);
	va_end(args);
	return ret;
}

static SemaphoreHandle_t libwrap_outbuf_mutex = NULL;
UWIPMC_INITIALIZE_STATIC_SEMAPHORE(libwrap_outbuf_mutex, Mutex);

static char printf_outbuf[PRINTF_MAX_LENGTH];

StatCounter printf_overrun_discard_count("printf.stdout_full.count");
StatCounter printf_overrun_discard_bytes("printf.stdout_full.lost_bytes");
StatCounter printfs_count("printf.count");
StatCounter printfs_bytes("printf.bytes");

int _uwipmc_vprintf(const char *format, va_list ap) {
	xSemaphoreTake(libwrap_outbuf_mutex, portMAX_DELAY);
	xSemaphoreTake(libwrap_mutex, portMAX_DELAY);
	int ret = vsnprintf(printf_outbuf, PRINTF_MAX_LENGTH, format, ap);
	configASSERT(ret < PRINTF_MAX_LENGTH); // Catch cases where a printf filled the whole buffer (and probably was truncated).
	xSemaphoreGive(libwrap_mutex);

	/* We have returned libwrap_mutex so things like sprintf or anything else we
	 * choose to wrap does not block on stdout writes.  We will protect our
	 * static buffer with another mutex, instead.
	 *
	 * It is not sufficiently valuable to require all stack sizes increased to
	 * match the maximum possible allocated buffer size, or to stress ourselves
	 * with true dynamic sizing in the end.
	 */
	// TODO: Mirror to tracebuffer.
	int written = 0;
	if (ret > 0)
		written = uart_ps0->write(printf_outbuf, ret, 0);
	if (written != ret) {
		printf_overrun_discard_count.increment();
		printf_overrun_discard_bytes.increment(ret-written);
	}
	printfs_count.increment();
	printfs_bytes.increment(ret);
	xSemaphoreGive(libwrap_outbuf_mutex);
	return ret;
}

int _uwipmc_vsprintf(char *str, const char *format, va_list ap) {
	xSemaphoreTake(libwrap_mutex, portMAX_DELAY);
	int ret = vsprintf(str, format, ap);
	xSemaphoreGive(libwrap_mutex);
	return ret;
}

int _uwipmc_vsnprintf(char *str, size_t size, const char *format, va_list ap) {
	xSemaphoreTake(libwrap_mutex, portMAX_DELAY);
	int ret = vsnprintf(str, size, format, ap);
	xSemaphoreGive(libwrap_mutex);
	return ret;
}

std::string stdsprintf(const char *fmt, ...) {
    va_list va;
    va_list va2;
    va_start(va, fmt);
    va_copy(va2, va);
    size_t s = _uwipmc_vsnprintf(NULL, 0, fmt, va) + 1;
    char str[s];
    _uwipmc_vsnprintf(str, s, fmt, va2);
    va_end(va);
    va_end(va2);
    return std::string(str);
}
