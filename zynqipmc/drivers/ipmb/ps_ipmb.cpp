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

// Only include driver if the PS IIC is detected in the BSP.
#if XSDK_INDEXING || __has_include("xiicps.h")

#include "ps_ipmb.h"
#include "xscugic.h"
#include "xiicps.h"
#include <FreeRTOS.h>
#include <semphr.h>
#include <queue.h>
#include <task.h>
#include <libs/threading.h>
#include <libs/printf.h>
#include <libs/except.h>


PSIPMB::PSIPMB(uint16_t device_id, uint16_t intr_id, uint8_t addr) : InterruptBasedDriver(intr_id),
		kIPMBAddr(addr),
		messages_received(stdsprintf("ipmb0.ps_ipmb.%hu.messages_received", device_id)),
		invalid_messages_received(stdsprintf("ipmb0.ps_ipmb.%hu.invalid_messages_received", device_id)),
		incoming_messages_missed(stdsprintf("ipmb0.ps_ipmb.%hu.incoming_messages_missed", device_id)),
		unexpected_send_result_interrupts(stdsprintf("ipmb0.ps_ipmb.%hu.unexpected_send_result_interrupts", device_id)),
		lost_transmit_interrutpts(stdsprintf("ipmb0.ps_ipmb.%hu.lost_transmit_interrupts", device_id)) {
	this->mutex = xSemaphoreCreateMutex();
	configASSERT(this->mutex);

	this->sendresult_q = xQueueCreate(1, sizeof(uint32_t));
	configASSERT(this->sendresult_q);

	XIicPs_Config *config = XIicPs_LookupConfig(device_id);
	if (XST_SUCCESS != XIicPs_CfgInitialize(&this->iic, config, config->BaseAddress)) {
		throw except::hardware_error("Unable to initialize PSIPMB(device_id=" + std::to_string(device_id) + ")");
	}

	this->setupSlave();
}

PSIPMB::~PSIPMB() {
	vQueueDelete(this->sendresult_q);
	vSemaphoreDelete(this->mutex);
}

void PSIPMB::setupSlave() {
	while (XIicPs_BusIsBusy(&this->iic)) {
		vTaskDelay(pdMS_TO_TICKS(1));
	}

	// Stop any previous operation.
	this->disableInterrupts();

	// Reset and configure the device.
	XIicPs_Reset(&this->iic);
	XIicPs_SetSClk(&this->iic, 400000);
	XIicPs_SetStatusHandler(&this->iic, reinterpret_cast<void*>(this), reinterpret_cast<XIicPs_IntrHandler>(PSIPMB::_InterruptPassthrough));

	// Start in Slave configuration
	this->master = false;
	this->enableInterrupts();
	XIicPs_SetupSlave(&this->iic, this->kIPMBAddr >> 1);

	// Start Receiving
	XIicPs_SlaveRecv(&this->iic, this->i2c_inbuf, this->i2c_bufsize);
}

void PSIPMB::setupMaster() {
	while (XIicPs_BusIsBusy(&this->iic)) {
		vTaskDelay(pdMS_TO_TICKS(1));
	}

	// Stop any previous operation.
	this->disableInterrupts();

	// Reset and configure the device.
	XIicPs_Reset(&this->iic);
	XIicPs_SetSClk(&this->iic, 400000);
	XIicPs_SetStatusHandler(&this->iic, reinterpret_cast<void*>(this), reinterpret_cast<XIicPs_IntrHandler>(PSIPMB::_InterruptPassthrough));

	// Start in Master configuration
	this->master = true;
	this->enableInterrupts();
}

bool PSIPMB::sendMessage(IPMIMessage &msg, uint32_t retry) {
	uint8_t msgbuf[this->i2c_bufsize];
	int msglen = msg.unparseMessage(msgbuf, this->i2c_bufsize);

	MutexGuard<false> lock(this->mutex, true);
	this->setupMaster();

	uint32_t isr_result;
	xQueueReceive(this->sendresult_q, &isr_result, 0); // Clear any late/delayed result.
	XIicPs_MasterSend(&this->iic, msgbuf, msglen, msg.rsSA>>1);

	if (pdFAIL == xQueueReceive(this->sendresult_q, &isr_result, pdMS_TO_TICKS(10))) {
		// We didn't get a response. Move on with our lives.
		// 32 bytes at 100MHz would have been 2.56ms, so we're well past it at 10.
		isr_result = XIICPS_EVENT_ERROR;
		this->lost_transmit_interrutpts.increment();
	}

	this->setupSlave(); // Return to slave mode.

	return isr_result == XIICPS_EVENT_COMPLETE_SEND; // Return wire-level success/failure.
}

