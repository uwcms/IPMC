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

// Only include driver if PS SPI is detected in the BSP.
#if XSDK_INDEXING || __has_include("xspips.h")

#include "ps_spi.h"
#include <libs/printf.h>
#include <libs/except.h>

PSSPI::PSSPI(uint16_t device_id, uint16_t intr_id) :
InterruptBasedDriver(intr_id) {
	this->irq_sync_q = xQueueCreate(1, sizeof(trans_st_t));
	configASSERT(this->irq_sync_q);

	XSpiPs_Config *config = XSpiPs_LookupConfig(device_id);
	if (config == nullptr) {
		throw except::hardware_error("Unable find configuration for PSSPI(device_id=" + std::to_string(device_id) + ")");
	}

	if (XST_SUCCESS != XSpiPs_CfgInitialize(&this->xspips, config, config->BaseAddress)) {
		throw except::hardware_error("Unable to initialize PSSPI(device_id=" + std::to_string(device_id) + ")");
	}

	if (XST_SUCCESS != XSpiPs_SelfTest(&this->xspips)) {
		throw except::hardware_error("Self-test failed for PSSPI(device_id=" + std::to_string(device_id) + ")");
	}

	XSpiPs_Reset(&this->xspips);

	XSpiPs_SetStatusHandler(&this->xspips, reinterpret_cast<void*>(this),
			reinterpret_cast<XSpiPs_StatusHandler>(PSSPI::_InterruptPassthrough));

	XSpiPs_SetOptions(&this->xspips, XSPIPS_MANUAL_START_OPTION |
			XSPIPS_MASTER_OPTION |
			XSPIPS_FORCE_SSELECT_OPTION);

	XSpiPs_SetClkPrescaler(&this->xspips, XSPIPS_CLK_PRESCALE_64);

	this->error_not_done = 0;
	this->error_byte_count = 0;

	this->transfer_running = false;

	// Enable interrupts
	this->enableInterrupts();
}

PSSPI::~PSSPI() {
}

bool PSSPI::transfer(size_t chip, const uint8_t *sendbuf, uint8_t *recvbuf, size_t bytes, TickType_t timeout) {
	// TODO: Needs some work
	MutexGuard<true> lock(this->mutex, true);

	// Assert the EEPROM chip select
	XSpiPs_SetSlaveSelect(&this->xspips, chip);

	// Start SPI transfer
	XSpiPs_Transfer(&this->xspips, (uint8_t*)sendbuf, recvbuf, bytes);
	this->transfer_running = true;

	// Block on the queue, waiting for irq to signal transfer completion
	trans_st_t trans_st;
	xQueueReceive(this->irq_sync_q, &(trans_st), timeout);

	bool rc = true;

	//If the event was not transfer done, then track it as an error
	if (trans_st.event_status != XST_SPI_TRANSFER_DONE) {
		this->error_not_done++;
		rc = false;
	}

	//If the last transfer completed with, then track it as an error
	if (trans_st.byte_count != bytes) {
		this->error_byte_count++;
#if 0
		rc = false;  //TODO: not clear yet why xilinx driver reports byte_count = 0 for all successful transfers
#endif
	}

	return rc;
}

bool PSSPI::transfer(const uint8_t *sendbuf, uint8_t *recvbuf, size_t bytes, TickType_t timeout) {
	if (!this->inAtomic()) throw std::runtime_error("Not atomic, unsafe operation");

	// Start SPI transfer
	XSpiPs_Transfer(&this->xspips, (uint8_t*)sendbuf, recvbuf, bytes);
	this->transfer_running = true;

	// Block on the queue, waiting for irq to signal transfer completion
	trans_st_t trans_st;
	xQueueReceive(this->irq_sync_q, &(trans_st), timeout);

	bool rc = true;

	//If the event was not transfer done, then track it as an error
	if (trans_st.event_status != XST_SPI_TRANSFER_DONE) {
		this->error_not_done++;
		rc = false;
	}

	//If the last transfer completed with, then track it as an error
	if (trans_st.byte_count != bytes) {
		this->error_byte_count++;
#if 0
		rc = false;  //TODO: not clear yet why xilinx driver reports byte_count = 0 for all successful transfers
#endif
	}

	return rc;
}

void PSSPI::select(size_t cs) {
	// TODO
}

void PSSPI::deselect() {
	// TODO
}

void PSSPI::_InterruptPassthrough(PSSPI *ps_spi, uint32_t event_status, uint32_t byte_count) {
	ps_spi->_HandleInterrupt(event_status, byte_count);
}

void PSSPI::_InterruptHandler() {
	XSpiPs_InterruptHandler(&(this->xspips));
}

void PSSPI::_HandleInterrupt(uint32_t event_status, uint32_t byte_count) {
	trans_st_t trans_st;
	trans_st.event_status = event_status;
	trans_st.byte_count = byte_count;

	xQueueSendFromISR(this->irq_sync_q, &trans_st, pdFALSE);

	this->transfer_running = false;
}

#endif
