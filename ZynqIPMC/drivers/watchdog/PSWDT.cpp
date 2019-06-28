/*
 * PSWDT.cpp
 *
 *  Created on: Feb 1, 2018
 *      Author: jtikalsky
 */

#include <drivers/watchdog/PSWDT.h>
#include <IPMC.h>
#include <libs/ThreadingPrimitives.h>
#include <libs/printf.h>
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
#include <libs/except.h>

const static volatile uint32_t global_canary_lshifted1 = 0x87d64518;
#define GLOBAL_CANARY_RSHIFTED1 0x21f59146

/**
 * Instantiate and start a PS_WDT.
 *
 * @param DeviceId The DeviceID of the XWDTPS
 * @param num_slots The number of service slots provided by this watchdog
 * @param log A log facility for when things go VERY wrong
 */
PS_WDT::PS_WDT(u32 DeviceId, u8 num_slots, LogTree &log, std::function<void(void)> on_trip)
	: log(log), on_trip(on_trip), num_slots(num_slots), free_slot(0) {
	this->slots = (struct wdtslot*)malloc(num_slots * sizeof(struct wdtslot));
	this->heap_slotkey_rshifted1 = (volatile uint64_t*)malloc(sizeof(uint64_t));
	*this->heap_slotkey_rshifted1 = PS_WDT::slotkey_lshifted1 >> 2;
	this->global_canary = global_canary_lshifted1>>1;

	XWdtPs_Config *Config = XWdtPs_LookupConfig(DeviceId);
	if (!Config)
		throw except::hardware_error("Unable to locate PS watchdog device");
	if (XST_SUCCESS != XWdtPs_CfgInitialize(&this->wdt, Config, Config->BaseAddress))
		throw except::hardware_error("Unable to configure PS watchdog device");

	for (u8 i = 0; i < this->num_slots; ++i) {
		struct wdtslot &slot = this->slots[i];
		slot.enabled = 0;
		slot.config_cksum = ((~((static_cast<uint64_t>(slot.enabled)<<32)|static_cast<uint64_t>(slot.lifetime))) ^ (slotkey_lshifted1>>1));
		slot.timeout_cksum = ((~slot.timeout) ^ (slotkey_lshifted1>>1));
		slot.last_serviced_by[0] = '\0'; // null string.
	}

	UWTaskCreate("PS_WDT", TASK_PRIORITY_WATCHDOG, [this]() -> void { this->_run_thread(); });
}

PS_WDT::~PS_WDT() {
	configASSERT(0); // Unsupported, as we don't have a mechanism to trigger thread deletion.
	// Well, I hope you deactivated us first, or this is going to go poorly for you.
	*this->heap_slotkey_rshifted1 = 0;
	free(const_cast<uint64_t*>(this->heap_slotkey_rshifted1));
	memset(this->slots, 0, this->num_slots * sizeof(struct wdtslot));
	free(this->slots);
}

void PS_WDT::_run_thread() {
	// We really don't want this interrupted.
	CriticalGuard critical(true);

	/* Default clock is CPU_1x.  According to Vivado the WDT clock is: Requested=133.333Mhz, Actual=111.111Mhz
	 *
	 * We'll use a 5 second timeout here.
	 *
	 * 0x20fff*4096 / CPU1x = 4.98 seconds.
	 */
	XWdtPs_SetControlValue(&this->wdt, (u8)XWDTPS_COUNTER_RESET, (u8)0x20);
	XWdtPs_SetControlValue(&this->wdt, (u8)XWDTPS_CLK_PRESCALE, (u8)XWDTPS_CCR_PSCALE_4096);

	XWdtPs_EnableOutput(&this->wdt, XWDTPS_RESET_SIGNAL);
	XWdtPs_Start(&this->wdt);
	XWdtPs_RestartWdt(&this->wdt);

	critical.release();

	while (true) {
		vTaskDelay(configTICK_RATE_HZ); // On a five second reset, we can service once per second.

		uint64_t now64 = get_tick64();
		for (u8 i = 0; i < this->num_slots; ++i) {
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
		}
		else {
			if (this->on_trip)
				this->on_trip();
			volatile bool suspend = true;
			while (suspend) {
				// Done.
			}
		}
	}
}

/**
 * Register a free watchdog slot with the specified lifetime.
 *
 * @param lifetime The timeout period for this slot in ticks
 * @return A (inactive) slot handle.
 */
PS_WDT::slot_handle_t PS_WDT::register_slot(uint32_t lifetime) {
	CriticalGuard critical(true);
	if (this->free_slot >= this->num_slots)
		throw std::domain_error("No free PS_WDT slots.");
	u8 slotid = this->free_slot++;
	struct wdtslot &slot = this->slots[slotid];
	slot.enabled = 0;
	slot.lifetime = lifetime;
	slot.config_cksum = ((~((static_cast<uint64_t>(slot.enabled)<<32)|static_cast<uint64_t>(slot.lifetime))) ^ (slotkey_lshifted1>>1));
	slot.timeout_cksum = ((~slot.timeout) ^ (slotkey_lshifted1>>1));
	return 0x80000000|(slotid<<24)|((~slotid)<<16)|slotid;
}

/**
 * Enable & service the provided watchdog slot.
 *
 * @param slot_handle The slot to enable.
 */
void PS_WDT::activate_slot(slot_handle_t slot_handle) {
	u8 slotid = slot_handle & 0xff;
	if (slot_handle != (0x80000000|(slotid<<24)|((~slotid)<<16)|slotid)) // Valid?
		throw std::out_of_range(std::string("Invalid PS_WDT slot: ") + std::to_string(slotid));
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

/**
 * Disable the provided watchdog slot.
 *
 * @param slot_handle The slot to disable
 * @param deactivate_code Provide PS_WDT::deactivate_code_lshifted1>>1
 */
void PS_WDT::deactivate_slot(slot_handle_t slot_handle, uint32_t deactivate_code) {
	u8 slotid = slot_handle & 0xff;
	if (slot_handle != (0x80000000|(slotid<<24)|((~slotid)<<16)|slotid)) // Valid?
		throw std::out_of_range(std::string("Invalid PS_WDT slot: ") + std::to_string(slotid));
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

/**
 * Service the watchdog for a provided slot.
 *
 * @param slot_handle The watchdog slot to service.
 */
void PS_WDT::service_slot(slot_handle_t slot_handle) {
	u8 slotid = slot_handle & 0xff;
	if (slot_handle != (0x80000000|(slotid<<24)|((~slotid)<<16)|slotid)) // Valid?
		throw std::out_of_range(std::string("Invalid PS_WDT slot: ") + std::to_string(slotid));
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

const volatile uint32_t PS_WDT::deactivate_code_lshifted1 = 0x508030a4UL;
const volatile uint64_t PS_WDT::slotkey_lshifted1 = 0x9b0b3beee931a24ULL; ///< The key component of slot_cksum, left-shifted one bit.
