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

/**
 * This file contains several standard functions implemented in a way that is optimal
 * for the ZYQN-IPMC framework, including memory management and stdout.
 *
 * None of this functions will be directly used, the compiler will take care of wrapping
 * and the system calls.
 */

#include <libs/printf.h>
#include <FreeRTOS.h>
#include <semphr.h>
#include <core.h>

#include <stdio.h>
#include <stdarg.h>
#include <algorithm>
#include <ctype.h>
#include <drivers/ps_uart/ps_uart.h>
#include <libs/logtree/logtree.h>
#include <libs/statcounter/statcounter.h>
#include <libs/threading.h>
#include <time.h>
#include <sys/time.h>

#define WRAP_ATTR __attribute__((externally_visible))

//! Re-declare cpp new
inline void* operator new(std::size_t n) { return malloc(n); }

//! Re-declare cpp delete
inline void operator delete(void* p) throw() { free(p); }

extern "C" {
/**
 * List of original system functions that will be getting replaced.
 * @{
 */
int __real_printf(const char *format, ...);
int __real_sprintf(char *str, const char *format, ...);
int __real_snprintf(char *str, size_t size, const char *format, ...);
int __real_vprintf(const char *format, va_list ap);
int __real_vsprintf(char *str, const char *format, va_list ap);
int __real_vsnprintf(char *str, size_t size, const char *format, va_list ap);
void __real_sha_256(const unsigned char *in, const unsigned int size, unsigned char *out);
/** @}*/

/**
 * List of custom system functions that will get implemented.
 * @{
 */
void *__wrap_malloc(size_t size) WRAP_ATTR __attribute__((malloc, alloc_size(1)));
void __wrap_free(void *ptr) WRAP_ATTR;
void *__wrap_calloc(size_t nmemb, size_t size) WRAP_ATTR __attribute__((malloc, alloc_size(1,2)));
void *__wrap_realloc(void *ptr, size_t size) WRAP_ATTR __attribute__((alloc_size(1))); // __attribute__((malloc)) is not valid for realloc.
int __wrap_printf(const char *format, ...) WRAP_ATTR;
int __wrap_sprintf(char *str, const char *format, ...) WRAP_ATTR;
int __wrap_snprintf(char *str, size_t size, const char *format, ...) WRAP_ATTR;
int __wrap_vprintf(const char *format, va_list ap) WRAP_ATTR;
int __wrap_vsprintf(char *str, const char *format, va_list ap) WRAP_ATTR;
int __wrap_vsnprintf(char *str, size_t size, const char *format, va_list ap) WRAP_ATTR;
void __wrap_xil_printf( const char8 *ctrl1, ...) WRAP_ATTR;
void __wrap_print( const char8 *ptr) WRAP_ATTR;
unsigned __wrap_sleep(unsigned int seconds) WRAP_ATTR;
void __wrap_sha_256(const unsigned char *in, const unsigned int size, unsigned char *out) WRAP_ATTR;
/** @}*/

/**
 * The following is to support time functions without a RTC.
 * timeofday_in_us should be incremented with an internal timer function
 * and it should be set by NTP.
 * @{
 */
volatile uint32_t _ntp_updates = 0;
volatile uint64_t _time_in_us = 0;
/** @}*/

int _gettimeofday( struct timeval *tv, struct timezone *tz )
{
	// Make a non-volatile local copy
	CriticalGuard critical(true);
	uint64_t time_us = _time_in_us;
	critical.release();

	tv->tv_sec = time_us / 1000000;   // convert to seconds
	tv->tv_usec = time_us % 1000000;  // get remaining microseconds
	return 0;  // return non-zero for error
}

} // extern "C"

SemaphoreHandle_t stdlib_mutex = nullptr;

void *__wrap_malloc(size_t size) {
	// The FreeRTOS pvPortMalloc/vPortFree are not ISR safe, only critical safe.
	if (IN_INTERRUPT())
		abort();
	return pvPortMalloc(size);
}

void __wrap_free(void *ptr) {
	// The FreeRTOS pvPortMalloc/vPortFree are not ISR safe, only critical safe.
	if (IN_INTERRUPT())
		abort();
	vPortFree(ptr);
}

void *__wrap_calloc(size_t nmemb, size_t size) {
	configASSERT(0); // Unimplemented.  Check manpage for memory initialization requirements.
	return nullptr;
}

/**
 * We need to allocate this somewhere, and the FreeRTOS managed allocation is
 * declared as static, so we can't check it's address or offsets relative to it,
 * which we will need to do for abort safety in realloc, below.
 */
uint8_t ucHeap[ configTOTAL_HEAP_SIZE ];

