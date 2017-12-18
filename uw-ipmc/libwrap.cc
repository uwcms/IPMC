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
#include <libs/LogTree.h>

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

StatCounter printf_overrun_discard_count("printf.stdout_full.count");
StatCounter printf_overrun_discard_bytes("printf.stdout_full.lost_bytes");
StatCounter printfs_count("printf.count");
StatCounter printfs_bytes("printf.bytes");

int _uwipmc_vprintf(const char *format, va_list ap) {
	/* We have to do this printf ourself, but we'll do it onto the heap.
	 *
	 * We're going to do a snprintf with length 0, to get the hypothetical
	 * output length, then malloc that, then create a std::string with it so
	 * that we can call windows_newline() on it (I hate you, UART), and free
	 * the temporary buffer.  Then we'll write out that std::string.
	 */
    va_list ap2;
    va_copy(ap2, ap);
    int s = _uwipmc_vsnprintf(NULL, 0, format, ap) + 1;
    if (s <= 0) {
        va_end(ap);
        va_end(ap2);
        return s;
    }
    char *str = (char*)pvPortMalloc(s);
    _uwipmc_vsnprintf(str, s, format, ap2);
    va_end(ap);
    va_end(ap2);
    std::string outstr(str);
    vPortFree(str);
    windows_newline(outstr);

    /* Log all printf output to the trace buffer.
     *
     * Even if this is turned on, the log relay mechanism uses a direct UART
     * write, not a printf, so this won't recurse.
     */
    static LogTree *printf_trace = NULL;
    if (!printf_trace)
    	printf_trace = &LOG["trace"]["printf"];
    printf_trace->log(outstr, LogTree::LOG_TRACE);

	size_t written = uart_ps0->write(outstr.data(), outstr.size(), 0);
	if (written != outstr.size()) {
		printf_overrun_discard_count.increment();
		printf_overrun_discard_bytes.increment(outstr.size()-written);
	}
	printfs_count.increment();
	printfs_bytes.increment(outstr.size());
	return written;
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

/**
 * Modify a std::string in place to contain \r\n where it currently contains \n.
 * @param input The string to modify.
 */
void windows_newline(std::string &input) {
	for (unsigned int i = 0; i < input.size(); ++i) {
		if (input[i] == '\n') {
			input.replace(i, 1, "\r\n");
			i++; // We don't want to hit that \n again on the (now) next character.
		}
	}
}
