/*
 * PSIPMBAB.cpp
 *
 *  Created on: Oct 24, 2017
 *      Author: jtikalsky
 */

#include <drivers/ipmb/PSIPMB.h>

#include "PSIPMB.h"
#include "IPMC.h"
#include "xscugic.h"
#include "xiicps.h"
#include <FreeRTOS.h>
#include <semphr.h>
#include <queue.h>
#include <task.h>
#include <libs/ThreadingPrimitives.h>
#include <libs/printf.h>
#include <libs/except.h>


/**
 * Instantiate a PS_IPMB driver.
 *
 * \note This performs hardware setup (mainly interrupt configuration).
 *
 * \param DeviceId             The DeviceId, used for XIicPs_LookupConfig(), etc
 * \param IntrId               The interrupt ID, for configuring the GIC.
 * \param IPMBAddr             The IPMB slave address to listen on.
 */
PS_IPMB::PS_IPMB(u16 DeviceId, u32 IntrId, u8 IPMBAddr) : InterruptBasedDriver(IntrId),
		IPMBAddr(IPMBAddr), messages_received(stdsprintf("ipmb0.ps_ipmb.%hu.messages_received", DeviceId)),
		invalid_messages_received(stdsprintf("ipmb0.ps_ipmb.%hu.invalid_messages_received", DeviceId)),
		incoming_messages_missed(stdsprintf("ipmb0.ps_ipmb.%hu.incoming_messages_missed", DeviceId)),
		unexpected_send_result_interrupts(stdsprintf("ipmb0.ps_ipmb.%hu.unexpected_send_result_interrupts", DeviceId)) {
	this->mutex = xSemaphoreCreateMutex();
	configASSERT(this->mutex);

	this->sendresult_q = xQueueCreate(1, sizeof(u32));
	configASSERT(this->sendresult_q);

	XIicPs_Config *Config = XIicPs_LookupConfig(DeviceId);
	if (XST_SUCCESS != XIicPs_CfgInitialize(&this->IicInst, Config, Config->BaseAddress))
		throw except::hardware_error(stdsprintf("Unable to initialize PS I2C for PS_IPMB(%hu, %u, %hhu)", DeviceId, IntrId, IPMBAddr));

	this->setup_slave();
}

PS_IPMB::~PS_IPMB() {
	vQueueDelete(this->sendresult_q);
	vSemaphoreDelete(this->mutex);
}

/**
 * Configure the device in slave mode and initiate receiving.
 */
void PS_IPMB::setup_slave() {
	while (XIicPs_BusIsBusy(&this->IicInst)) {
		vTaskDelay(pdMS_TO_TICKS(200));
	}

	// Stop any previous operation.
	this->disableInterrupts();

	// Reset and configure the device.
	XIicPs_Reset(&this->IicInst);
	XIicPs_SetSClk(&this->IicInst, 400000);
	XIicPs_SetStatusHandler(&this->IicInst, reinterpret_cast<void*>(this), reinterpret_cast<XIicPs_IntrHandler>(PS_IPMB::_InterruptPassthrough));

	// Start in Slave configuration
	this->master = false;
	this->enableInterrupts();
	XIicPs_SetupSlave(&this->IicInst, this->IPMBAddr >> 1);

	// Start Receiving
	XIicPs_SlaveRecv(&this->IicInst, this->i2c_inbuf, this->i2c_bufsize);
}

/**
 * Configure the device in master mode.
 */
void PS_IPMB::setup_master() {
	while (XIicPs_BusIsBusy(&this->IicInst))
		(void)0;

	// Stop any previous operation.
	this->disableInterrupts();

	// Reset and configure the device.
	XIicPs_Reset(&this->IicInst);
	XIicPs_SetSClk(&this->IicInst, 400000);
	XIicPs_SetStatusHandler(&this->IicInst, reinterpret_cast<void*>(this), reinterpret_cast<XIicPs_IntrHandler>(PS_IPMB::_InterruptPassthrough));

	// Start in Master configuration
	this->master = true;
	this->enableInterrupts();
}

/**
 * This function will send a message out on the IPMB in a blocking manner.
 *
 * \param msg   The IPMI_MSG to deliver.
 * \param retry The retry counter for this message.
 * \return      true if message was delivered else false
 */
