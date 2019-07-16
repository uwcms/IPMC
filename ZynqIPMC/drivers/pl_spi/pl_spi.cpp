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

// Only include driver if PL SPI is detected in the BSP.
#if XSDK_INDEXING || __has_include("xspi.h")

#include "pl_spi.h"
#include <libs/printf.h>
#include <libs/except.h>

static void XSpi_Abort(XSpi *InstancePtr)
{
	uint16_t control_reg;

	/*
	 * Deselect the slave on the SPI bus to abort a transfer, this must be
	 * done before the device is disabled such that the signals which are
	 * driven by the device are changed without the device enabled.
	 */
	XSpi_SetSlaveSelectReg(InstancePtr,
				InstancePtr->SlaveSelectMask);
	/*
	 * Abort the operation currently in progress. Clear the mode
	 * fault condition by reading the status register (done) then
	 * writing the control register.
	 */
	control_reg = XSpi_GetControlReg(InstancePtr);

	/*
	 * Stop any transmit in progress and reset the FIFOs if they exist,
	 * don't disable the device just inhibit any data from being sent.
	 */
	control_reg |= XSP_CR_TRANS_INHIBIT_MASK;

	if (InstancePtr->HasFifos) {
		control_reg |= (XSP_CR_TXFIFO_RESET_MASK |
				XSP_CR_RXFIFO_RESET_MASK);
	}

	XSpi_SetControlReg(InstancePtr, control_reg);

	InstancePtr->RemainingBytes = 0;
	InstancePtr->RequestedBytes = 0;
	InstancePtr->IsBusy = FALSE;
}


void PLSPI::_InterruptHandler() {
	uint32_t intr_status = 0;
	uint32_t data = 0;
	uint32_t control_reg = 0;
	uint32_t status_reg = 0;
	uint8_t  data_width = 0;

	/*
	 * Get the Interrupt Status. Immediately clear the interrupts in case
	 * this Isr causes another interrupt to be generated. If we clear at the
	 * end of the Isr, we may miss this newly generated interrupt. This
	 * occurs because we transmit from within the Isr, potentially causing
	 * another TX_EMPTY interrupt.
	 */
	intr_status = XSpi_IntrGetStatus(&this->xspi);
	XSpi_IntrClear(&this->xspi, intr_status);

	/*
	 * Check for mode fault error. We want to check for this error first,
	 * before checking for progress of a transfer, since this error needs
	 * to abort any operation in progress.
	 */
	if (intr_status & XSP_INTR_MODE_FAULT_MASK) {
		this->xspi.Stats.ModeFaults++;

		/*
		 * Abort any operation currently in progress. This includes
		 * clearing the mode fault condition by reading the status
		 * register.
		 * Note that the status register should be read after the Abort
		 * since reading the status register clears the mode fault
		 * condition and would cause the device to restart any transfer
		 * that may be in progress.
		 */
		XSpi_Abort(&this->xspi);

		(void) XSpi_GetStatusReg(&this->xspi);

		unsigned int status = 0xFFFFFFFF; // Report failure
		xQueueSendFromISR(this->sync, &status, NULL);

		return;		/* Do not continue servicing other interrupts */
	}

	/*
	 * Check for overrun error. Simply report the error and update the
	 * statistics.
	 */
	if (intr_status & XSP_INTR_RX_OVERRUN_MASK) {
		this->xspi.Stats.RecvOverruns++;
	}

	data_width = this->xspi.DataWidth;
	if ((intr_status & XSP_INTR_TX_EMPTY_MASK) ||
		(intr_status & XSP_INTR_TX_HALF_EMPTY_MASK)) {
		/*
		 * A transmit has just completed. Process received data and
		 * check for more data to transmit. Always inhibit the
		 * transmitter while the Isr re-fills the transmit
		 * register/FIFO, or make sure it is stopped if we're done.
		 */
		control_reg = XSpi_GetControlReg(&this->xspi);
		XSpi_SetControlReg(&this->xspi, control_reg |
					XSP_CR_TRANS_INHIBIT_MASK);

		/*
		 * First get the data received as a result of the transmit that
		 * just completed.  We get all the data available by reading the
		 * status register to determine when the receive register/FIFO
		 * is empty. Always get the received data, but only fill the
		 * receive buffer if it points to something (the upper layer
		 * software may not care to receive data).
		 */
		status_reg = XSpi_GetStatusReg(&this->xspi);

		while ((status_reg & XSP_SR_RX_EMPTY_MASK) == 0) {
			data = XSpi_ReadReg(this->xspi.BaseAddr, XSP_DRR_OFFSET);

			/*
			 * Data Transfer Width is Byte (8 bit).
			 */
			if (data_width == XSP_DATAWIDTH_BYTE) {
				if (this->xspi.RecvBufferPtr != NULL) {
					*this->xspi.RecvBufferPtr++ = (uint8_t) data;
				}
			} else if (data_width == XSP_DATAWIDTH_HALF_WORD) {
				if (this->xspi.RecvBufferPtr != NULL) {
					*(uint16_t *) this->xspi.RecvBufferPtr =
							(uint16_t) data;
					this->xspi.RecvBufferPtr +=2;
				}
			} else if (data_width == XSP_DATAWIDTH_WORD) {
				if (this->xspi.RecvBufferPtr != NULL) {
					*(uint32_t *) this->xspi.RecvBufferPtr =
							data;
					this->xspi.RecvBufferPtr +=4;
				}
			}

			this->xspi.Stats.BytesTransferred += (data_width >> 3);
			status_reg = XSpi_GetStatusReg(&this->xspi);
		}

		/*
		 * See if there is more data to send.
		 */
		if (this->xspi.RemainingBytes > 0) {
			/*
			 * Fill the DTR/FIFO with as many bytes as it will take
			 * (or as many as we have to send). We use the full
			 * status bit to know if the device can take more data.
			 * By doing this, the driver does not need to know the
			 * size of the FIFO or that there even is a FIFO.
			 * The downside is that the status must be read each
			 * loop iteration.
			 */
			status_reg = XSpi_GetStatusReg(&this->xspi);
			while (((status_reg & XSP_SR_TX_FULL_MASK) == 0) &&
				(this->xspi.RemainingBytes > 0)) {
				if (data_width == XSP_DATAWIDTH_BYTE) {
					/*
					 * Data Transfer Width is Byte (8 bit).
					 */
					data = *this->xspi.SendBufferPtr;

				} else if (data_width ==
						XSP_DATAWIDTH_HALF_WORD) {
					/*
					 * Data Transfer Width is Half Word
					 * (16 bit).
					 */
					data = *(uint16_t *) this->xspi.SendBufferPtr;
				} else if (data_width ==
						XSP_DATAWIDTH_WORD) {
					/*
					 * Data Transfer Width is Word (32 bit).
					 */
					data = *(uint32_t *) this->xspi.SendBufferPtr;
				}

				XSpi_WriteReg(this->xspi.BaseAddr, XSP_DTR_OFFSET,
						data);
				this->xspi.SendBufferPtr += (data_width >> 3);
				this->xspi.RemainingBytes -= (data_width >> 3);
				status_reg = XSpi_GetStatusReg(&this->xspi);
			}

			/*
			 * Start the transfer by not inhibiting the transmitter
			 * any longer.
			 */
			XSpi_SetControlReg(&this->xspi, control_reg);
		} else {
			/*
			 * No more data to send.  Disable the interrupt and
			 * inform the upper layer software that the transfer is
			 * done. The interrupt will be re-enabled when another
			 * transfer is initiated.
			 */
			XSpi_IntrDisable(&this->xspi, XSP_INTR_TX_EMPTY_MASK);

			this->xspi.IsBusy = FALSE;

			unsigned int status = 0;
			if (this->xspi.Stats.RecvOverruns > 0) status = 1;

			xQueueSendFromISR(this->sync, &status, NULL);
		}
	}
}