void *__wrap_realloc(void *ptr, size_t size) {
	/* man malloc:
	 *
	 * The  realloc() function changes the size of the memory block pointed to
     * by ptr to size bytes.  The contents will be unchanged in the range from
     * the start of the region up to the minimum of the old and new sizes.  If
     * the new size is larger than the old size, the added memory will not  be
     * initialized.   If  ptr  is  nullptr,  then  the call is equivalent to mal‐
     * loc(size), for all values of size; if size is equal to zero, and ptr is
     * not  nullptr,  then  the  call  is equivalent to free(ptr).  Unless ptr is
     * nullptr, it must have been returned by an earlier call to  malloc(),  cal‐
     * loc()  or  realloc().  If the area pointed to was moved, a free(ptr) is
     * done.
	 */
	if (!ptr)
		return (size ? malloc(size) : nullptr);

	if (!size) {
		free(ptr);
		return nullptr;
	}

	// We... aren't going to be smart about this, sorry.

	// But we'll at least avoid a data fault on the copy.
	size_t copysize = size;
	if (reinterpret_cast<uint8_t*>(ptr)+size > ucHeap+configTOTAL_HEAP_SIZE)
		copysize = (ucHeap+configTOTAL_HEAP_SIZE) - (reinterpret_cast<uint8_t*>(ptr));

	// New memory.
	void *newptr = malloc(size);
	memcpy(newptr, ptr, copysize);
	free(ptr);
	return newptr;
}

int __wrap_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    int ret = vprintf(format, args);
    va_end(args);
    return ret;
}

int __wrap_sprintf(char *str, const char *format, ...) {
	safe_init_static_mutex(stdlib_mutex, false);
	va_list args;
	va_start(args, format);
	MutexGuard<false> lock(stdlib_mutex, true);
	int ret = __real_vsprintf(str, format, args);
	lock.release();
	va_end(args);
	return ret;
}

int __wrap_snprintf(char *str, size_t size, const char *format, ...) {
	safe_init_static_mutex(stdlib_mutex, false);
	va_list args;
	va_start(args, format);
	MutexGuard<false> lock(stdlib_mutex, true);
	int ret = __real_vsnprintf(str, size, format, args);
	lock.release();
	va_end(args);
	return ret;
}

StatCounter printfs_count("printf.count");
StatCounter printfs_bytes("printf.bytes");

int __wrap_vprintf(const char *format, va_list ap) {
	/* We have to do this printf ourself, but we'll do it onto the heap.
	 *
	 * We're going to do a snprintf with length 0, to get the hypothetical
	 * output length, then malloc that, then create a std::string with it so
	 * that we can call windows_newline() on it (I hate you, UART), and free
	 * the temporary buffer.  Then we'll write out that std::string.
	 */
	va_list ap2;
	va_copy(ap2, ap);
	int s = vsnprintf(nullptr, 0, format, ap) + 1;
	if (s <= 0) {
		va_end(ap);
		va_end(ap2);
		return s;
	}
	char *str = (char*) malloc(s);
	vsnprintf(str, s, format, ap2);
	va_end(ap);
	va_end(ap2);
	std::string outstr(str);
	free(str);

	// printf goes through the log facility now.
	static LogTree *printf_trace = nullptr;
	if (!printf_trace)
		printf_trace = &LOG["printf"];
	printf_trace->log(outstr, LogTree::LOG_NOTICE);
	printfs_count.increment();
	printfs_bytes.increment(outstr.size());
	return outstr.size();
}

int __wrap_vsprintf(char *str, const char *format, va_list ap) {
	safe_init_static_mutex(stdlib_mutex, false);
	MutexGuard<false> lock(stdlib_mutex, true);
	return vsprintf(str, format, ap);
}

int __wrap_vsnprintf(char *str, size_t size, const char *format, va_list ap) {
	safe_init_static_mutex(stdlib_mutex, false);
	MutexGuard<false> lock(stdlib_mutex, true);
	return __real_vsnprintf(str, size, format, ap);
}

void __wrap_xil_printf( const char8 *ctrl1, ...) {
    va_list args;
    va_start(args, ctrl1);
    vprintf(ctrl1, args);
    va_end(args);
}

extern "C" {
void ipmc_lwip_printf(const char *ctrl1, ...);
}

