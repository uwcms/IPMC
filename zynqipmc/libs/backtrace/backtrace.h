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
 * This library provides backtrace support during runtime execution and
 * also logging of traces during exceptions.
 *
 * Check BackTrace::traceException for exceptions and Backtrace::trace
 * for regular code tracing.
 *
 * The code needs to be compiled with -funwind-tables so that the executable
 * has the unwind tables embedded and with -mpoke-function-name to keep function
 * names. Both will increase the size of the elf file.
 *
 * This code is very specialized and uses a lot of hidden features from GCC and
 * ARM EABI, it is likely not portable and won't work with other compilers.
 */

#ifndef SRC_COMMON_ZYNQIPMC_LIBS_BACKTRACE_H_
#define SRC_COMMON_ZYNQIPMC_LIBS_BACKTRACE_H_

#include <memory>

extern "C" {
int backtrace(void **array, int size);

void __wrap___cxa_throw(void *ex, void *info, void (*dest)(void *));
void __wrap___cxa_free_exception(void *ex);
} /* !extern "C" */

struct BackTrace final {
	static constexpr size_t MAX_TRACE_DEPTH = 20;

	BackTrace() : count(0), name(nullptr) {};
	~BackTrace() { if (this->name) free(name); };

	/**
	 * Takes a snapshot of the current call stack. Use BackTrace::toString to print it.
	 */
	inline void trace() {
		this->count = backtrace((void**)&(this->frames), BackTrace::MAX_TRACE_DEPTH);
	}

	/**
	 * Attempt to trace an existing or the current exception.
	 * Exceptions traces are internally tracked and can be fetched using the exception pointer.
	 * If no pointer is provided then traceException will try to get the trace of the exception being thrown.
	 * However, if no exception is being thrown then nullptr will be returned.
	 * Trace records of exceptions are automatically erased after the try/catch block.
	 *
	 * @param ex Pointer to the exception object.
	 * @return nullptr if no valid back trace is available or a fully valid trace.
	 * @note Thread-safe.
	 */
	static BackTrace* traceException(void *ex = nullptr);

	//! Generates a multi-line string where each line represents a frame of the stack trace.
	const std::string toString() const;

	//! Returns the name associate to the trace, if any.
	const std::string getName() const {
		if (this->name) {
			return std::string(this->name);
		} else {
			return std::string("unknown");
		}
	}

	void* frames[MAX_TRACE_DEPTH];	///< Call stack frames of this trace.
	size_t count;					///< Number of frame entries.
	char* name;						///< Trace name.
};


#endif /* SRC_COMMON_ZYNQIPMC_LIBS_BACKTRACE_H_ */
