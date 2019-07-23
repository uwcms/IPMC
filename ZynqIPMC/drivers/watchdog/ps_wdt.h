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

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_WATCHDOG_PSWDT_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_WATCHDOG_PSWDT_H_

// Only include driver if PS WDT in the BSP.
#if XSDK_INDEXING || __has_include("xwdtps.h")

#include "xwdtps.h"
#include <stdint.h>
#include <xil_types.h>
#include <functional>
#include <libs/logtree/logtree.h>

/**
 * A watchdog driver using the Zynq PS SWDT.
 *
 * Tasks can request a slot that they then need to service.
 * If one of those tasks fails to service the watchdog in time
 * (e.g. due to a driver hang), then the watchdog will trip.
 */
class PSWDT final {
public:
	/**
	 * Create and initialize the watchdog timer.
	 * @param device_id Watchdog device ID, normally XPAR_PS7_WDT_0_DEVICE_ID. Check xparameters.h.
	 * @param num_slots Number of service slots provided by this watchdog.
	 * @param log Log facility for reports when the watchdog trips.
	 * @param on_trip Function/labda that runs when the watchdog trips.
	 * @throw except::hardware_error if failed to initialize low-level driver.
	 */
	PSWDT(uint16_t device_id, size_t num_slots, LogTree &log, std::function<void(void)> on_trip = nullptr);
	virtual ~PSWDT();

	//! Type for watchdog slot handles.
	typedef uint32_t slot_handle_t;

	/**
	 * Register a free watchdog slot with the specified lifetime.
	 * @param lifetime The timeout period for this slot in ticks.
	 * @return A (inactive) slot handle.
	 * @throw std::domain_error if there are no more slots available.
	 */
	slot_handle_t register_slot(uint32_t lifetime);

	/**
	 * Enable and service the provided watchdog slot.
	 * @param slot_handle The slot to enable.
	 * @throw std::out_of_range if slot is not in range.
	 */
	void activate_slot(slot_handle_t slot_handle);

	/**
	 * Disable the provided watchdog slot.
	 * @param slot_handle The slot to disable
	 * @param deactivate_code Provide PS_WDT::deactivate_code_lshifted1>>1
	 * @throw std::out_of_range if slot is not in range.
	 */
	void deactivate_slot(slot_handle_t slot_handle, uint32_t deactivate_code);

	/**
	 * Service the watchdog for a provided slot.
	 * @param slot_handle The watchdog slot to service.
	 * @throw std::out_of_range if slot is not in range.
	 */
	void service_slot(slot_handle_t slot_handle);

	//! The deactivate code, left-shifted one bit.
	static const volatile uint32_t deactivate_code_lshifted1;

private:
	void backgroundTask(); ///< Internal task to keep an eye on the watchdog.

	LogTree &log; ///< A log facility for critical alerts.

	std::function<void(void)> on_trip;	///< A function running when the watchdog is tripped.

	//! A structure defining a WDT slot.
	struct wdtslot {
		volatile uint32_t enabled;			///< UINT32_MAX if enabled, 0 if disabled, any other value is fatal.
		volatile uint32_t lifetime;			///< The lifetime of a watchdog update, in ticks.
		volatile uint64_t config_cksum;		///< (~((enabled<<32)|lifetime)) ^ slot_cksum_key.
		volatile uint64_t timeout;			///< The current timeout of this watchdog as a tick64 (last update + lifetime).
		volatile uint64_t timeout_cksum;	///< (~timeout) ^ slot_cksum_key.
		volatile char last_serviced_by[configMAX_TASK_NAME_LEN]; ///< The last task servicing this slot.
	};

	const size_t kNumSlots;	///< The number of slots supported by this PS_WDT.
	size_t free_slot;		///< The index of the next free slot.
	struct wdtslot *slots;	///< A heap-allocated array of num_slots wdtslot structures.

	static const volatile uint64_t slotkey_lshifted1;	///< The key component of slot_cksum, left-shifted one bit.
	volatile uint64_t *heap_slotkey_rshifted1;			///< The key component of slot_cksum, right-shifted one bit.
	volatile uint32_t global_canary;					///< If this value does not match the canary value, the WDT will never be reset.
	XWdtPs wdt;											///< A watchdog timer instance.
};

#endif

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_WATCHDOG_PSWDT_H_ */
