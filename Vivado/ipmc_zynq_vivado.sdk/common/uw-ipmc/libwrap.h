/*
 * libwrap.h
 *
 *  Created on: Sep 14, 2017
 *      Author: jtikalsky
 */

#ifndef SRC_LIBWRAP_H_
#define SRC_LIBWRAP_H_

#include <stdarg.h>
#include <stddef.h>
#include <FreeRTOS.h>

#define malloc(s) pvPortMalloc(s)
#define free(p) vPortFree(p)

#ifndef SRC_LIBWRAP_C_

#define printf(...)    _uwipmc_printf(__VA_ARGS__)
#define sprintf(...)   _uwipmc_sprintf(__VA_ARGS__)
#define snprintf(...)  _uwipmc_snprintf(__VA_ARGS__)

#define vprintf(...)   _uwipmc_vprintf(__VA_ARGS__)
#define vsprintf(...)  _uwipmc_vsprintf(__VA_ARGS__)
#define vsnprintf(...) _uwipmc_vsnprintf(__VA_ARGS__)

#endif

int _uwipmc_printf(const const char *format, ...) __attribute__((format(printf,1,2)));
int _uwipmc_sprintf(char *str, const char *format, ...) __attribute__((format(printf,2,3)));
int _uwipmc_snprintf(char *str, size_t size, const char *format, ...) __attribute__((format(printf,3,4)));
int _uwipmc_vprintf(const char *format, va_list ap) __attribute__((format(printf,1,0)));
int _uwipmc_vsprintf(char *str, const char *format, va_list ap) __attribute__((format(printf,2,0)));
int _uwipmc_vsnprintf(char *str, size_t size, const char *format, va_list ap) __attribute__((format(printf,3,0)));

#endif /* SRC_LIBWRAP_H_ */
