/*
 * PLSPI.cpp
 *
 *  Created on: Aug 17, 2018
 *      Author: mpv
 */

#include <drivers/pl_spi/PLSPI.h>
#include <libs/printf.h>
#include <libs/except.h>

void XSpi_Abort(XSpi *InstancePtr)
{
	u16 ControlReg;

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
	ControlReg = XSpi_GetControlReg(InstancePtr);

	/*
	 * Stop any transmit in progress and reset the FIFOs if they exist,
	 * don't disable the device just inhibit any data from being sent.
	 */
	ControlReg |= XSP_CR_TRANS_INHIBIT_MASK;

	if (InstancePtr->HasFifos) {
		ControlReg |= (XSP_CR_TXFIFO_RESET_MASK |
				XSP_CR_RXFIFO_RESET_MASK);
	}

	XSpi_SetControlReg(InstancePtr, ControlReg);

	InstancePtr->RemainingBytes = 0;
	InstancePtr->RequestedBytes = 0;
	InstancePtr->IsBusy = FALSE;
}


void PL_SPI::_InterruptHandler() {
	u32 IntrStatus;
	u32 Data = 0;
	u32 ControlReg;
	u32 StatusReg;
	u8  DataWidth;

	/*
	 * Get the Interrupt Status. Immediately clear the interrupts in case
	 * this Isr causes another interrupt to be generated. If we clear at the
	 * end of the Isr, we may miss this newly generated interrupt. This
	 * occurs because we transmit from within the Isr, potentially causing
	 * another TX_EMPTY interrupt.
	 */
	IntrStatus = XSpi_IntrGetStatus(&this->xspi);
	XSpi_IntrClear(&this->xspi, IntrStatus);

	/*
	 * Check for mode fault error. We want to check for this error first,
	 * before checking for progress of a transfer, since this error needs
	 * to abort any operation in progress.
	 */
	if (IntrStatus & XSP_INTR_MODE_FAULT_MASK) {
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
	if (IntrStatus & XSP_INTR_RX_OVERRUN_MASK) {
		this->xspi.Stats.RecvOverruns++;
	}

	DataWidth = this->xspi.DataWidth;
	if ((IntrStatus & XSP_INTR_TX_EMPTY_MASK) ||
		(IntrStatus & XSP_INTR_TX_HALF_EMPTY_MASK)) {
		/*
		 * A transmit has just completed. Process received data and
		 * check for more data to transmit. Always inhibit the
		 * transmitter while the Isr re-fills the transmit
		 * register/FIFO, or make sure it is stopped if we're done.
		 */
		ControlReg = XSpi_GetControlReg(&this->xspi);
		XSpi_SetControlReg(&this->xspi, ControlReg |
					XSP_CR_TRANS_INHIBIT_MASK);

		/*
		 * First get the data received as a result of the transmit that
		 * just completed.  We get all the data available by reading the
		 * status register to determine when the receive register/FIFO
		 * is empty. Always get the received data, but only fill the
		 * receive buffer if it points to something (the upper layer
		 * software may not care to receive data).
		 */
		StatusReg = XSpi_GetStatusReg(&this->xspi);

		while ((StatusReg & XSP_SR_RX_EMPTY_MASK) == 0) {
			Data = XSpi_ReadReg(this->xspi.BaseAddr, XSP_DRR_OFFSET);

			/*
			 * Data Transfer Width is Byte (8 bit).
			 */
			if (DataWidth == XSP_DATAWIDTH_BYTE) {
				if (this->xspi.RecvBufferPtr != NULL) {
					*this->xspi.RecvBufferPtr++ = (u8) Data;
				}
			} else if (DataWidth == XSP_DATAWIDTH_HALF_WORD) {
				if (this->xspi.RecvBufferPtr != NULL) {
					*(u16 *) this->xspi.RecvBufferPtr =
							(u16) Data;
					this->xspi.RecvBufferPtr +=2;
				}
			} else if (DataWidth == XSP_DATAWIDTH_WORD) {
				if (this->xspi.RecvBufferPtr != NULL) {
					*(u32 *) this->xspi.RecvBufferPtr =
							Data;
					this->xspi.RecvBufferPtr +=4;
				}
			}

			this->xspi.Stats.BytesTransferred += (DataWidth >> 3);
			StatusReg = XSpi_GetStatusReg(&this->xspi);
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
			StatusReg = XSpi_GetStatusReg(&this->xspi);
			while (((StatusReg & XSP_SR_TX_FULL_MASK) == 0) &&
				(this->xspi.RemainingBytes > 0)) {
				if (DataWidth == XSP_DATAWIDTH_BYTE) {
					/*
					 * Data Transfer Width is Byte (8 bit).
					 */
					Data = *this->xspi.SendBufferPtr;

				} else if (DataWidth ==
						XSP_DATAWIDTH_HALF_WORD) {
					/*
					 * Data Transfer Width is Half Word
					 * (16 bit).
					 */
					Data = *(u16 *) this->xspi.SendBufferPtr;
				} else if (DataWidth ==
						XSP_DATAWIDTH_WORD) {
					/*
					 * Data Transfer Width is Word (32 bit).
					 */
					Data = *(u32 *) this->xspi.SendBufferPtr;
				}

				XSpi_WriteReg(this->xspi.BaseAddr, XSP_DTR_OFFSET,
						Data);
				this->xspi.SendBufferPtr += (DataWidth >> 3);
				this->xspi.RemainingBytes -= (DataWidth >> 3);
				StatusReg = XSpi_GetStatusReg(&this->xspi);
			}

			/*
			 * Start the transfer by not inhibiting the transmitter
			 * any longer.
			 */
			XSpi_SetControlReg(&this->xspi, ControlReg);
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

PL_SPI::PL_SPI(uint16_t DeviceId, uint32_t IntrId)
: InterruptBasedDriver(IntrId, 0x3) {
	this->sync = xQueueCreate(1, sizeof(unsigned int));
	configASSERT(this->sync);

	// Initialize the XSpi driver so that it's ready to use
	if (XST_SUCCESS != XSpi_Initialize(&(this->xspi), DeviceId))
		throw except::hardware_error(stdsprintf("Unable to initialize PL_SPI(%hu, %lu)", DeviceId, IntrId));

	// Perform a self-test to ensure that the hardware was built correctly
	if (XST_SUCCESS != XSpi_SelfTest(&(this->xspi)))
		throw except::hardware_error(stdsprintf("Self-test for PL_SPI(%hu, %lu) failed.", DeviceId, IntrId));

	// Apply proper settings to the IP.
	if (XST_SUCCESS != XSpi_SetOptions(&this->xspi, XSP_MASTER_OPTION | XSP_MANUAL_SSELECT_OPTION))
		throw except::hardware_error(stdsprintf("Unable to XSpi_SetOptions on PL_SPI(%hu, %lu)", DeviceId, IntrId));

	this->enableInterrupts();
}

PL_SPI::~PL_SPI() {
}

void PL_SPI::start() {
	this->xspi.Stats.RecvOverruns = 0;
	XSpi_Start(&this->xspi);
}

void PL_SPI::stop() {
	XSpi_Stop(&this->xspi);
}

bool PL_SPI::transfer(u8 cs, const u8 *sendbuf, u8 *recvbuf, size_t bytes, TickType_t timeout) {
	MutexGuard<false> lock(this->mutex, true);

	this->select(cs);
	bool r = this->transfer_unsafe(sendbuf, recvbuf, bytes, timeout);
	this->deselect();

	return r;
}

bool PL_SPI::transfer_unsafe(const u8 *sendbuf, u8 *recvbuf, size_t bytes, TickType_t timeout) {
	int r = XSpi_Transfer(&this->xspi, (u8*)sendbuf, recvbuf, bytes);
	if (r != XST_SUCCESS) return false;

	// TODO: This should have a proper timeout
	unsigned int status = 0;
	if (!xQueueReceive(this->sync, &status, timeout)) {
		// Transfer failed
		return false;
	}

	if (status != 0) return false;

	// Transfer successful
	return true;
}

void PL_SPI::select(uint32_t cs) {
	this->start();
	XSpi_SetSlaveSelect(&this->xspi, (1 << cs));
	XSpi_SetSlaveSelectReg(&this->xspi, this->xspi.SlaveSelectReg);
}

void PL_SPI::deselect() {
	XSpi_SetSlaveSelectReg(&this->xspi, this->xspi.SlaveSelectMask);
	this->stop();
}
