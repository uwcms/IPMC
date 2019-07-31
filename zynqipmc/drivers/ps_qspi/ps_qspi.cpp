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

// Only include driver if PS QSPI is detected in the BSP.
#if XSDK_INDEXING || __has_include("xqspips.h")

#include "ps_qspi.h"
#include <libs/printf.h>
#include <libs/except.h>

//! This typedef defines QSPI flash instruction format.
typedef struct {
	uint8_t opcode;		///< Operational code of the instruction.
	uint8_t instsize;	///< Size of the instruction including address bytes.
	uint8_t txoffset;	///< Register address where instruction has to be written.
} XQspiPsInstFormat;

PSQSPI::PSQSPI(uint16_t device_id, uint16_t intr_id) :
InterruptBasedDriver(intr_id), selected(false), started(false), OpMode(SINGLE) {
	this->sync = xQueueCreate(1, sizeof(unsigned int));
	configASSERT(this->sync);

	XQspiPs_Config *config = XQspiPs_LookupConfig(device_id);
	if (config == nullptr) {
		throw except::hardware_error("Unable find configuration for PSQSPI(device_id=" + std::to_string(device_id) + ")");
	}

	if (XST_SUCCESS != XQspiPs_CfgInitialize(&this->qspi, config, config->BaseAddress)) {
		throw except::hardware_error("Unable to initialize PSQSPI(device_id=" + std::to_string(device_id) + ")");
	}

	if (XST_SUCCESS != XQspiPs_SelfTest(&this->qspi)) {
		throw except::hardware_error("Self-test failed for PSQSPI(device_id=" + std::to_string(device_id) + ")");
	}

	XQspiPs_Reset(&this->qspi);

	XQspiPs_SetOptions(&this->qspi, XQSPIPS_MANUAL_START_OPTION |
			XQSPIPS_HOLD_B_DRIVE_OPTION |
			XQSPIPS_FORCE_SSELECT_OPTION);

	XQspiPs_SetClkPrescaler(&this->qspi, XQSPIPS_CLK_PRESCALE_2);

	this->enableInterrupts();
}

PSQSPI::~PSQSPI() {
}

bool PSQSPI::transfer(size_t chip, const uint8_t *sendbuf, uint8_t *recvbuf, size_t bytes, TickType_t timeout) {
	MutexGuard<false> lock(this->mutex, true);

	this->select(chip);
	bool r = this->transfer(sendbuf, recvbuf, bytes, timeout);
	this->deselect();

	return r;
}

