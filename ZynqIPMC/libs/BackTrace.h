/*
 * BackTrace.h
 *
 *  Created on: Nov 7, 2018
 *      Author: mpv
 */

#ifndef SRC_COMMON_UW_IPMC_LIBS_BACKTRACE_H_
#define SRC_COMMON_UW_IPMC_LIBS_BACKTRACE_H_

// -funwind-tables is necessary to properly unwind the stack using BackTrace
// -mpoke-function-name is necessary to properly associate function names

#include <memory>

extern "C" {

int backtrace(void **array, int size);

void __wrap___cxa_throw(void *ex, void *info, void (*dest)(void *));
void __wrap___cxa_free_exception(void *ex);

} /* !extern "C" */

struct BackTrace final {
	static constexpr size_t MAX_TRACE_DEPTH = 20;

	BackTrace() : count(0), name(NULL) {};
	~BackTrace() { if (this->name) free(name); };

	inline void trace() {
		this->count = backtrace((void**)&(this->frames), BackTrace::MAX_TRACE_DEPTH);
	}

	/**
	 * Attempt to trace an existing or the current exception.
	 * Exceptions traces are internally tracked and can be fetched using the exception pointer.
	 * If no pointer is provided then traceException will try to get the trace of the exception being thrown.
	 * However, if no exception is being thrown then nullptr will be returned.
	 * Trace records of exceptions are automatically erased after the try/catch block.
	 * @param ex Pointer to the exception object.
	 * @return nullptr if no valid back trace is available or a fully valid trace.
	 * @note Thread-safe.
	 */
	static BackTrace* traceException(void *ex = nullptr);

	///! Generates a multi-line string where each line represents a frame of the stack trace.
	const std::string toString() const;

	///! Returns the name associate to the trace, if any.
	const std::string getName() const {
		if (this->name) {
			return std::string(this->name);
		} else {
			return std::string("unknown");
		}
	}

	void* frames[MAX_TRACE_DEPTH];
	size_t count;
	char* name;
};


#endif /* SRC_COMMON_UW_IPMC_LIBS_BACKTRACE_H_ */
