/*
 * BackTrace.cpp
 *
 *  Created on: Nov 7, 2018
 *      Author: mpv
 */

#include <FreeRTOS.h>
#include <libs/BackTrace.h>
#include <semphr.h>
#include <map>
#include <unwind.h> // GCC's internal unwinder, part of newlib
#include "unwind-cxx.h" // Extra utilities copied from GCC source code, normally hidden

#include <libs/ThreadingPrimitives.h>

// Based on:
// http://infocenter.arm.com/help/topic/com.arm.doc.ihi0038b/IHI0038B_ehabi.pdf
// https://code.woboq.org/userspace/glibc/debug/backtrace.c.html
// https://elixir.bootlin.com/linux/latest/source/arch/arm/kernel/unwind.c
// https://github.com/red-rocket-computing/backtrace

#if !defined (__ARM_EABI__)
#warning Your compiler does not have EABI support.
#warning ARM unwind is known to compile only with EABI compilers.
#warning Change compiler or disable ARM_UNWIND option.
#endif

// Global variables used to track exception traces.
static std::map<void*, std::shared_ptr<BackTrace>>* __exception_backtrace_log = nullptr;
static StaticSemaphore_t __eb_mutex_buffer;
static SemaphoreHandle_t __eb_mutex = nullptr;

static void init_backtrace_mutex() {
	if (__eb_mutex)
		return;
	taskENTER_CRITICAL();
	if (__eb_mutex) {
		// Another thread took over
		taskEXIT_CRITICAL();
		return;
	}

	__eb_mutex = xSemaphoreCreateMutexStatic(&__eb_mutex_buffer);

	taskEXIT_CRITICAL();
}

struct unwind_idx {
	unsigned long addr_offset;	///< prel31
	unsigned long insn;			///< unwind instruction
};

// __exidx_start and __exidx_end defined in the linker.
extern const struct unwind_idx __exidx_start[];
extern const struct unwind_idx __exidx_end[];

/**
 * Converts a prel31 offset to absolute address.
 * prel31 represents the offset from the unwind index pointer to the
 * corresponding instruction with bit 31 cleared.
 * @param ptr Pointer from the unwind index prel31 offset.
 */
inline unsigned long prel31_to_addr(const unsigned long* ptr) {
	// Sign-extend to 32 bits (complement 2).
	long offset = (((long)*(ptr)) << 1) >> 1;
	return (unsigned long)(ptr) + offset;
}

/**
 * Search for the unwind index based on instruction pointer.
 * @param addr Instruction address, doesn't need to be function start.
 * @param start Unwind index start address.
 * @param stop Unwind index end address.
 * @return Index corresponding to the function holding addr or nullptr.
 */
static const struct unwind_idx *search_index(unsigned long addr,
				       const struct unwind_idx *start,
				       const struct unwind_idx *stop)
{
	unsigned long addr_prel31;

	// prel31 for address relative to start.
	addr_prel31 = (addr - (unsigned long)start) & 0x7fffffff;

	// Do a binary search
	while (start < stop - 1) {
		const struct unwind_idx *mid = start + ((stop - start) >> 1);

		// As addr_prel31 is relative to start an offset is needed to make it relative to mid.
		if (addr_prel31 - ((unsigned long)mid - (unsigned long)start) < mid->addr_offset)
			stop = mid;
		else {
			// Keep addr_prel31 relative to start.
			addr_prel31 -= ((unsigned long)mid - (unsigned long)start);
			start = mid;
		}
	}

	if (start->addr_offset <= addr_prel31)
		return start;
	else {
		return nullptr;
	}
}

/**
 * Fetches the function name from the function address.
 * @param address Target function address.
 * @return Static pointer to the function name (mangled for C++), nullptr otherwise.
 * @note -mpoke-function-name must be present for it to work.
 */
static const char *get_function_name(unsigned long address)
{
	// If -mpoke-function-name is used during compilating then two items will be added
	// right before the function start:
	// 1. A marker (0xFFxxxxxx) indicating there is a name present, where xxxxxx is
	//    the number of bytes in the name.
	// 2. The actual name right before the marker.
	// Using both of these fields it is possible to check if there is a function
	// name present and fetch it.

	unsigned long marker = *(unsigned long *)(address - 4);

	if ((marker & 0xff000000) == 0xff000000) {
		return (const char *)(address - 4 - (marker & 0x00ffffff));
	}

	return nullptr;
}

/**
 * Return the function name and address associated to provided instruction address.
 * @param pc Instruction address to get the symbol name.
 * @param fptr Used to return the function start address, useful for offset calculation. Can be nullptr assigned.
 * @return Null-terminated string or nullptr if no name available
 */
const char *backtrace_symbol_info(unsigned long pc, unsigned long* faddr)
{
	// Fetch the index so that the function address can be retrieved.
	const struct unwind_idx *index = search_index(pc, __exidx_start, __exidx_end);
	if (!index) {
		return nullptr;
	}

	// Translate prel31 to function address.
	unsigned int func_addr = prel31_to_addr(&index->addr_offset);
	if (faddr) *faddr = func_addr;

	// Get the function name using the function address.
	return get_function_name(func_addr);
}