bool PSQSPI::transfer(const uint8_t *sendbuf, uint8_t *recvbuf, size_t bytes, TickType_t timeout) {
	if (!this->inAtomic()) throw std::runtime_error("Not atomic, unsafe operation");

	uint8_t instruction;
	XQspiPsInstFormat currinst;
	size_t transcount = 0;
	bool SwitchFlag = false;

	/*
	 * Check whether there is another transfer in progress. Not thread-safe.
	 */
	if (this->qspi.IsBusy) return false;

	/*
	 * Set the busy flag, which will be cleared in the ISR when the
	 * transfer is entirely done.
	 */
	this->qspi.IsBusy = TRUE;

	/*
	 * Set up buffer pointers.
	 */
	this->qspi.SendBufferPtr = (uint8_t*)sendbuf;
	this->qspi.RecvBufferPtr = recvbuf;

	this->qspi.RequestedBytes = bytes;
	this->qspi.RemainingBytes = bytes;

	if (!this->started) {
		/*
		 * The first byte with every chip-select assertion is always
		 * expected to be an instruction for flash interface mode
		 */
		instruction = *(this->qspi.SendBufferPtr);

		switch(bytes%4)
		{
			case XQSPIPS_SIZE_ONE:
				currinst.opcode = instruction;
				currinst.instsize = XQSPIPS_SIZE_ONE;
				currinst.txoffset = XQSPIPS_TXD_01_OFFSET;
				break;
			case XQSPIPS_SIZE_TWO:
				currinst.opcode = instruction;
				currinst.instsize = XQSPIPS_SIZE_TWO;
				currinst.txoffset = XQSPIPS_TXD_10_OFFSET;
				break;
			case XQSPIPS_SIZE_THREE:
				currinst.opcode = instruction;
				currinst.instsize = XQSPIPS_SIZE_THREE;
				currinst.txoffset = XQSPIPS_TXD_11_OFFSET;
				break;
			default:
				currinst.opcode = instruction;
				currinst.instsize = XQSPIPS_SIZE_FOUR;
				currinst.txoffset = XQSPIPS_TXD_00_OFFSET;
				break;
		}

		// Might need alignment
		if ((currinst.txoffset != XQSPIPS_TXD_00_OFFSET) && (bytes > 4)) {
			SwitchFlag = true;
		}

		/*
		 * If the instruction size in not 4 bytes then the data received needs
		 * to be shifted
		 */
		if( currinst.instsize != 4 ) {
			this->qspi.ShiftReadData = 1;
		} else {
			this->qspi.ShiftReadData = 0;
		}

		/*
		 * Set the RX FIFO threshold
		 */
		XQspiPs_WriteReg(this->qspi.Config.BaseAddress,
				XQSPIPS_RXWR_OFFSET, XQSPIPS_RXFIFO_THRESHOLD_OPT);

		/*
		 * If the slave select is "Forced" or under manual control,
		 * set the slave select now, before beginning the transfer.
		 */
		if (XQspiPs_IsManualChipSelect(&(this->qspi))) {
			uint32_t ConfigReg = XQspiPs_ReadReg(this->qspi.Config.BaseAddress,
					 XQSPIPS_CR_OFFSET);
			ConfigReg &= ~XQSPIPS_CR_SSCTRL_MASK;
			XQspiPs_WriteReg(this->qspi.Config.BaseAddress,
					  XQSPIPS_CR_OFFSET,
					  ConfigReg);
		}

		/*
		 * Enable the device.
		 */
		XQspiPs_Enable((&(this->qspi)));

		/*
		 * Clear all the interrupts.
		 */
		XQspiPs_WriteReg(this->qspi.Config.BaseAddress, XQSPIPS_SR_OFFSET,
				XQSPIPS_IXR_WR_TO_CLR_MASK);

		/* Get the complete command (flash inst + address/data) */
		uint32_t Data = *((uint32_t *)this->qspi.SendBufferPtr);
		this->qspi.SendBufferPtr += currinst.instsize;
		this->qspi.RemainingBytes -= currinst.instsize;
		if (this->qspi.RemainingBytes < 0) {
			this->qspi.RemainingBytes = 0;
		}

		/* Write the command to the FIFO */
		XQspiPs_WriteReg(this->qspi.Config.BaseAddress,
				 currinst.txoffset, Data);
		transcount++;

		/*
		 * If switching from TXD1/2/3 to TXD0, then start transfer and
		 * check for FIFO empty
		 */
		if(SwitchFlag) {
			SwitchFlag = false;
			/*
			 * If, in Manual Start mode, start the transfer.
			 */
			if (XQspiPs_IsManualStart(&(this->qspi))) {
				uint32_t ConfigReg = XQspiPs_ReadReg(
						this->qspi.Config.BaseAddress,
						 XQSPIPS_CR_OFFSET);
				ConfigReg |= XQSPIPS_CR_MANSTRT_MASK;
				XQspiPs_WriteReg(this->qspi.Config.BaseAddress,
						 XQSPIPS_CR_OFFSET, ConfigReg);
			}
			/*
			 * Wait for the transfer to finish by polling Tx fifo status.
			 */
			uint32_t StatusReg;
			do {
				StatusReg = XQspiPs_ReadReg(
						this->qspi.Config.BaseAddress,
						XQSPIPS_SR_OFFSET);
			} while ((StatusReg & XQSPIPS_IXR_TXOW_MASK) == 0);

		}

		this->started = true;
	}

	/*
	 * Fill the Tx FIFO with as many bytes as it takes (or as many as
	 * we have to send).
	 */
	while ((this->qspi.RemainingBytes > 0) &&
		(transcount < XQSPIPS_FIFO_DEPTH)) {
		XQspiPs_WriteReg(this->qspi.Config.BaseAddress,
				  XQSPIPS_TXD_00_OFFSET,
				  *((uint32_t *)this->qspi.SendBufferPtr));
		this->qspi.SendBufferPtr += 4;
		this->qspi.RemainingBytes -= 4;
		if (this->qspi.RemainingBytes < 0) {
			this->qspi.RemainingBytes = 0;
		}
		transcount++;
	}

	/*
	 * Enable QSPI interrupts (connecting to the interrupt controller and
	 * enabling interrupts should have been done by the caller).
	 */
	XQspiPs_WriteReg(this->qspi.Config.BaseAddress,
			  XQSPIPS_IER_OFFSET, XQSPIPS_IXR_RXNEMPTY_MASK |
			  XQSPIPS_IXR_TXOW_MASK | XQSPIPS_IXR_RXOVR_MASK |
			  XQSPIPS_IXR_TXUF_MASK);

	/*
	 * If, in Manual Start mode, Start the transfer.
	 */
	if (XQspiPs_IsManualStart(&(this->qspi))) {
		uint32_t ConfigReg = XQspiPs_ReadReg(this->qspi.Config.BaseAddress,
				XQSPIPS_CR_OFFSET);
		ConfigReg |= XQSPIPS_CR_MANSTRT_MASK;
		XQspiPs_WriteReg(this->qspi.Config.BaseAddress,
				  XQSPIPS_CR_OFFSET, ConfigReg);
	}

	// Wait for response
	unsigned int status = 0;
	if (!xQueueReceive(this->sync, &status, timeout)) {
		// Transfer failed
		return false;
	}

	if (status != XST_SPI_TRANSFER_DONE)
		return false;

	// Transfer successful
	return true;
}

