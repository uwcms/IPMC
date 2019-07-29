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

// Only include driver if PS WDT in the BSP.
#if XSDK_INDEXING || __has_include("xwdtps.h")

#include "ps_wdt.h"
#include <IPMC.h>
#include <FreeRTOS.h>
#include <task.h>
#include <libs/threading.h>
#include <libs/printf.h>
#include <libs/except.h>
#include <string>

const static volatile uint32_t global_canary_lshifted1 = 0x87d64518;
#define GLOBAL_CANARY_RSHIFTED1 0x21f59146

PSWDT::PSWDT(uint16_t device_id, size_t num_slots, LogTree &log, std::function<void(void)> on_trip) :
log(log), on_trip(on_trip), kNumSlots(num_slots), free_slot(0) {
	this->slots = new struct wdtslot[num_slots];
	this->heap_slotkey_rshifted1 = new volatile uint64_t;
	*this->heap_slotkey_rshifted1 = PSWDT::slotkey_lshifted1 >> 2;
	this->global_canary = global_canary_lshifted1>>1;

	XWdtPs_Config *config = XWdtPs_LookupConfig(device_id);
	if (!config) {
		throw except::hardware_error("Unable to locate PS watchdog device");
	}

	if (XST_SUCCESS != XWdtPs_CfgInitialize(&this->wdt, config, config->BaseAddress)) {
		throw except::hardware_error("Unable to configure PS watchdog device");
	}

	for (uint8_t i = 0; i < this->kNumSlots; ++i) {
		struct wdtslot &slot = this->slots[i];
		slot.enabled = 0;
		slot.config_cksum = ((~((static_cast<uint64_t>(slot.enabled)<<32)|static_cast<uint64_t>(slot.lifetime))) ^ (slotkey_lshifted1>>1));
		slot.timeout_cksum = ((~slot.timeout) ^ (slotkey_lshifted1>>1));
		slot.last_serviced_by[0] = '\0'; // null string.
	}

	runTask("watchdog", TASK_PRIORITY_WATCHDOG, [this]() -> void { this->backgroundTask(); });
}

PSWDT::~PSWDT() {
	configASSERT(0); // Unsupported, as we don't have a mechanism to trigger thread deletion.

	// Well, I hope you deactivated us first, or this is going to go poorly for you.
	*this->heap_slotkey_rshifted1 = 0;
	delete this->heap_slotkey_rshifted1;
	memset(this->slots, 0, this->kNumSlots * sizeof(struct wdtslot));
	delete [] this->slots;
}

void PSWDT::backgroundTask() {
	// We really don't want this interrupted.
	CriticalGuard critical(true);

	/* Default clock is CPU_1x.  According to Vivado the WDT clock is: Requested=133.333Mhz, Actual=111.111Mhz
	 *
	 * We'll use a 5 second timeout here.
	 *
	 * 0x20fff*4096 / CPU1x = 4.98 seconds.
	 */
	XWdtPs_SetControlValue(&this->wdt, (uint8_t)XWDTPS_COUNTER_RESET, (uint8_t)0x20);
	XWdtPs_SetControlValue(&this->wdt, (uint8_t)XWDTPS_CLK_PRESCALE, (uint8_t)XWDTPS_CCR_PSCALE_4096);

	XWdtPs_EnableOutput(&this->wdt, XWDTPS_RESET_SIGNAL);
	XWdtPs_Start(&this->wdt);
	XWdtPs_RestartWdt(&this->wdt);

	critical.release();

	while (true) {
		vTaskDelay(configTICK_RATE_HZ); // On a five second reset, we can service once per second.

		uint64_t now64 = get_tick64();
		for (uint8_t i = 0; i < this->kNumSlots; ++i) {
			struct wdtslot &slot = this->slots[i];

			if (slot.config_cksum != ((~((static_cast<uint64_t>(slot.enabled)<<32)|static_cast<uint64_t>(slot.lifetime))) ^ (slotkey_lshifted1>>1))) {
				if (this->global_canary) {
					this->global_canary = 0; // Break.
					slot.last_serviced_by[configMAX_TASK_NAME_LEN-1] = '\0'; // Terminate, just in case.
					this->log.log(stdsprintf("WATCHDOG MEMORY CORRUPTED: Slot %hhu (%s) config_cksum mismatch.  WATCHDOG SERVICE DISABLED.", i, slot.last_serviced_by), LogTree::LOG_CRITICAL);
				}
			}

			if (slot.timeout_cksum != ((~slot.timeout) ^ (slotkey_lshifted1>>1))) {
				if (this->global_canary) {
					this->global_canary = 0; // Break.
					slot.last_serviced_by[configMAX_TASK_NAME_LEN-1] = '\0'; // Terminate, just in case.
					this->log.log(stdsprintf("WATCHDOG MEMORY CORRUPTED: Slot %hhu (%s) timeout_cksum mismatch.  WATCHDOG SERVICE DISABLED.", i, slot.last_serviced_by), LogTree::LOG_CRITICAL);
				}
			}

			if (slot.enabled == 0)
				continue;

			if (slot.enabled != UINT32_MAX) {
				if (this->global_canary) {
					this->global_canary = 0; // Break.
					slot.last_serviced_by[configMAX_TASK_NAME_LEN-1] = '\0'; // Terminate, just in case.
					this->log.log(stdsprintf("WATCHDOG MEMORY CORRUPTED: Slot %hhu (%s) enable value invalid.  WATCHDOG SERVICE DISABLED.", i, slot.last_serviced_by), LogTree::LOG_CRITICAL);
				}
			}

			if (slot.timeout < now64) {
				if (this->global_canary) {
					this->global_canary = 0; // Break.
					slot.last_serviced_by[configMAX_TASK_NAME_LEN-1] = '\0'; // Terminate, just in case.
					this->log.log(stdsprintf("WATCHDOG TIMEOUT EXPIRED: Slot %hhu (%s) watchdog has expired.  WATCHDOG SERVICE DISABLED.", i, slot.last_serviced_by), LogTree::LOG_CRITICAL);
				}
			}
		}

		if (this->global_canary == (global_canary_lshifted1>>1) && this->global_canary == (GLOBAL_CANARY_RSHIFTED1<<1)) {
			XWdtPs_RestartWdt(&this->wdt);
		} else {
			if (this->on_trip)
				this->on_trip();

			volatile bool suspend = true;

			while (suspend) {
				// Done.
			}
		}
	}
}