PLSPI::PLSPI(uint16_t device_id, uint16_t intr_id)
: InterruptBasedDriver(intr_id, 0x3) {
	this->sync = xQueueCreate(1, sizeof(unsigned int));
	configASSERT(this->sync);

	// Initialize the XSpi driver so that it's ready to use.
	if (XST_SUCCESS != XSpi_Initialize(&(this->xspi), device_id)) {
		throw except::hardware_error("Unable to initialize PLSPI(device_id=" + std::to_string(device_id) + ")");
	}

	// Perform a self-test to ensure that the hardware was built correctly.
	if (XST_SUCCESS != XSpi_SelfTest(&(this->xspi))) {
		throw except::hardware_error("Self-test failed for PLSPI(device_id=" + std::to_string(device_id) + ")");
	}

	// Apply proper settings to the IP.
	if (XST_SUCCESS != XSpi_SetOptions(&this->xspi, XSP_MASTER_OPTION | XSP_MANUAL_SSELECT_OPTION)) {
		throw except::hardware_error("Unable to set options of PLSPI(device_id=" + std::to_string(device_id) + ")");
	}

	this->enableInterrupts();
}

PLSPI::~PLSPI() {
}

void PLSPI::start() {
	this->xspi.Stats.RecvOverruns = 0;
	XSpi_Start(&this->xspi);
}

void PLSPI::stop() {
	XSpi_Stop(&this->xspi);
}

bool PLSPI::transfer(size_t chip, const uint8_t *sendbuf, uint8_t *recvbuf, size_t bytes, TickType_t timeout) {
	MutexGuard<false> lock(this->mutex, true);

	this->select(chip);
	bool r = this->transfer(sendbuf, recvbuf, bytes, timeout);
	this->deselect();

	return r;
}

bool PLSPI::transfer(const uint8_t *sendbuf, uint8_t *recvbuf, size_t bytes, TickType_t timeout) {
	if (!this->inAtomic()) throw std::runtime_error("Not atomic, unsafe operation");

	int r = XSpi_Transfer(&this->xspi, (uint8_t*)sendbuf, recvbuf, bytes);
	if (r != XST_SUCCESS) return false;

	unsigned int status = 0;
	if (!xQueueReceive(this->sync, &status, timeout)) {
		// Transfer failed
		return false;
	}

	if (status != 0) return false;

	// Transfer successful
	return true;
}

void PLSPI::select(size_t cs) {
	this->start();
	XSpi_SetSlaveSelect(&this->xspi, (1 << cs));
	XSpi_SetSlaveSelectReg(&this->xspi, this->xspi.SlaveSelectReg);
}

void PLSPI::deselect() {
	XSpi_SetSlaveSelectReg(&this->xspi, this->xspi.SlaveSelectMask);
	this->stop();
}

#endif