void PSQSPI::select(size_t cs) {
	// Start will take effect when the instruction is sent
	this->selected = true;
}

void PSQSPI::deselect() {
	/*
	 * If the Slave select is being manually controlled,
	 * disable it because the transfer is complete.
	 */
	if (XQspiPs_IsManualChipSelect(&(this->qspi))) {
		uint32_t config_reg = XQspiPs_ReadReg( this->qspi.Config.BaseAddress, XQSPIPS_CR_OFFSET);
		config_reg |= XQSPIPS_CR_SSCTRL_MASK;
		XQspiPs_WriteReg(this->qspi.Config.BaseAddress, XQSPIPS_CR_OFFSET, config_reg);
	}

	XQspiPs_Disable((&(this->qspi)));

	// Reset the RX FIFO threshold to one
	XQspiPs_WriteReg(this->qspi.Config.BaseAddress, XQSPIPS_RXWR_OFFSET, XQSPIPS_RXWR_RESET_VALUE);

	this->selected = false;
	this->started = false;
}

static void XQspiPs_GetReadData(XQspiPs *instptr, uint32_t Data, uint8_t Size)
{
	uint8_t data_byte_3;

	if (instptr->RecvBufferPtr) {
		switch (Size) {
		case 1:
			if (instptr->ShiftReadData == 1) {
				*((uint8_t *)instptr->RecvBufferPtr) =
					((Data & 0xFF000000) >> 24);
			} else {
				*((uint8_t *)instptr->RecvBufferPtr) =
					(Data & 0xFF);
			}
			instptr->RecvBufferPtr += 1;
			break;
		case 2:
			if (instptr->ShiftReadData == 1) {
				*((uint16_t *)instptr->RecvBufferPtr) =
					((Data & 0xFFFF0000) >> 16);
			} else 	{
				*((uint16_t *)instptr->RecvBufferPtr) =
					(Data & 0xFFFF);
			}
			instptr->RecvBufferPtr += 2;
			break;
		case 3:
			if (instptr->ShiftReadData == 1) {
				*((uint16_t *)instptr->RecvBufferPtr) =
					((Data & 0x00FFFF00) >> 8);
				instptr->RecvBufferPtr += 2;
				data_byte_3 = ((Data & 0xFF000000) >> 24);
				*((uint8_t *)instptr->RecvBufferPtr) = data_byte_3;
			} else {
				*((uint16_t *)instptr->RecvBufferPtr) =
					(Data & 0xFFFF);
				instptr->RecvBufferPtr += 2;
				data_byte_3 = ((Data & 0x00FF0000) >> 16);
				*((uint8_t *)instptr->RecvBufferPtr) = data_byte_3;
			}
			instptr->RecvBufferPtr += 1;
			break;
		default:
			/* This will never execute */
			break;
		}
	}
	instptr->ShiftReadData  = 0;
	instptr->RequestedBytes -= Size;
	if (instptr->RequestedBytes < 0) {
		instptr->RequestedBytes = 0;
	}
}