PSWDT::slot_handle_t PSWDT::register_slot(uint32_t lifetime) {
	CriticalGuard critical(true);

	if (this->free_slot >= this->kNumSlots) {
		throw std::domain_error("No free PSWDT slots.");
	}

	uint8_t slotid = this->free_slot++;
	struct wdtslot &slot = this->slots[slotid];
	slot.enabled = 0;
	slot.lifetime = lifetime;
	slot.config_cksum = ((~((static_cast<uint64_t>(slot.enabled)<<32)|static_cast<uint64_t>(slot.lifetime))) ^ (slotkey_lshifted1>>1));
	slot.timeout_cksum = ((~slot.timeout) ^ (slotkey_lshifted1>>1));

	return 0x80000000|(slotid<<24)|((~slotid)<<16)|slotid;
}

void PSWDT::activate_slot(slot_handle_t slot_handle) {
	uint8_t slotid = slot_handle & 0xff;

	if (slot_handle != (0x80000000|(slotid<<24)|((~slotid)<<16)|slotid)) { // Valid?
		throw std::out_of_range(std::string("Invalid PSWDT slot: ") + std::to_string(slotid));
	}

	CriticalGuard critical(true);

	struct wdtslot &slot = this->slots[slotid];
	if (slot.config_cksum != ((~((static_cast<uint64_t>(slot.enabled)<<32)|static_cast<uint64_t>(slot.lifetime))) ^ (slotkey_lshifted1>>1))) {
		if (this->global_canary) {
			this->global_canary = 0; // Break.
			slot.last_serviced_by[configMAX_TASK_NAME_LEN-1] = '\0'; // Terminate, just in case.
			this->log.log(stdsprintf("WATCHDOG MEMORY CORRUPTED: Slot %hhu (%s) config_cksum mismatch.  WATCHDOG SERVICE DISABLED.", slotid, slot.last_serviced_by), LogTree::LOG_CRITICAL);
		}
	}

	slot.enabled = UINT32_MAX;
	slot.config_cksum = ((~((static_cast<uint64_t>(slot.enabled)<<32)|static_cast<uint64_t>(slot.lifetime))) ^ (slotkey_lshifted1>>1));
	slot.timeout = get_tick64() + slot.lifetime;
	slot.timeout_cksum = ((~slot.timeout) ^ (slotkey_lshifted1>>1));
	strncpy(const_cast<char*>(slot.last_serviced_by), pcTaskGetName(NULL), configMAX_TASK_NAME_LEN);
	slot.last_serviced_by[configMAX_TASK_NAME_LEN-1] = '\0'; // Terminate, just in case.

	critical.release();

	this->log.log(stdsprintf("Watchdog slot %hhu activated by %s.", slotid, slot.last_serviced_by), LogTree::LOG_INFO);
}

