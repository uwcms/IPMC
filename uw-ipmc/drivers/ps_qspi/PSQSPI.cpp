/*
 * PSQSPI.cpp
 *
 *  Created on: Mar 19, 2019
 *      Author: mpv
 */

#include "PSQSPI.h"
#include <libs/printf.h>
#include <libs/except.h>

/**
 * This typedef defines qspi flash instruction format
 */
typedef struct {
	u8 OpCode;	/**< Operational code of the instruction */
	u8 InstSize;	/**< Size of the instruction including address bytes */
	u8 TxOffset;	/**< Register address where instruction has to be
			     written */
} XQspiPsInstFormat;

/**
 * An interrupt-based driver for the PS SPI.
 *
 * \note This performs hardware setup (including interrupt configuration).
 *
 * \param DeviceId    The DeviceId, used for XUartPs_LookupConfig(), etc
 * \param IntrId      The interrupt ID, for configuring the GIC.
 */
PS_QSPI::PS_QSPI(u16 DeviceId, u32 IntrId) : InterruptBasedDriver(IntrId), selected(false), started(false), OpMode(SINGLE) {
	this->sync = xQueueCreate(1, sizeof(unsigned int));
	configASSERT(this->sync);

	XQspiPs_Config *Config = XQspiPs_LookupConfig(DeviceId);

	if (XST_SUCCESS != XQspiPs_CfgInitialize(&this->QspiInst, Config, Config->BaseAddress))
		throw except::hardware_error(stdsprintf("Unable to initialize PS_QSPI(%hu, %lu)", DeviceId, IntrId));

	if (XST_SUCCESS != XQspiPs_SelfTest(&this->QspiInst))
		throw except::hardware_error(stdsprintf("Self-test failed for PS_QSPI(%hu, %lu)", DeviceId, IntrId));

	XQspiPs_Reset(&this->QspiInst);

	XQspiPs_SetOptions(&this->QspiInst, XQSPIPS_MANUAL_START_OPTION |
			XQSPIPS_HOLD_B_DRIVE_OPTION |
			XQSPIPS_FORCE_SSELECT_OPTION);

	XQspiPs_SetClkPrescaler(&this->QspiInst, XQSPIPS_CLK_PRESCALE_2);
}

PS_QSPI::~PS_QSPI() {
}

/**
 * This function will perform a SPI transfer in a blocking manner.
 *
 * \param chip     The chip select to enable.
 * \param sendbuf  The data to send.
 * \param recvbuf  A buffer for received data.
 * \param bytes    The number of bytes to transfer
 * \return false on error, else true
 */
bool PS_QSPI::transfer(uint8_t chip, const uint8_t *sendbuf, uint8_t *recvbuf, size_t bytes, TickType_t timeout) {
	MutexGuard<false> lock(this->mutex, true);

	this->select(chip);
	bool r = this->transfer_unsafe(sendbuf, recvbuf, bytes, timeout);
	this->deselect();

	return r;
}