/**
 * Helper used by the #XIicPs_VariableLengthSlaveInterruptHandler()
 * @param InstancePtr The IicPs instance pointer.
 * @return Remaining expected bytes.
 * @note This function is duplicated because it is static in xiicps_slave.c.
 */
static s32 SlaveRecvData(XIicPs *InstancePtr)
{
	uint32_t StatusReg;
	uint32_t BaseAddr;

	BaseAddr = InstancePtr->Config.BaseAddress;

	StatusReg = XIicPs_ReadReg(BaseAddr, XIICPS_SR_OFFSET);

	while (((StatusReg & XIICPS_SR_RXDV_MASK)!=0x0U) &&
			((InstancePtr->RecvByteCount > 0) != 0x0U)) {
		XIicPs_RecvByte(InstancePtr);
		StatusReg = XIicPs_ReadReg(BaseAddr, XIICPS_SR_OFFSET);
	}

	return InstancePtr->RecvByteCount;
}

void PSIPMB::_InterruptPassthrough(PSIPMB *ps_ipmb, uint32_t StatusEvent) {
	ps_ipmb->_HandleStatus(StatusEvent);
}

void PSIPMB::_InterruptHandler() {
	if (this->master) {
		XIicPs_MasterInterruptHandler(&(this->iic));
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
 * @param InstancePtr The IicPs instance pointer.
 */
void PSIPMB::_VariableLengthSlaveInterruptHandler()
{
	uint32_t IntrStatusReg;
	uint32_t IsSend = 0U;
	uint32_t StatusEvent = 0U;
	s32 LeftOver;
	uint32_t BaseAddr;

	/*
	 * Assert validates the input arguments.
	 */
	Xil_AssertVoid(this->iic.IsReady == (uint32_t)XIL_COMPONENT_IS_READY);

	BaseAddr = this->iic.Config.BaseAddress;

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
	if (this->iic.RecvBufferPtr == nullptr) {
		IsSend = 1U;
	}

	/* Data interrupt
	 *
	 * This means master wants to do more data transfers.
	 * Also check for completion of transfer, signal upper layer if done.
	 */
	if ((uint32_t)0U != (IntrStatusReg & XIICPS_IXR_DATA_MASK)) {
		if (IsSend != 0x0U) {
			LeftOver = TransmitFifoFill(&(this->iic));
				/*
				 * We may finish send here
				 */
				if (LeftOver == 0) {
					StatusEvent |=
						XIICPS_EVENT_COMPLETE_SEND;
				}
		} else {
			LeftOver = SlaveRecvData(&(this->iic));

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
			if (this->iic.SendByteCount > 0) {
				StatusEvent |= XIICPS_EVENT_ERROR;
			}else {
				StatusEvent |= XIICPS_EVENT_COMPLETE_SEND;
			}
		} else {
			LeftOver = SlaveRecvData(&(this->iic));
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
	if ((uint32_t)0U != StatusEvent) {
		this->_HandleStatus(StatusEvent);
	}
}

void PSIPMB::_HandleStatus(uint32_t StatusEvent) {
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

	uint32_t LeftOverBytes = StatusEvent >> 26;
	StatusEvent &= 0x03ffffff;

	if (StatusEvent == XIICPS_EVENT_COMPLETE_RECV) {
		IPMIMessage msg;
		if (msg.parseMessage(this->i2c_inbuf, this->i2c_bufsize - LeftOverBytes, this->kIPMBAddr)) {
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
		XIicPs_SlaveRecv(&this->iic, this->i2c_inbuf, this->i2c_bufsize); // Reset Receiver
	}

	if (this->master) {
		// Just pass this along to the sendmessage function to handle matters.
		configASSERT(pdTRUE == xQueueSendFromISR(this->sendresult_q, &StatusEvent, &isrwake));
	}

	portYIELD_FROM_ISR(isrwake);
}

#endif