bool PS_IPMB::send_message(IPMI_MSG &msg, uint32_t retry) {
	uint8_t msgbuf[this->i2c_bufsize];
	int msglen = msg.unparse_message(msgbuf, this->i2c_bufsize);

	MutexGuard<false> lock(this->mutex, true);
	this->setup_master();
	XIicPs_MasterSend(&this->IicInst, msgbuf, msglen, msg.rsSA>>1);

	u32 isr_result;
	xQueueReceive(this->sendresult_q, &isr_result, portMAX_DELAY);

	this->setup_slave(); // Return to slave mode.

	return isr_result == XIICPS_EVENT_COMPLETE_SEND; // Return wire-level success/failure.
}


/**
 * Helper used by the #XIicPs_VariableLengthSlaveInterruptHandler()
 *
 * \note This function is duplicated because it is static in xiicps_slave.c
 *
 * @param InstancePtr The IicPs instance pointer.
 * @return Remaining expected bytes.
 */
static s32 SlaveRecvData(XIicPs *InstancePtr)
{
	u32 StatusReg;
	u32 BaseAddr;

	BaseAddr = InstancePtr->Config.BaseAddress;

	StatusReg = XIicPs_ReadReg(BaseAddr, XIICPS_SR_OFFSET);

	while (((StatusReg & XIICPS_SR_RXDV_MASK)!=0x0U) &&
			((InstancePtr->RecvByteCount > 0) != 0x0U)) {
		XIicPs_RecvByte(InstancePtr);
		StatusReg = XIicPs_ReadReg(BaseAddr, XIICPS_SR_OFFSET);
	}

	return InstancePtr->RecvByteCount;
}

void PS_IPMB::_InterruptPassthrough(PS_IPMB *ps_ipmb, u32 StatusEvent) {
	ps_ipmb->_HandleStatus(StatusEvent);
}

void PS_IPMB::_InterruptHandler() {
	if (this->master) {
		XIicPs_MasterInterruptHandler(&(this->IicInst));
	} else {
		this->_VariableLengthSlaveInterruptHandler();
	}
}

/**
 * This is a duplicate of the XIicPs_SlaveInterruptHandler with one exception:
 * It does not treat "receive buffer not filled completely" as an error.  This
 * allows it to receive IPMB messages without requiring that the length of the
 * message is known at listen time.
 *
 * We consider it an error, not if the buffer is not completely filled, but if
 * it IS completely filled.  (This would imply an IPMI message was longer than
 * the specification would allow, as we intend to use slightly excessive
 * buffers.)
 *
 * We also will need to know the length of the message received.  Rather than
 * writing too much code into this, we will pass out the LeftOver bytes count
 * from the original function as the top 6 bits of the status value, which were
 * otherwise unused.
 *
 * \param InstancePtr The IicPs instance pointer.
 */
