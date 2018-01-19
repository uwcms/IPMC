/*
 * libwrap.c
 *
 *  Created on: Sep 14, 2017
 *      Author: jtikalsky
 */

#include <IPMC.h>
#include <drivers/ps_uart/PSUART.h>
#include <libs/StatCounter.h>
#include <libs/LogTree.h>

#include <FreeRTOS.h>
#include <semphr.h>
#include <IPMC.h>

#include <stdio.h>
#include <stdarg.h>
#include <cxxabi.h> // for cxa_demangle()

extern "C" {
int __real_printf(const char *format, ...);
int __real_sprintf(char *str, const char *format, ...);
int __real_snprintf(char *str, size_t size, const char *format, ...);
int __real_vprintf(const char *format, va_list ap);
int __real_vsprintf(char *str, const char *format, va_list ap);
int __real_vsnprintf(char *str, size_t size, const char *format, va_list ap);

void *__wrap_malloc(size_t size);
void __wrap_free(void *ptr);
void *__wrap_calloc(size_t nmemb, size_t size);
void *__wrap_realloc(void *ptr, size_t size);
int __wrap_printf(const char *format, ...);
int __wrap_sprintf(char *str, const char *format, ...);
int __wrap_snprintf(char *str, size_t size, const char *format, ...);
int __wrap_vprintf(const char *format, va_list ap);
int __wrap_vsprintf(char *str, const char *format, va_list ap);
int __wrap_vsnprintf(char *str, size_t size, const char *format, va_list ap);
void __wrap_xil_printf( const char8 *ctrl1, ...);
void __wrap_print( const char8 *ptr);
} // extern "C"

volatile SemaphoreHandle_t stdlib_mutex = NULL;

void *__wrap_malloc(size_t size) {
	return pvPortMalloc(size);
}

void __wrap_free(void *ptr) {
	vPortFree(ptr);
}

void *__wrap_calloc(size_t nmemb, size_t size) {
	configASSERT(0); // Unimplemented.  Check manpage for memory initialization requirements.
	return NULL;
}

/* We need to allocate this somewhere, and the FreeRTOS managed allocation is
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
     * initialized.   If  ptr  is  NULL,  then  the call is equivalent to mal‐
     * loc(size), for all values of size; if size is equal to zero, and ptr is
     * not  NULL,  then  the  call  is equivalent to free(ptr).  Unless ptr is
     * NULL, it must have been returned by an earlier call to  malloc(),  cal‐
     * loc()  or  realloc().  If the area pointed to was moved, a free(ptr) is
     * done.
	 */
	if (!ptr)
		return (size ? malloc(size) : NULL);

	if (!size) {
		free(ptr);
		return NULL;
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

void init_stdlib_mutex() {
	if (stdlib_mutex)
		return;
	SemaphoreHandle_t sem = xSemaphoreCreateMutex();
	configASSERT(sem);
	taskENTER_CRITICAL();
	if (!stdlib_mutex) {
		stdlib_mutex = sem;
		sem = NULL;
	}
	taskEXIT_CRITICAL();
	if (sem)
		vSemaphoreDelete(sem);
}

int __wrap_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    int ret = vprintf(format, args);
    va_end(args);
    return ret;
}

int __wrap_sprintf(char *str, const char *format, ...) {
	init_stdlib_mutex();
	va_list args;
	va_start(args, format);
	xSemaphoreTake(stdlib_mutex, portMAX_DELAY);
	int ret = __real_vsprintf(str, format, args);
	xSemaphoreGive(stdlib_mutex);
	va_end(args);
	return ret;
}

int __wrap_snprintf(char *str, size_t size, const char *format, ...) {
	init_stdlib_mutex();
	va_list args;
	va_start(args, format);
	xSemaphoreTake(stdlib_mutex, portMAX_DELAY);
	int ret = __real_vsnprintf(str, size, format, args);
	xSemaphoreGive(stdlib_mutex);
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
	int s = vsnprintf(NULL, 0, format, ap) + 1;
	if (s <= 0) {
		va_end(ap);
		va_end(ap2);
		return s;
	}
	char *str = (char*) pvPortMalloc(s);
	vsnprintf(str, s, format, ap2);
	va_end(ap);
	va_end(ap2);
	std::string outstr(str);
	vPortFree(str);
	windows_newline(outstr);

	// printf goes through the log facility now.
	static LogTree *printf_trace = NULL;
	if (!printf_trace)
		printf_trace = &LOG["printf"];
	printf_trace->log(outstr, LogTree::LOG_NOTICE);
	printfs_count.increment();
	printfs_bytes.increment(outstr.size());
	return outstr.size();
}

int __wrap_vsprintf(char *str, const char *format, va_list ap) {
	init_stdlib_mutex();
	xSemaphoreTake(stdlib_mutex, portMAX_DELAY);
	int ret = vsprintf(str, format, ap);
	xSemaphoreGive(stdlib_mutex);
	return ret;
}

int __wrap_vsnprintf(char *str, size_t size, const char *format, va_list ap) {
	init_stdlib_mutex();
	xSemaphoreTake(stdlib_mutex, portMAX_DELAY);
	int ret = __real_vsnprintf(str, size, format, ap);
	xSemaphoreGive(stdlib_mutex);
	return ret;
}

void __wrap_xil_printf( const char8 *ctrl1, ...) {
    va_list args;
    va_start(args, ctrl1);
    vprintf(ctrl1, args);
    va_end(args);
}

void __wrap_print( const char8 *ptr) {
	printf("%s", ptr);
}

/**
 * This function calls the cross-vendor C++ Application Binary Interface to
 * demangle a C++ type name.
 *
 * @param name The name to be demangled, perhaps returned from typeid(T).name()
 * @return The demangled form of name, or name if an error occurred.
 */
std::string cxa_demangle(const char *name) {
	init_stdlib_mutex();
	size_t length = 0;
	int status = 0;
	std::string result(name);
	xSemaphoreTake(stdlib_mutex, portMAX_DELAY);
	// __cxa_demangle will malloc, but that's now intercepted, but it might still need this protection.
	char *demangled = abi::__cxa_demangle(name, NULL, &length, &status);
	xSemaphoreGive(stdlib_mutex);
	if (status == 0 && demangled)
		result = demangled;
	if (demangled)
		free(demangled);
	return result;
}
