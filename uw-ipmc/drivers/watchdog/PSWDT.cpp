/*
 * PSWDT.cpp
 *
 *  Created on: Feb 1, 2018
 *      Author: jtikalsky
 */

#include <drivers/watchdog/PSWDT.h>
#include <IPMC.h>
#include <libs/ThreadingPrimitives.h>
#include "FreeRTOS.h"
#include "task.h"

static void run_PS_WDT_thread(void *cb_ps) {
	reinterpret_cast<PS_WDT*>(cb_ps)->_run_thread();
}

const static volatile uint32_t global_canary_lshifted1 = 0x87d64518;
#define GLOBAL_CANARY_RSHIFTED1 0x21f59146

/**
 * Instantiate and start a PS_WDT.
 *
 * @param DeviceId The DeviceID of the XWDTPS
 * @param num_slots The number of service slots provided by this watchdog
 * @param log A log facility for when things go VERY wrong
 */
PS_WDT::PS_WDT(u32 DeviceId, u8 num_slots, LogTree &log)
	: log(log), num_slots(num_slots), free_slot(0) {
	this->slots = (struct wdtslot*)malloc(num_slots * sizeof(struct wdtslot));
	this->heap_slotkey_rshifted1 = (volatile uint64_t*)malloc(sizeof(uint64_t));
	*this->heap_slotkey_rshifted1 = PS_WDT::slotkey_lshifted1 >> 2;
	this->global_canary = global_canary_lshifted1>>1;

	XWdtPs_Config *Config = XWdtPs_LookupConfig(DeviceId);
	configASSERT(Config);
	configASSERT(XST_SUCCESS == XWdtPs_CfgInitialize(&this->wdt, Config, Config->BaseAddress));

	for (u8 i = 0; i < this->num_slots; ++i) {
		struct wdtslot &slot = this->slots[i];
		slot.enabled = 0;
		slot.config_cksum = ((~((static_cast<uint64_t>(slot.enabled)<<32)|static_cast<uint64_t>(slot.lifetime))) ^ (slotkey_lshifted1>>1));
		slot.timeout_cksum = ((~slot.timeout) ^ (slotkey_lshifted1>>1));
	}

	configASSERT(xTaskCreate(run_PS_WDT_thread, "PS_WDT", UWIPMC_STANDARD_STACK_SIZE, this, TASK_PRIORITY_WATCHDOG, NULL));
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
	portENTER_CRITICAL();

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

	portEXIT_CRITICAL();

	while (true) {
		vTaskDelay(configTICK_RATE_HZ); // On a five second reset, we can service once per second.

		uint64_t now64 = get_tick64();
		for (u8 i = 0; i < this->num_slots; ++i) {
			struct wdtslot &slot = this->slots[i];
			if (slot.config_cksum != ((~((static_cast<uint64_t>(slot.enabled)<<32)|static_cast<uint64_t>(slot.lifetime))) ^ (slotkey_lshifted1>>1))) {
				if (this->global_canary) {
					this->global_canary = 0; // Break.
					this->log.log(stdsprintf("WATCHDOG MEMORY CORRUPTED: Slot %hhu config_cksum mismatch.  WATCHDOG SERVICE DISABLED.", i), LogTree::LOG_CRITICAL);
				}
			}
			if (slot.timeout_cksum != ((~slot.timeout) ^ (slotkey_lshifted1>>1))) {
				if (this->global_canary) {
					this->global_canary = 0; // Break.
					this->log.log(stdsprintf("WATCHDOG MEMORY CORRUPTED: Slot %hhu timeout_cksum mismatch.  WATCHDOG SERVICE DISABLED.", i), LogTree::LOG_CRITICAL);
				}
			}
			if (slot.enabled == 0)
				continue;
			if (slot.enabled != UINT32_MAX) {
				if (this->global_canary) {
					this->global_canary = 0; // Break.
					this->log.log(stdsprintf("WATCHDOG MEMORY CORRUPTED: Slot %hhu enable value invalid.  WATCHDOG SERVICE DISABLED.", i), LogTree::LOG_CRITICAL);
				}
			}
			if (slot.timeout < now64) {
				if (this->global_canary) {
					this->global_canary = 0; // Break.
					this->log.log(stdsprintf("WATCHDOG TIMEOUT EXPIRED: Slot %hhu watchdog has expired.  WATCHDOG SERVICE DISABLED.", i), LogTree::LOG_CRITICAL);
				}
			}
		}
		if (this->global_canary == (global_canary_lshifted1>>1) && this->global_canary == (GLOBAL_CANARY_RSHIFTED1<<1)) {
			XWdtPs_RestartWdt(&this->wdt);
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
	portENTER_CRITICAL();
	configASSERT(this->free_slot < this->num_slots);
	u8 slotid = this->free_slot++;
	struct wdtslot &slot = this->slots[slotid];
	slot.enabled = 0;
	slot.lifetime = lifetime;
	slot.config_cksum = ((~((static_cast<uint64_t>(slot.enabled)<<32)|static_cast<uint64_t>(slot.lifetime))) ^ (slotkey_lshifted1>>1));
	slot.timeout_cksum = ((~slot.timeout) ^ (slotkey_lshifted1>>1));
	portEXIT_CRITICAL();
	return 0x80000000|(slotid<<24)|((~slotid)<<16)|slotid;
}

/**
 * Enable & service the provided watchdog slot.
 *
 * @param slot_handle The slot to enable.
 */
void PS_WDT::activate_slot(slot_handle_t slot_handle) {
	u8 slotid = slot_handle & 0xff;
	configASSERT(slot_handle == (0x80000000|(slotid<<24)|((~slotid)<<16)|slotid)); // Valid?
	portENTER_CRITICAL();
	struct wdtslot &slot = this->slots[slotid];
	if (slot.config_cksum != ((~((static_cast<uint64_t>(slot.enabled)<<32)|static_cast<uint64_t>(slot.lifetime))) ^ (slotkey_lshifted1>>1))) {
		if (this->global_canary) {
			this->global_canary = 0; // Break.
			this->log.log(stdsprintf("WATCHDOG MEMORY CORRUPTED: Slot %hhu config_cksum mismatch.  WATCHDOG SERVICE DISABLED.", slotid), LogTree::LOG_CRITICAL);
		}
	}
	slot.enabled = UINT32_MAX;
	slot.config_cksum = ((~((static_cast<uint64_t>(slot.enabled)<<32)|static_cast<uint64_t>(slot.lifetime))) ^ (slotkey_lshifted1>>1));
	slot.timeout = get_tick64() + slot.lifetime;
	slot.timeout_cksum = ((~slot.timeout) ^ (slotkey_lshifted1>>1));
	portEXIT_CRITICAL();
	this->log.log(stdsprintf("Watchdog slot %hhu activated by %s.", slotid, pcTaskGetName(NULL)), LogTree::LOG_INFO);
}

/**
 * Disable the provided watchdog slot.
 *
 * @param slot_handle The slot to disable
 * @param deactivate_code Provide PS_WDT::deactivate_code_lshifted1>>1
 */
void PS_WDT::deactivate_slot(slot_handle_t slot_handle, uint32_t deactivate_code) {
	u8 slotid = slot_handle & 0xff;
	configASSERT(slot_handle == (0x80000000|(slotid<<24)|((~slotid)<<16)|slotid)); // Valid?
	portENTER_CRITICAL();
	if (deactivate_code != (deactivate_code_lshifted1>>1)) {
		this->global_canary = 0; // Break.
		this->log.log(stdsprintf("WATCHDOG ILLEGAL DISABLE: Slot %hhu deactivate_code invalid.  WATCHDOG SERVICE DISABLED.", slotid), LogTree::LOG_CRITICAL);
	}
	struct wdtslot &slot = this->slots[slotid];
	if (slot.config_cksum != ((~((static_cast<uint64_t>(slot.enabled)<<32)|static_cast<uint64_t>(slot.lifetime))) ^ (slotkey_lshifted1>>1))) {
		if (this->global_canary) {
			this->global_canary = 0; // Break.
			this->log.log(stdsprintf("WATCHDOG MEMORY CORRUPTED: Slot %hhu config_cksum mismatch.  WATCHDOG SERVICE DISABLED.", slotid), LogTree::LOG_CRITICAL);
		}
	}
	slot.enabled = 0;
	slot.config_cksum = ((~((static_cast<uint64_t>(slot.enabled)<<32)|static_cast<uint64_t>(slot.lifetime))) ^ (slotkey_lshifted1>>1));
	portEXIT_CRITICAL();
	this->log.log(stdsprintf("Watchdog slot %hhu deactivated by %s.", slotid, pcTaskGetName(NULL)), LogTree::LOG_INFO);
}

/**
 * Service the watchdog for a provided slot.
 *
 * @param slot_handle The watchdog slot to service.
 */
void PS_WDT::service_slot(slot_handle_t slot_handle) {
	u8 slotid = slot_handle & 0xff;
	configASSERT(slot_handle == (0x80000000|(slotid<<24)|((~slotid)<<16)|slotid)); // Valid?
	portENTER_CRITICAL();
	struct wdtslot &slot = this->slots[slotid];
	if (slot.config_cksum != ((~((static_cast<uint64_t>(slot.enabled)<<32)|static_cast<uint64_t>(slot.lifetime))) ^ (slotkey_lshifted1>>1))) {
		if (this->global_canary) {
			this->global_canary = 0; // Break.
			this->log.log(stdsprintf("WATCHDOG MEMORY CORRUPTED: Slot %hhu config_cksum mismatch.  WATCHDOG SERVICE DISABLED.", slotid), LogTree::LOG_CRITICAL);
		}
	}
	if (slot.timeout_cksum != ((~slot.timeout) ^ (slotkey_lshifted1>>1))) {
		if (this->global_canary) {
			this->global_canary = 0; // Break.
			this->log.log(stdsprintf("WATCHDOG MEMORY CORRUPTED: Slot %hhu timeout_cksum mismatch.  WATCHDOG SERVICE DISABLED.", slotid), LogTree::LOG_CRITICAL);
		}
	}
	slot.timeout = get_tick64() + slot.lifetime;
	slot.timeout_cksum = ((~slot.timeout) ^ (slotkey_lshifted1>>1));
	portEXIT_CRITICAL();
	this->log.log(stdsprintf("Watchdog slot %hhu serviced by %s.", slotid, pcTaskGetName(NULL)), LogTree::LOG_DIAGNOSTIC);
}

const volatile uint32_t PS_WDT::deactivate_code_lshifted1 = 0x508030a4UL;
const volatile uint64_t PS_WDT::slotkey_lshifted1 = 0x9b0b3beee931a24ULL; ///< The key component of slot_cksum, left-shifted one bit.