void PS_IPMB::_VariableLengthSlaveInterruptHandler()
{
	u32 IntrStatusReg;
	u32 IsSend = 0U;
	u32 StatusEvent = 0U;
	s32 LeftOver;
	u32 BaseAddr;

	/*
	 * Assert validates the input arguments.
	 */
	Xil_AssertVoid(this->IicInst.IsReady == (u32)XIL_COMPONENT_IS_READY);

	BaseAddr = this->IicInst.Config.BaseAddress;

	/*
	 * Read the Interrupt status register.
	 */
	IntrStatusReg = XIicPs_ReadReg(BaseAddr, XIICPS_ISR_OFFSET);

	/*
	 * Write the status back to clear the interrupts so no events are missed
	 * while processing this interrupt.
	 */
	XIicPs_WriteReg(BaseAddr, XIICPS_ISR_OFFSET, IntrStatusReg);

	/*
	 * Use the Mask register AND with the Interrupt Status register so
	 * disabled interrupts are not processed.
	 */
	IntrStatusReg &= ~(XIicPs_ReadReg(BaseAddr, XIICPS_IMR_OFFSET));

	/*
	 * Determine whether the device is sending.
	 */
	if (this->IicInst.RecvBufferPtr == NULL) {
		IsSend = 1U;
	}

	/* Data interrupt
	 *
	 * This means master wants to do more data transfers.
	 * Also check for completion of transfer, signal upper layer if done.
	 */
	if ((u32)0U != (IntrStatusReg & XIICPS_IXR_DATA_MASK)) {
		if (IsSend != 0x0U) {
			LeftOver = TransmitFifoFill(&(this->IicInst));
				/*
				 * We may finish send here
				 */
				if (LeftOver == 0) {
					StatusEvent |=
						XIICPS_EVENT_COMPLETE_SEND;
				}
		} else {
			LeftOver = SlaveRecvData(&(this->IicInst));

			/* We may finish the receive here */
			if (LeftOver == 0) {
				//UW//StatusEvent |= XIICPS_EVENT_COMPLETE_RECV;
				StatusEvent |= XIICPS_EVENT_ERROR; //UW//
			}
		}
	}

	/*
	 * Complete interrupt.
	 *
	 * In slave mode, it means the master has done with this transfer, so
	 * we signal the application using completion event.
	 */
	if (0U != (IntrStatusReg & XIICPS_IXR_COMP_MASK)) {
		if (IsSend != 0x0U) {
			if (this->IicInst.SendByteCount > 0) {
				StatusEvent |= XIICPS_EVENT_ERROR;
			}else {
				StatusEvent |= XIICPS_EVENT_COMPLETE_SEND;
			}
		} else {
			LeftOver = SlaveRecvData(&(this->IicInst));
			if (LeftOver > 0) {
				//UW//StatusEvent |= XIICPS_EVENT_ERROR;
				StatusEvent |= (LeftOver << 26) | XIICPS_EVENT_COMPLETE_RECV; //UW//
			} else {
				//UW//StatusEvent |= XIICPS_EVENT_COMPLETE_RECV;
				StatusEvent |= XIICPS_EVENT_ERROR; //UW//
			}
		}
	}

	/*
	 * Nack interrupt, pass this information to application.
	 */
	if (0U != (IntrStatusReg & XIICPS_IXR_NACK_MASK)) {
		StatusEvent |= XIICPS_EVENT_NACK;
	}

	/*
	 * All other interrupts are treated as error.
	 */
	if (0U != (IntrStatusReg & (XIICPS_IXR_TO_MASK |
				XIICPS_IXR_RX_UNF_MASK |
				XIICPS_IXR_TX_OVR_MASK |
				XIICPS_IXR_RX_OVR_MASK))){
		StatusEvent |= XIICPS_EVENT_ERROR;
	}

	/*
	 * Signal application if there are any events.
	 */
	if ((u32)0U != StatusEvent) {
		this->_HandleStatus(StatusEvent);
	}
}

void PS_IPMB::_HandleStatus(u32 StatusEvent) {
	//#define XIICPS_EVENT_COMPLETE_SEND	0x0001U  /**< Transmit Complete Event*/
	//#define XIICPS_EVENT_COMPLETE_RECV	0x0002U  /**< Receive Complete Event*/
	//#define XIICPS_EVENT_TIME_OUT		0x0004U  /**< Transfer timed out */
	//#define XIICPS_EVENT_ERROR			0x0008U  /**< Receive error */
	//#define XIICPS_EVENT_ARB_LOST		0x0010U  /**< Arbitration lost */
	//#define XIICPS_EVENT_NACK			0x0020U  /**< NACK Received */
	//#define XIICPS_EVENT_SLAVE_RDY		0x0040U  /**< Slave ready */
	//#define XIICPS_EVENT_RX_OVR			0x0080U  /**< RX overflow */
	//#define XIICPS_EVENT_TX_OVR			0x0100U  /**< TX overflow */
	//#define XIICPS_EVENT_RX_UNF			0x0200U  /**< RX underflow */

	BaseType_t isrwake = pdFALSE;

	u32 LeftOverBytes = StatusEvent >> 26;
	StatusEvent &= 0x03ffffff;

	if (StatusEvent == XIICPS_EVENT_COMPLETE_RECV) {
		IPMI_MSG msg;
		if (msg.parse_message(this->i2c_inbuf, this->i2c_bufsize - LeftOverBytes, this->IPMBAddr)) {
			BaseType_t ret = pdFALSE;
			if (this->incoming_message_queue)
				ret = xQueueSendFromISR(this->incoming_message_queue, &msg, &isrwake);
			if (ret == pdTRUE)
				this->messages_received.increment();
			else
				this->incoming_messages_missed.increment();
		}
		else {
			this->invalid_messages_received.increment();
		}
		XIicPs_SlaveRecv(&this->IicInst, this->i2c_inbuf, this->i2c_bufsize); // Reset Receiver
	}

	if (this->master) {
		// Just pass this along to the sendmessage function to handle matters.
		configASSERT(pdTRUE == xQueueSendFromISR(this->sendresult_q, &StatusEvent, &isrwake));
	}

	portYIELD_FROM_ISR(isrwake);
}