void PSWDT::deactivate_slot(slot_handle_t slot_handle, uint32_t deactivate_code) {
	uint8_t slotid = slot_handle & 0xff;

	if (slot_handle != (0x80000000|(slotid<<24)|((~slotid)<<16)|slotid)) { // Valid?
		throw std::out_of_range(std::string("Invalid PSWDT slot: ") + std::to_string(slotid));
	}

	CriticalGuard critical(true);

	struct wdtslot &slot = this->slots[slotid];
	if (deactivate_code != (deactivate_code_lshifted1>>1)) {
		this->global_canary = 0; // Break.
		slot.last_serviced_by[configMAX_TASK_NAME_LEN-1] = '\0'; // Terminate, just in case.
		this->log.log(stdsprintf("WATCHDOG ILLEGAL DISABLE: Slot %hhu (%s) deactivate_code invalid.  WATCHDOG SERVICE DISABLED.", slotid, slot.last_serviced_by), LogTree::LOG_CRITICAL);
	}

	if (slot.config_cksum != ((~((static_cast<uint64_t>(slot.enabled)<<32)|static_cast<uint64_t>(slot.lifetime))) ^ (slotkey_lshifted1>>1))) {
		if (this->global_canary) {
			this->global_canary = 0; // Break.
			slot.last_serviced_by[configMAX_TASK_NAME_LEN-1] = '\0'; // Terminate, just in case.
			this->log.log(stdsprintf("WATCHDOG MEMORY CORRUPTED: Slot %hhu (%s) config_cksum mismatch.  WATCHDOG SERVICE DISABLED.", slotid, slot.last_serviced_by), LogTree::LOG_CRITICAL);
		}
	}

	slot.enabled = 0;
	slot.config_cksum = ((~((static_cast<uint64_t>(slot.enabled)<<32)|static_cast<uint64_t>(slot.lifetime))) ^ (slotkey_lshifted1>>1));
	strncpy(const_cast<char*>(slot.last_serviced_by), pcTaskGetName(NULL), configMAX_TASK_NAME_LEN);
	slot.last_serviced_by[configMAX_TASK_NAME_LEN-1] = '\0'; // Terminate, just in case.

	critical.release();

	this->log.log(stdsprintf("Watchdog slot %hhu deactivated by %s.", slotid, pcTaskGetName(NULL)), LogTree::LOG_INFO);
}

void PSWDT::service_slot(slot_handle_t slot_handle) {
	uint8_t slotid = slot_handle & 0xff;

	if (slot_handle != (0x80000000|(slotid<<24)|((~slotid)<<16)|slotid)) { // Valid?
		throw std::out_of_range(std::string("Invalid PSWDT slot: ") + std::to_string(slotid));
	}

	CriticalGuard critical(true);

	struct wdtslot &slot = this->slots[slotid];
	if (slot.config_cksum != ((~((static_cast<uint64_t>(slot.enabled)<<32)|static_cast<uint64_t>(slot.lifetime))) ^ (slotkey_lshifted1>>1))) {
		if (this->global_canary) {
			this->global_canary = 0; // Break.
			slot.last_serviced_by[configMAX_TASK_NAME_LEN-1] = '\0'; // Terminate, just in case.
			this->log.log(stdsprintf("WATCHDOG MEMORY CORRUPTED: Slot %hhu (%s) config_cksum mismatch.  WATCHDOG SERVICE DISABLED.", slotid, slot.last_serviced_by), LogTree::LOG_CRITICAL);
		}
	}

	if (slot.timeout_cksum != ((~slot.timeout) ^ (slotkey_lshifted1>>1))) {
		if (this->global_canary) {
			this->global_canary = 0; // Break.
			slot.last_serviced_by[configMAX_TASK_NAME_LEN-1] = '\0'; // Terminate, just in case.
			this->log.log(stdsprintf("WATCHDOG MEMORY CORRUPTED: Slot %hhu (%s) timeout_cksum mismatch.  WATCHDOG SERVICE DISABLED.", slotid, slot.last_serviced_by), LogTree::LOG_CRITICAL);
		}
	}

	slot.timeout = get_tick64() + slot.lifetime;
	slot.timeout_cksum = ((~slot.timeout) ^ (slotkey_lshifted1>>1));
	strncpy(const_cast<char*>(slot.last_serviced_by), pcTaskGetName(NULL), configMAX_TASK_NAME_LEN);
	slot.last_serviced_by[configMAX_TASK_NAME_LEN-1] = '\0'; // Terminate, just in case.

	critical.release();

	this->log.log(stdsprintf("Watchdog slot %hhu serviced by %s.", slotid, pcTaskGetName(NULL)), LogTree::LOG_DIAGNOSTIC);
}

const volatile uint32_t PSWDT::deactivate_code_lshifted1 = 0x508030a4UL;
const volatile uint64_t PSWDT::slotkey_lshifted1 = 0x9b0b3beee931a24ULL; ///< The key component of slot_cksum, left-shifted one bit.

#endif