extern "C" {

struct trace_arg
{
	void **array;
	_Unwind_Word cfa;
	int cnt;
	int size;
};

static _Unwind_Reason_Code backtrace_helper (struct _Unwind_Context *ctx, void *a)
{
	struct trace_arg *arg = (struct trace_arg*)a;
	/* We are first called with address in the __backtrace function. Skip it.  */
	if (arg->cnt != -1)
	{
		arg->array[arg->cnt] = (void *) _Unwind_GetIP (ctx);
		/* Check whether we make any progress.  */
		_Unwind_Word cfa = _Unwind_GetCFA (ctx);
		if (arg->cnt > 0 && arg->array[arg->cnt - 1] == arg->array[arg->cnt] && cfa == arg->cfa)
			return _URC_END_OF_STACK;
		arg->cfa = cfa;
	}
	if (++arg->cnt == arg->size)
		return _URC_END_OF_STACK;
	return _URC_NO_REASON;
}

/**
 * Backtrace the current call stack.
 * @param array Pointer array to store unwinded stack.
 * @param size The maximum number items that array can hold.
 * @return The total number of calls unwinded.
 */
int backtrace (void **array, int size)
{
	struct trace_arg arg = { array, 0, -1, size };
	if (size <= 0)
		return 0;
	_Unwind_Backtrace(backtrace_helper, &arg);
	/* _Unwind_Backtrace seems to put NULL address above _start.  Fix it up here.  */
	if (arg.cnt > 1 && arg.array[arg.cnt - 1] == NULL)
		--arg.cnt;
	return arg.cnt != -1 ? arg.cnt : 0;
}

void __real___cxa_throw(void *ex, void *info, void (*dest)(void *));

void __wrap___cxa_throw(void *ex, void *info, void (*dest)(void *)) {
	configASSERT(!IN_INTERRUPT()); // No throws allowed in interrupts - sorry!

	std::shared_ptr<BackTrace> trace(new BackTrace());

	trace->count = backtrace((void**)&(trace->frames), BackTrace::MAX_TRACE_DEPTH);

	// trace->name will get deleted when BackTrace gets destroyed
	int status;
	trace->name = abi::__cxa_demangle(reinterpret_cast<const std::type_info*>(info)->name(), 0, 0, &status);
	if (status != 0) {
		trace->name = NULL;
	}

	init_backtrace_mutex();
	xSemaphoreTake(__eb_mutex, portMAX_DELAY);
	// Add the new trace to the log.
	if (!__exception_backtrace_log)
		__exception_backtrace_log = new std::map<void*, std::shared_ptr<BackTrace>>();

	(*__exception_backtrace_log)[ex] = trace;
	xSemaphoreGive(__eb_mutex);

	__real___cxa_throw(ex,info,dest);
}

void __real___cxa_free_exception(void *ex);

void __wrap___cxa_free_exception(void *ex) {
	xSemaphoreTake(__eb_mutex, portMAX_DELAY);
	// The exception is no longer in play, remove it.
	__exception_backtrace_log->erase(ex);
	xSemaphoreGive(__eb_mutex);

	__real___cxa_free_exception(ex);
}

} /* !extern "C" */

BackTrace* BackTrace::traceException(void *ex) {
	if (!ex) {
		// No exception pointer provided, attempt to get current exception info.
		abi::__cxa_eh_globals *globals = abi::__cxa_get_globals();
		if (!globals) return nullptr;

		abi::__cxa_exception *header = globals->caughtExceptions;
		if (!header) return nullptr;

		ex = abi::__get_object_from_ambiguous_exception(header);
		if (!ex) return nullptr;
	}

	BackTrace* r = nullptr;

	init_backtrace_mutex();
	xSemaphoreTake(__eb_mutex, portMAX_DELAY);

	// Find the trace record.
	if (!__exception_backtrace_log) {
		__exception_backtrace_log = new std::map<void*, std::shared_ptr<BackTrace>>();
	} else {
		auto record = __exception_backtrace_log->find(ex);
		if (record != __exception_backtrace_log->end()) {
			std::shared_ptr<BackTrace> trace = record->second;
			r = &*trace;
		}
	}

	xSemaphoreGive(__eb_mutex);
	return r;
}

const std::string BackTrace::toString() const {
	constexpr size_t BUFFER_SIZE = 512;

	int status = 0;
	char *buffer = new char[BUFFER_SIZE];
	std::string str = "";

	for ( size_t i = 0 ; i < this->count ; i++ ) {
		const char *name = nullptr;
		unsigned long faddr = 0;

		// Attempt to map the frame's PC to a function name and demangle it.
		const char *mangled_name = backtrace_symbol_info((uint32_t)this->frames[i], &faddr);
		char *demangled_name = abi::__cxa_demangle(mangled_name, 0, 0, &status);

		if (demangled_name) name = demangled_name;
		else if (mangled_name) name = mangled_name;
		else name = "???";

		unsigned long iaddr = (unsigned long)this->frames[i] - sizeof(unsigned long);
		unsigned long offset = iaddr - faddr;

		snprintf(buffer, BUFFER_SIZE, "%d: [0x%08X] %s+0x%X\n", i, (unsigned int)iaddr, name, (unsigned int)offset);

		str += buffer;

		// We are responsible for freeing mangled_name
		if (demangled_name) free(demangled_name);
	}

	delete buffer;
	return str;
}