void PSQSPI::_InterruptHandler() {
	uint32_t intr_status = 0;
	uint32_t config_reg = 0;
	uint32_t data = 0;
	uint32_t transcount = 0;
	uint32_t count = 0;

	/*
	 * Immediately clear the interrupts in case the ISR causes another
	 * interrupt to be generated. If we clear at the end of the ISR,
	 * we may miss newly generated interrupts. This occurs because we
	 * transmit from within the ISR, which could potentially cause another
	 * TX_EMPTY interrupt.
	 */
	intr_status = XQspiPs_ReadReg(this->qspi.Config.BaseAddress,
					  XQSPIPS_SR_OFFSET);
	XQspiPs_WriteReg(this->qspi.Config.BaseAddress, XQSPIPS_SR_OFFSET,
			  (intr_status & XQSPIPS_IXR_WR_TO_CLR_MASK));
	XQspiPs_WriteReg(this->qspi.Config.BaseAddress, XQSPIPS_IDR_OFFSET,
			XQSPIPS_IXR_TXOW_MASK | XQSPIPS_IXR_RXNEMPTY_MASK |
			XQSPIPS_IXR_RXOVR_MASK | XQSPIPS_IXR_TXUF_MASK);

	if ((intr_status & XQSPIPS_IXR_TXOW_MASK) ||
		(intr_status & XQSPIPS_IXR_RXNEMPTY_MASK)) {

		/*
		 * Rx FIFO has just reached threshold no. of entries.
		 * Read threshold no. of entries from RX FIFO
		 * Another possiblity of entering this loop is when
		 * the last byte has been transmitted and TX FIFO is empty,
		 * in which case, read all the data from RX FIFO.
		 * Always get the received data, but only fill the
		 * receive buffer if it is not null (it can be null when
		 * the device does not care to receive data).
		 */
		transcount = this->qspi.RequestedBytes - this->qspi.RemainingBytes;
		if (transcount % 4) {
			transcount = transcount/4 + 1;
		} else {
			transcount = transcount/4;
		}

		while ((count < transcount) &&
			(count < XQSPIPS_RXFIFO_THRESHOLD_OPT)) {

			if (this->qspi.RecvBufferPtr != nullptr) {
				if (this->qspi.RequestedBytes < 4) {
					data = XQspiPs_ReadReg(this->qspi.Config.BaseAddress,
						XQSPIPS_RXD_OFFSET);
					XQspiPs_GetReadData(&(this->qspi), data,
						this->qspi.RequestedBytes);
				} else {
					(*(uint32_t *)this->qspi.RecvBufferPtr) =
						XQspiPs_ReadReg(this->qspi.Config.BaseAddress,
						XQSPIPS_RXD_OFFSET);
					this->qspi.RecvBufferPtr += 4;
					this->qspi.RequestedBytes -= 4;
					if (this->qspi.RequestedBytes < 0) {
						this->qspi.RequestedBytes = 0;
					}
				}
			} else {
				XQspiPs_ReadReg(this->qspi.Config.BaseAddress,
						XQSPIPS_RXD_OFFSET);
				this->qspi.RequestedBytes -= 4;
				if (this->qspi.RequestedBytes < 0) {
					this->qspi.RequestedBytes = 0;
				}

			}
			count++;
		}
		count = 0;
		/*
		 * Interrupt asserted as TX_OW got asserted
		 * See if there is more data to send.
		 * Fill TX FIFO with RX threshold no. of entries or
		 * remaining entries (in case that is less than threshold)
		 */
		while ((this->qspi.RemainingBytes > 0) &&
			(count < XQSPIPS_RXFIFO_THRESHOLD_OPT)) {
			/*
			 * Send more data.
			 */
			XQspiPs_WriteReg(this->qspi.Config.BaseAddress,
				XQSPIPS_TXD_00_OFFSET,
				*((uint32_t *)this->qspi.SendBufferPtr));
			this->qspi.SendBufferPtr += 4;
			this->qspi.RemainingBytes -= 4;
			if (this->qspi.RemainingBytes < 0) {
				this->qspi.RemainingBytes = 0;
			}

			count++;
		}

		if ((this->qspi.RemainingBytes == 0) &&
			(this->qspi.RequestedBytes == 0)) {
			/*
			 * No more data to send.  Disable the interrupt
			 * and inform the upper layer software that the
			 * transfer is done. The interrupt will be re-enabled
			 * when another transfer is initiated.
			 */
			XQspiPs_WriteReg(this->qspi.Config.BaseAddress,
					  XQSPIPS_IDR_OFFSET,
					  XQSPIPS_IXR_RXNEMPTY_MASK |
					  XQSPIPS_IXR_TXOW_MASK |
					  XQSPIPS_IXR_RXOVR_MASK |
					  XQSPIPS_IXR_TXUF_MASK);

			/*
			 * Clear the busy flag.
			 */
			this->qspi.IsBusy = FALSE;

			uint32_t status = XST_SPI_TRANSFER_DONE;
			xQueueSendFromISR(this->sync, &status, nullptr);
		} else {
			/*
			 * Enable the TXOW interrupt.
			 */
			XQspiPs_WriteReg(this->qspi.Config.BaseAddress,
					 XQSPIPS_IER_OFFSET,
					 XQSPIPS_IXR_RXNEMPTY_MASK |
					 XQSPIPS_IXR_TXOW_MASK |
					 XQSPIPS_IXR_RXOVR_MASK |
					 XQSPIPS_IXR_TXUF_MASK);
			/*
			 * If, in Manual Start mode, start the transfer.
			 */
			if (XQspiPs_IsManualStart(&(this->qspi))) {
				config_reg = XQspiPs_ReadReg(
					this->qspi.Config.BaseAddress,
					 XQSPIPS_CR_OFFSET);
				config_reg |= XQSPIPS_CR_MANSTRT_MASK;
				XQspiPs_WriteReg(
					this->qspi.Config.BaseAddress,
					 XQSPIPS_CR_OFFSET, config_reg);
			}
		}
	}

	/*
	 * Check for overflow and underflow errors.
	 */
	if (intr_status & XQSPIPS_IXR_RXOVR_MASK) {
		this->qspi.IsBusy = FALSE;

		uint32_t status = XST_SPI_RECEIVE_OVERRUN;
		xQueueSendFromISR(this->sync, &status, nullptr);
	}

	if (intr_status & XQSPIPS_IXR_TXUF_MASK) {
		this->qspi.IsBusy = FALSE;

		uint32_t status = XST_SPI_TRANSMIT_UNDERRUN;
		xQueueSendFromISR(this->sync, &status, nullptr);
	}
}

#endif