void ipmc_lwip_printf(const char *ctrl1, ...) {
	/* We have to do this printf ourself, but we'll do it onto the heap.
	 *
	 * We're going to do a snprintf with length 0, to get the hypothetical
	 * output length, then malloc that, then create a std::string with it so
	 * that we can call windows_newline() on it (I hate you, UART), and free
	 * the temporary buffer.  Then we'll write out that std::string.
	 */
	va_list ap, ap2;
	va_start(ap, ctrl1);
	va_copy(ap2, ap);
	int s = vsnprintf(nullptr, 0, ctrl1, ap) + 1;
	if (s <= 0) {
		va_end(ap);
		va_end(ap2);
		return;
	}
	char *str = (char*) malloc(s);
	vsnprintf(str, s, ctrl1, ap2);
	va_end(ap);
	va_end(ap2);
	std::string outstr(str);
	free(str);
	windows_newline(outstr);

	// We need to strip trailing \r\n.  The logger handles that.
	while (outstr.size() && (outstr.back() == '\r' || outstr.back() == '\n'))
		outstr.pop_back();

	if (outstr.empty())
		return;

	/* Alright, now we have the formatted string.  We just need to figure out
	 * an appropriate loglevel for it.  Since this is for messages from the lwip
	 * core library, we don't get any kind of proper hints about this, so we're
	 * going to have to guess.
	 */

//	static const std::map<std::string, enum LogTree::LogLevel> prio_mapping{
//		{"assert",    LogTree::LOG_CRITICAL},
//		{"error",     LogTree::LOG_ERROR},
//		{"err:",      LogTree::LOG_ERROR},
//		{"failed",    LogTree::LOG_ERROR},
//		{"failure",   LogTree::LOG_ERROR},
//		{"incorrect", LogTree::LOG_ERROR},
//		{"unable",    LogTree::LOG_ERROR},
//		{"invalid",   LogTree::LOG_ERROR},
//		{"warn",      LogTree::LOG_WARNING},
//	};
//	std::string outstr_l = outstr;
//	std::transform(outstr_l.begin(), outstr_l.end(), outstr_l.begin(), tolower /* defined in ctype.h */);
//	enum LogTree::LogLevel outlevel = LogTree::LOG_INFO;
//	for (auto it = prio_mapping.begin(), eit = prio_mapping.end(); it != eit; ++it) {
//		if (it->second < outlevel && outstr_l.find(it->first) != std::string::npos)
//			outlevel = it->second;
//	}

	static LogTree *lwiplog = nullptr;
	if (!lwiplog)
		lwiplog = &LOG["network"]["lwip"];
	//lwiplog->log(outstr, outlevel);
	lwiplog->log(outstr, LogTree::LOG_ERROR);
}

void __wrap_print( const char8 *ptr) {
	printf("%s", ptr);
}

unsigned __wrap_sleep(unsigned int seconds) {
	vTaskDelay(seconds * configTICK_RATE_HZ);
	return 0;
}

static SemaphoreHandle_t librsa_mutex = nullptr;

void __wrap_sha_256(const unsigned char *in, const unsigned int size, unsigned char *out) {
	safe_init_static_mutex(librsa_mutex, false);
	MutexGuard<false> lock(librsa_mutex, true);
	__real_sha_256(in, size, out);
}

// Expose __real_print which is then used in abort()
extern "C" {
void __real_print(const char8 *ptr);
}

// Replace week abort with one that actually does something.
// In case abort gets called there will be a stack trace showing on the console.
// __real_printf is used so that writes to the UART driver are blocking.
void abort() {
	BackTrace *extrace = BackTrace::traceException();

	TaskHandle_t handler = xTaskGetCurrentTaskHandle();
	std::string tskname = "unknown_task";
	if (handler) {
		tskname = pcTaskGetName(handler);
	}

	std::string output;
	if (extrace) {
		output += "\n-- ABORT DUE TO EXCEPTION --\n";
		output += extrace->toString();
	} else {
		BackTrace trace;
		trace.trace();
		output += "\n-- ABORT CALLED --\n";
		output += trace.toString();
	}

	output += "-- ASSERTING --\n";

	/* Put it through the trace facility, so regardless of our ability to
	 * put it through the standard log paths, it gets trace logged.
	 */
	std::string log_facility = "ipmc.unhandled_exception." + tskname;
	TRACE.log(log_facility.c_str(), log_facility.size(), LogTree::LOG_CRITICAL, output.c_str(), output.size());

	/* Put it directly to the UART console, for the same reason.
	 *
	 * This is second because the TraceBuffer facility is highly self-contained
	 * and designed for absolute zero-dependency reliability.  windows_newline()
	 * performs memory allocations via std::string internals, in order to make
	 * space for the extra \r in \r\n sequences, and as such, it relies on
	 * functional memory allocation machinery which the TraceBuffer facility
	 * does not require.
	 *
	 * At some point, perhaps __real_print() can be reimplemented as a custom
	 * for() loop relying on putc() with conditional logic rather than string
	 * buffer editing in this instance, to remove this reliance on the allocator.
	 */
	std::string wnl_output = output;
	windows_newline(wnl_output);
	__real_print(wnl_output.c_str());

	// Put it through the standard log system.
	LOG[tskname].log(output, LogTree::LOG_CRITICAL);

	configASSERT(0);

	/* This function is attribute noreturn, configASSERT(0) can technically
	 * return within a debugger.  This literally can't, it has no epilogue.
	 *
	 * Ensure it doesn't.
	 */
	while (1);
}

