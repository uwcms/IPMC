/*
 * PSWDT.h
 *
 *  Created on: Feb 1, 2018
 *      Author: jtikalsky
 */

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_WATCHDOG_PSWDT_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_WATCHDOG_PSWDT_H_

#include <libs/LogTree.h>
#include <stdint.h>
#include <xil_types.h>
#include "xwdtps.h"

/**
 * A watchdog driver using the Zynq PS SWDT.
 */
class PS_WDT {
public:
	PS_WDT(u32 DeviceId, u8 num_slots, LogTree &log);
	virtual ~PS_WDT();

	typedef uint32_t slot_handle_t; ///< A type for watchdog slot handles.
	slot_handle_t register_slot(uint32_t lifetime);
	void activate_slot(slot_handle_t slot_handle);
	void deactivate_slot(slot_handle_t slot_handle, uint32_t deactivate_code);
	void service_slot(slot_handle_t slot_handle);
	static const volatile uint32_t deactivate_code_lshifted1; ///< The deactivate code, left-shifted one bit.

protected:
	LogTree &log; ///< A log facility for critical alerts.

	/// A structure defining a WDT slot.
	struct wdtslot {
		volatile uint32_t enabled; ///< UINT32_MAX if enabled, 0 if disabled, any other value is fatal.
		volatile uint32_t lifetime; ///< The lifetime of a watchdog update, in ticks.
		volatile uint64_t config_cksum; ///< (~((enabled<<32)|lifetime)) ^ slot_cksum_key
		volatile uint64_t timeout; ///< The current timeout of this watchdog as a tick64 (last update + lifetime)
		volatile uint64_t timeout_cksum; ///< (~timeout) ^ slot_cksum_key
		volatile char last_serviced_by[configMAX_TASK_NAME_LEN]; ///< The last task servicing this slot.
	};

	u8 num_slots; ///< The number of slots supported by this PS_WDT.
	u8 free_slot; ///< The index of the next free slot.
	struct wdtslot *slots; ///< A heap-allocated array of num_slots wdtslot structures.

	static const volatile uint64_t slotkey_lshifted1; ///< The key component of slot_cksum, left-shifted one bit.
	volatile uint64_t *heap_slotkey_rshifted1; ///< The key component of slot_cksum, right-shifted one bit.
	volatile uint32_t global_canary; ///< If this value does not match the canary value, the WDT will never be reset.
	XWdtPs wdt; ///< A watchdog timer instance.

public:
	void _run_thread(); ///< \protected Internal
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_WATCHDOG_PSWDT_H_ */
