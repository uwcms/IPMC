#include "printf.h"

#include <stdarg.h>
#include <cxxabi.h> // for cxa_demangle()
#include <alloca.h>
#include <FreeRTOS.h>
#include <semphr.h>
#include <libs/ThreadingPrimitives.h>

/**
 * Mirror sprintf() but returning a std::string.
 */
std::string stdsprintf(const char *fmt, ...) {
    va_list va;
    va_list va2;
    va_start(va, fmt);
    va_copy(va2, va);
    size_t s = vsnprintf(NULL, 0, fmt, va) + 1;
    char *str;
    if (s <= 128)
    	str = (char*)alloca(s);
    else
    	str = (char*)malloc(s);
    vsnprintf(str, s, fmt, va2);
    va_end(va);
    va_end(va2);
    std::string ret(str);
    if (s > 128)
    	free(str);
    return ret;
}


/**
 * Modify a std::string in place to contain \r\n where it currently contains \n.
 * @param input The string to modify.
 * @param nlchar The newline character to detect, if not '\n'.
 */
void windows_newline(std::string &input, char nlchar) {
	for (unsigned int i = 0; i < input.size(); ++i) {
		if (input[i] == nlchar) {
			input.replace(i, 1, "\r\n");
			i++; // We don't want to hit that \n again on the (now) next character.
		}
	}
}

extern volatile SemaphoreHandle_t stdlib_mutex; // From libwrap.cc
void init_stdlib_mutex(); // From libwrap.cc

/**
 * This function calls the cross-vendor C++ Application Binary Interface to
 * demangle a C++ type name.
 *
 * @param name The name to be demangled, perhaps returned from typeid(T).name()
 * @return The demangled form of name, or name if an error occurred.
 */
std::string cxa_demangle(const char *name) {
	safe_init_static_mutex(stdlib_mutex, false);
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