bool PS_QSPI::transfer_unsafe(const uint8_t *sendbuf, uint8_t *recvbuf, size_t bytes, TickType_t timeout) {
	uint8_t Instruction;
	XQspiPsInstFormat CurrInst;
	size_t TransCount = 0;
	bool SwitchFlag = false;

	if (!this->selected)
		throw except::hardware_error("PS_QSPI::transfer_unsafe cannot be used outside atomic clause");

	/*
	 * Check whether there is another transfer in progress. Not thread-safe.
	 */
	if (this->QspiInst.IsBusy) return false;

	/*
	 * Set the busy flag, which will be cleared in the ISR when the
	 * transfer is entirely done.
	 */
	this->QspiInst.IsBusy = TRUE;

	/*
	 * Set up buffer pointers.
	 */
	this->QspiInst.SendBufferPtr = (uint8_t*)sendbuf;
	this->QspiInst.RecvBufferPtr = recvbuf;

	this->QspiInst.RequestedBytes = bytes;
	this->QspiInst.RemainingBytes = bytes;

	if (!this->started) {
		/*
		 * The first byte with every chip-select assertion is always
		 * expected to be an instruction for flash interface mode
		 */
		Instruction = *(this->QspiInst.SendBufferPtr);

		switch(bytes%4)
		{
			case XQSPIPS_SIZE_ONE:
				CurrInst.OpCode = Instruction;
				CurrInst.InstSize = XQSPIPS_SIZE_ONE;
				CurrInst.TxOffset = XQSPIPS_TXD_01_OFFSET;
				break;
			case XQSPIPS_SIZE_TWO:
				CurrInst.OpCode = Instruction;
				CurrInst.InstSize = XQSPIPS_SIZE_TWO;
				CurrInst.TxOffset = XQSPIPS_TXD_10_OFFSET;
				break;
			case XQSPIPS_SIZE_THREE:
				CurrInst.OpCode = Instruction;
				CurrInst.InstSize = XQSPIPS_SIZE_THREE;
				CurrInst.TxOffset = XQSPIPS_TXD_11_OFFSET;
				break;
			default:
				CurrInst.OpCode = Instruction;
				CurrInst.InstSize = XQSPIPS_SIZE_FOUR;
				CurrInst.TxOffset = XQSPIPS_TXD_00_OFFSET;
				break;
		}

		// Might need alignment
		if ((CurrInst.TxOffset != XQSPIPS_TXD_00_OFFSET) && (bytes > 4)) {
			SwitchFlag = true;
		}

		/*
		 * If the instruction size in not 4 bytes then the data received needs
		 * to be shifted
		 */
		if( CurrInst.InstSize != 4 ) {
			this->QspiInst.ShiftReadData = 1;
		} else {
			this->QspiInst.ShiftReadData = 0;
		}

		/*
		 * Set the RX FIFO threshold
		 */
		XQspiPs_WriteReg(this->QspiInst.Config.BaseAddress,
				XQSPIPS_RXWR_OFFSET, XQSPIPS_RXFIFO_THRESHOLD_OPT);

		/*
		 * If the slave select is "Forced" or under manual control,
		 * set the slave select now, before beginning the transfer.
		 */
		if (XQspiPs_IsManualChipSelect(&(this->QspiInst))) {
			uint32_t ConfigReg = XQspiPs_ReadReg(this->QspiInst.Config.BaseAddress,
					 XQSPIPS_CR_OFFSET);
			ConfigReg &= ~XQSPIPS_CR_SSCTRL_MASK;
			XQspiPs_WriteReg(this->QspiInst.Config.BaseAddress,
					  XQSPIPS_CR_OFFSET,
					  ConfigReg);
		}

		/*
		 * Enable the device.
		 */
		XQspiPs_Enable((&(this->QspiInst)));

		/*
		 * Clear all the interrupts.
		 */
		XQspiPs_WriteReg(this->QspiInst.Config.BaseAddress, XQSPIPS_SR_OFFSET,
				XQSPIPS_IXR_WR_TO_CLR_MASK);

		/* Get the complete command (flash inst + address/data) */
		uint32_t Data = *((uint32_t *)this->QspiInst.SendBufferPtr);
		this->QspiInst.SendBufferPtr += CurrInst.InstSize;
		this->QspiInst.RemainingBytes -= CurrInst.InstSize;
		if (this->QspiInst.RemainingBytes < 0) {
			this->QspiInst.RemainingBytes = 0;
		}

		/* Write the command to the FIFO */
		XQspiPs_WriteReg(this->QspiInst.Config.BaseAddress,
				 CurrInst.TxOffset, Data);
		TransCount++;

		/*
		 * If switching from TXD1/2/3 to TXD0, then start transfer and
		 * check for FIFO empty
		 */
		if(SwitchFlag) {
			SwitchFlag = false;
			/*
			 * If, in Manual Start mode, start the transfer.
			 */
			if (XQspiPs_IsManualStart(&(this->QspiInst))) {
				uint32_t ConfigReg = XQspiPs_ReadReg(
						this->QspiInst.Config.BaseAddress,
						 XQSPIPS_CR_OFFSET);
				ConfigReg |= XQSPIPS_CR_MANSTRT_MASK;
				XQspiPs_WriteReg(this->QspiInst.Config.BaseAddress,
						 XQSPIPS_CR_OFFSET, ConfigReg);
			}
			/*
			 * Wait for the transfer to finish by polling Tx fifo status.
			 */
			uint32_t StatusReg;
			do {
				StatusReg = XQspiPs_ReadReg(
						this->QspiInst.Config.BaseAddress,
						XQSPIPS_SR_OFFSET);
			} while ((StatusReg & XQSPIPS_IXR_TXOW_MASK) == 0);

		}

		this->started = true;
	}

	/*
	 * Fill the Tx FIFO with as many bytes as it takes (or as many as
	 * we have to send).
	 */
	while ((this->QspiInst.RemainingBytes > 0) &&
		(TransCount < XQSPIPS_FIFO_DEPTH)) {
		XQspiPs_WriteReg(this->QspiInst.Config.BaseAddress,
				  XQSPIPS_TXD_00_OFFSET,
				  *((uint32_t *)this->QspiInst.SendBufferPtr));
		this->QspiInst.SendBufferPtr += 4;
		this->QspiInst.RemainingBytes -= 4;
		if (this->QspiInst.RemainingBytes < 0) {
			this->QspiInst.RemainingBytes = 0;
		}
		TransCount++;
	}

	/*
	 * Enable QSPI interrupts (connecting to the interrupt controller and
	 * enabling interrupts should have been done by the caller).
	 */
	XQspiPs_WriteReg(this->QspiInst.Config.BaseAddress,
			  XQSPIPS_IER_OFFSET, XQSPIPS_IXR_RXNEMPTY_MASK |
			  XQSPIPS_IXR_TXOW_MASK | XQSPIPS_IXR_RXOVR_MASK |
			  XQSPIPS_IXR_TXUF_MASK);

	/*
	 * If, in Manual Start mode, Start the transfer.
	 */
	if (XQspiPs_IsManualStart(&(this->QspiInst))) {
		uint32_t ConfigReg = XQspiPs_ReadReg(this->QspiInst.Config.BaseAddress,
				XQSPIPS_CR_OFFSET);
		ConfigReg |= XQSPIPS_CR_MANSTRT_MASK;
		XQspiPs_WriteReg(this->QspiInst.Config.BaseAddress,
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

void PS_QSPI::select(uint32_t cs) {
	// Start will take effect when the instruction is sent
	this->selected = true;
}

void PS_QSPI::deselect() {
	/*
	 * If the Slave select is being manually controlled,
	 * disable it because the transfer is complete.
	 */
	if (XQspiPs_IsManualChipSelect(&(this->QspiInst))) {
		uint32_t ConfigReg = XQspiPs_ReadReg( this->QspiInst.Config.BaseAddress, XQSPIPS_CR_OFFSET);
		ConfigReg |= XQSPIPS_CR_SSCTRL_MASK;
		XQspiPs_WriteReg(this->QspiInst.Config.BaseAddress, XQSPIPS_CR_OFFSET, ConfigReg);
	}

	/*
	 * Disable the device.
	 */
	XQspiPs_Disable((&(this->QspiInst)));

	/*
	 * Reset the RX FIFO threshold to one
	 */
	XQspiPs_WriteReg(this->QspiInst.Config.BaseAddress, XQSPIPS_RXWR_OFFSET, XQSPIPS_RXWR_RESET_VALUE);

	this->selected = false;
	this->started = false;
}

/*****************************************************************************/
/**
*
* Copies data from Data to the Receive buffer.
*
* @param	InstancePtr is a pointer to the XQspiPs instance.
* @param	Data is the data which needs to be copied to the Rx buffer.
* @param	Size is the number of bytes to be copied to the Receive buffer.
*
* @return	None.
*
* @note		None.
*
******************************************************************************/
static void XQspiPs_GetReadData(XQspiPs *InstancePtr, uint32_t Data, uint8_t Size)
{
	uint8_t DataByte3;

	if (InstancePtr->RecvBufferPtr) {
		switch (Size) {
		case 1:
			if (InstancePtr->ShiftReadData == 1) {
				*((uint8_t *)InstancePtr->RecvBufferPtr) =
					((Data & 0xFF000000) >> 24);
			} else {
				*((uint8_t *)InstancePtr->RecvBufferPtr) =
					(Data & 0xFF);
			}
			InstancePtr->RecvBufferPtr += 1;
			break;
		case 2:
			if (InstancePtr->ShiftReadData == 1) {
				*((uint16_t *)InstancePtr->RecvBufferPtr) =
					((Data & 0xFFFF0000) >> 16);
			} else 	{
				*((uint16_t *)InstancePtr->RecvBufferPtr) =
					(Data & 0xFFFF);
			}
			InstancePtr->RecvBufferPtr += 2;
			break;
		case 3:
			if (InstancePtr->ShiftReadData == 1) {
				*((uint16_t *)InstancePtr->RecvBufferPtr) =
					((Data & 0x00FFFF00) >> 8);
				InstancePtr->RecvBufferPtr += 2;
				DataByte3 = ((Data & 0xFF000000) >> 24);
				*((uint8_t *)InstancePtr->RecvBufferPtr) = DataByte3;
			} else {
				*((uint16_t *)InstancePtr->RecvBufferPtr) =
					(Data & 0xFFFF);
				InstancePtr->RecvBufferPtr += 2;
				DataByte3 = ((Data & 0x00FF0000) >> 16);
				*((uint8_t *)InstancePtr->RecvBufferPtr) = DataByte3;
			}
			InstancePtr->RecvBufferPtr += 1;
			break;
		default:
			/* This will never execute */
			break;
		}
	}
	InstancePtr->ShiftReadData  = 0;
	InstancePtr->RequestedBytes -= Size;
	if (InstancePtr->RequestedBytes < 0) {
		InstancePtr->RequestedBytes = 0;
	}
}

void PS_QSPI::_InterruptHandler() {
	u32 IntrStatus;
	u32 ConfigReg;
	u32 Data;
	u32 TransCount;
	u32 Count = 0;

	/*
	 * Immediately clear the interrupts in case the ISR causes another
	 * interrupt to be generated. If we clear at the end of the ISR,
	 * we may miss newly generated interrupts. This occurs because we
	 * transmit from within the ISR, which could potentially cause another
	 * TX_EMPTY interrupt.
	 */
	IntrStatus = XQspiPs_ReadReg(this->QspiInst.Config.BaseAddress,
					  XQSPIPS_SR_OFFSET);
	XQspiPs_WriteReg(this->QspiInst.Config.BaseAddress, XQSPIPS_SR_OFFSET,
			  (IntrStatus & XQSPIPS_IXR_WR_TO_CLR_MASK));
	XQspiPs_WriteReg(this->QspiInst.Config.BaseAddress, XQSPIPS_IDR_OFFSET,
			XQSPIPS_IXR_TXOW_MASK | XQSPIPS_IXR_RXNEMPTY_MASK |
			XQSPIPS_IXR_RXOVR_MASK | XQSPIPS_IXR_TXUF_MASK);

	if ((IntrStatus & XQSPIPS_IXR_TXOW_MASK) ||
		(IntrStatus & XQSPIPS_IXR_RXNEMPTY_MASK)) {

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
		TransCount = this->QspiInst.RequestedBytes - this->QspiInst.RemainingBytes;
		if (TransCount % 4) {
			TransCount = TransCount/4 + 1;
		} else {
			TransCount = TransCount/4;
		}

		while ((Count < TransCount) &&
			(Count < XQSPIPS_RXFIFO_THRESHOLD_OPT)) {

			if (this->QspiInst.RecvBufferPtr != NULL) {
				if (this->QspiInst.RequestedBytes < 4) {
					Data = XQspiPs_ReadReg(this->QspiInst.Config.BaseAddress,
						XQSPIPS_RXD_OFFSET);
					XQspiPs_GetReadData(&(this->QspiInst), Data,
						this->QspiInst.RequestedBytes);
				} else {
					(*(u32 *)this->QspiInst.RecvBufferPtr) =
						XQspiPs_ReadReg(this->QspiInst.Config.BaseAddress,
						XQSPIPS_RXD_OFFSET);
					this->QspiInst.RecvBufferPtr += 4;
					this->QspiInst.RequestedBytes -= 4;
					if (this->QspiInst.RequestedBytes < 0) {
						this->QspiInst.RequestedBytes = 0;
					}
				}
			} else {
				XQspiPs_ReadReg(this->QspiInst.Config.BaseAddress,
						XQSPIPS_RXD_OFFSET);
				this->QspiInst.RequestedBytes -= 4;
				if (this->QspiInst.RequestedBytes < 0) {
					this->QspiInst.RequestedBytes = 0;
				}

			}
			Count++;
		}
		Count = 0;
		/*
		 * Interrupt asserted as TX_OW got asserted
		 * See if there is more data to send.
		 * Fill TX FIFO with RX threshold no. of entries or
		 * remaining entries (in case that is less than threshold)
		 */
		while ((this->QspiInst.RemainingBytes > 0) &&
			(Count < XQSPIPS_RXFIFO_THRESHOLD_OPT)) {
			/*
			 * Send more data.
			 */
			XQspiPs_WriteReg(this->QspiInst.Config.BaseAddress,
				XQSPIPS_TXD_00_OFFSET,
				*((u32 *)this->QspiInst.SendBufferPtr));
			this->QspiInst.SendBufferPtr += 4;
			this->QspiInst.RemainingBytes -= 4;
			if (this->QspiInst.RemainingBytes < 0) {
				this->QspiInst.RemainingBytes = 0;
			}

			Count++;
		}

		if ((this->QspiInst.RemainingBytes == 0) &&
			(this->QspiInst.RequestedBytes == 0)) {
			/*
			 * No more data to send.  Disable the interrupt
			 * and inform the upper layer software that the
			 * transfer is done. The interrupt will be re-enabled
			 * when another transfer is initiated.
			 */
			XQspiPs_WriteReg(this->QspiInst.Config.BaseAddress,
					  XQSPIPS_IDR_OFFSET,
					  XQSPIPS_IXR_RXNEMPTY_MASK |
					  XQSPIPS_IXR_TXOW_MASK |
					  XQSPIPS_IXR_RXOVR_MASK |
					  XQSPIPS_IXR_TXUF_MASK);

			/*
			 * Clear the busy flag.
			 */
			this->QspiInst.IsBusy = FALSE;

			uint32_t status = XST_SPI_TRANSFER_DONE;
			xQueueSendFromISR(this->sync, &status, NULL);
		} else {
			/*
			 * Enable the TXOW interrupt.
			 */
			XQspiPs_WriteReg(this->QspiInst.Config.BaseAddress,
					 XQSPIPS_IER_OFFSET,
					 XQSPIPS_IXR_RXNEMPTY_MASK |
					 XQSPIPS_IXR_TXOW_MASK |
					 XQSPIPS_IXR_RXOVR_MASK |
					 XQSPIPS_IXR_TXUF_MASK);
			/*
			 * If, in Manual Start mode, start the transfer.
			 */
			if (XQspiPs_IsManualStart(&(this->QspiInst))) {
				ConfigReg = XQspiPs_ReadReg(
					this->QspiInst.Config.BaseAddress,
					 XQSPIPS_CR_OFFSET);
				ConfigReg |= XQSPIPS_CR_MANSTRT_MASK;
				XQspiPs_WriteReg(
					this->QspiInst.Config.BaseAddress,
					 XQSPIPS_CR_OFFSET, ConfigReg);
			}
		}
	}

	/*
	 * Check for overflow and underflow errors.
	 */
	if (IntrStatus & XQSPIPS_IXR_RXOVR_MASK) {
		this->QspiInst.IsBusy = FALSE;

		uint32_t status = XST_SPI_RECEIVE_OVERRUN;
		xQueueSendFromISR(this->sync, &status, NULL);
	}

	if (IntrStatus & XQSPIPS_IXR_TXUF_MASK) {
		this->QspiInst.IsBusy = FALSE;

		uint32_t status = XST_SPI_TRANSMIT_UNDERRUN;
		xQueueSendFromISR(this->sync, &status, NULL);
	}
}


