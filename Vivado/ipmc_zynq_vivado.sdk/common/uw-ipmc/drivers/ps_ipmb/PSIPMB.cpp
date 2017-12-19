/*
 * PSIPMBAB.cpp
 *
 *  Created on: Oct 24, 2017
 *      Author: jtikalsky
 */

#include <drivers/ps_ipmb/PSIPMB.h>

#include "PSIPMB.h"
#include "IPMC.h"
#include "xscugic.h"
#include "xiicps.h"
#include <FreeRTOS.h>
#include <semphr.h>
#include <queue.h>
#include <task.h>

/**
 * Parse a raw IPMB request message into this structure.
 *
 * \param msg      The request message to parse.
 * \param len      The length of the message to parse.
 * \return         true if parse successful and checksums valid, else false
 *
 * \note This function will not correctly parse a response message.  It will
 *       reverse the sender/receiver identities in this case.
 */
bool IPMI_MSG::parse_message(u8 *msg, u8 len) {
	if (len < 7)
		return false; // Invalid.

	this->rsSA = msg[0];
	this->netFn = msg[1] >> 2;
	this->rsLUN = msg[1] & 0x03;
	// hdr_sum == msg[2]
	this->rqSA = msg[3];
	this->rqSeq = msg[4] >> 2;
	this->rqLUN = msg[4] & 0x03;
	this->cmd = msg[5];
	this->data_len = len-7;
	for (int i = 0; i < this->data_len; ++i)
		this->data[i] = msg[6+i];

	if (ipmi_checksum(msg, 3))
		return false;
	if (ipmi_checksum(msg, len))
		return false;
	return true;
}

/**
 * Format this IPMI_MSG into a valid raw message suitable for delivery.
 *
 * \param msg     The buffer to format this message into.
 * \param maxlen  The size of the buffer to format this message into.
 * \return        -1 if error, else the length of the formatted message.
 */
int IPMI_MSG::unparse_message(u8 *msg, u8 maxlen) const {
	if (maxlen < this->data_len+7)
		return -1;

	msg[0] = this->rsSA;
	msg[1] = (this->netFn << 2) || (this->rsLUN & 0x03);
	msg[2] = ipmi_checksum(msg, 2);
	msg[3] = this->rqSA;
	msg[4] = (this->rqSeq << 2) || (this->rqLUN & 0x03);
	msg[5] = this->cmd;
	for (int i = 0; i < this->data_len; ++i)
		msg[6+i] = this->data[i];
	msg[6+this->data_len] = ipmi_checksum(msg, 6+this->data_len);
	return this->data_len + 7;
}

/**
 * Prepare a reply to this message by applying mirrored sender/recipient
 * information to the passed IPMI_MSG, modifying the NetFN to the matching
 * response NetFN, copying the command itself, etc.  The response data is up to
 * you.
 *
 * \param reply  The IPMI_MSG to initialize as a reply to this one.
 *
 * \note You may call msg.reply(msg)
 */
void IPMI_MSG::prepare_reply(IPMI_MSG &reply) const {
	u8 rsSA = this->rqSA;
	u8 rqSA = this->rsSA;
	u8 rsLUN = this->rqLUN;
	u8 rqLUN = this->rsLUN;
	reply.rsSA = rsSA;
	reply.rqSA = rqSA;
	reply.rsLUN = rsLUN;
	reply.rqLUN = rqLUN;
	reply.netFn |= 1; // Mark as response.
	reply.cmd = this->cmd;
	reply.rqSeq = this->rqSeq;
}

/**
 * Match two IPMB messages as header-identical.
 *
 * @param other The IPMB message to compare.
 * @return true if identical else false
 */
bool IPMI_MSG::match(const IPMI_MSG &other) const {
	return (this->rqSA  == other.rqSA &&
			this->rsSA  == other.rsSA &&
			this->rqLUN == other.rqLUN &&
			this->rsLUN == other.rsLUN &&
			this->rqSeq == other.rqSeq &&
			this->netFn == other.netFn &&
			this->cmd   == other.cmd);
}

/**
 * Match two IPMB messages as request/response.
 *
 * @param other The IPMB message to compare.
 * @return true if matching else false
 */
bool IPMI_MSG::match_reply(const IPMI_MSG &other) const {
	return (this->rqSA  == other.rsSA &&
			this->rsSA  == other.rqSA &&
			this->rqLUN == other.rsLUN &&
			this->rsLUN == other.rqLUN &&
			this->rqSeq == other.rqSeq &&
			this->netFn == other.netFn &&
			this->cmd   == other.cmd);
}

/**
 * Format this IPMB message for log output.
 *
 * @return The formatted IPMB message.
 */
std::string IPMI_MSG::sprintf() const {
	char buf[this->max_data_len*3 + 1] = "";
	int i = 0;
	for (i = 0; i < this->data_len; ++i)
		snprintf(buf+(i*3), 4, "%02hhx ", this->data[i]);
	if (i)
		buf[(i*3) - 1] = '\0';
	return stdsprintf("%hhd.%02hhx -> %hhd.%02hhx: %02hhx.%02hhx (seq %02hhx) [%s]",
				this->rqLUN, this->rqSA,
				// " -> "
				this->rsLUN, this->rsSA,
				// ": "
				this->netFn, this->cmd,
				/* "(seq " */ this->rqSeq /* ") " */,
				/* "[" */ buf /* "]" */);
}

static void XIicPs_VariableLengthSlaveInterruptHandler(XIicPs *InstancePtr);

static void PS_IPMB_InterruptPassthrough(PS_IPMB *ps_ipmb, u32 StatusEvent) {
	ps_ipmb->_HandleInterrupt(StatusEvent);
}

/**
 * Instantiate a PS_IPMB driver.
 *
 * \note This performs hardware setup (mainly interrupt configuration).
 *
 * \param DeviceId             The DeviceId, used for XIicPs_LookupConfig(), etc
 * \param IntrId               The interrupt ID, for configuring the GIC.
 * \param SlaveAddr            The IPMB slave address to listen on.
 */
PS_IPMB::PS_IPMB(u16 DeviceId, u32 IntrId, u8 SlaveAddr) :
		incoming_message_queue(NULL),
		messages_received(stdsprintf("ipmb0.ps_ipmb.%hu.messages_received", DeviceId)),
		invalid_messages_received(stdsprintf("ipmb0.ps_ipmb.%hu.invalid_messages_received", DeviceId)),
		incoming_messages_missed(stdsprintf("ipmb0.ps_ipmb.%hu.incoming_messages_missed", DeviceId)),
		unexpected_send_result_interrupts(stdsprintf("ipmb0.ps_ipmb.%hu.unexpected_send_result_interrupts", DeviceId)),
		SlaveAddr(SlaveAddr), IntrId(IntrId) {

	this->sendresult_q = xQueueCreate(1, sizeof(u32));
	configASSERT(this->sendresult_q);

	XIicPs_Config *Config = XIicPs_LookupConfig(DeviceId);
	configASSERT(XST_SUCCESS == XIicPs_CfgInitialize(&this->IicInst, Config, Config->BaseAddress));

	this->setup_slave();
}

PS_IPMB::~PS_IPMB() {
	vQueueDelete(this->sendresult_q);
	XScuGic_Disable(&xInterruptController, this->IntrId);
	XScuGic_Disconnect(&xInterruptController, this->IntrId);
}

/**
 * Configure the device in slave mode and initiate receiving.
 */
void PS_IPMB::setup_slave() {
	while (XIicPs_BusIsBusy(&this->IicInst))
		(void)0;

	// Stop any previous operation.
	XScuGic_Disable(&xInterruptController, this->IntrId);

	// Reset and configure the device.
	XIicPs_Reset(&this->IicInst);
	XIicPs_SetSClk(&this->IicInst, 400000);
	XIicPs_SetStatusHandler(&this->IicInst, reinterpret_cast<void*>(this), reinterpret_cast<XIicPs_IntrHandler>(PS_IPMB_InterruptPassthrough));

	// Start in Slave configuration
	this->master = false;
	XScuGic_Connect(&xInterruptController, this->IntrId, reinterpret_cast<Xil_InterruptHandler>(XIicPs_VariableLengthSlaveInterruptHandler), (void*)&this->IicInst);
	XScuGic_Enable(&xInterruptController, this->IntrId);
	XIicPs_SetupSlave(&this->IicInst, this->SlaveAddr);

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
	XScuGic_Disable(&xInterruptController, this->IntrId);

	// Reset and configure the device.
	XIicPs_Reset(&this->IicInst);
	XIicPs_SetSClk(&this->IicInst, 400000);
	XIicPs_SetStatusHandler(&this->IicInst, reinterpret_cast<void*>(this), reinterpret_cast<XIicPs_IntrHandler>(PS_IPMB_InterruptPassthrough));

	// Start in Master configuration
	this->master = true;
	XScuGic_Connect(&xInterruptController, this->IntrId, reinterpret_cast<Xil_InterruptHandler>(XIicPs_MasterInterruptHandler), (void*)&this->IicInst);
	XScuGic_Enable(&xInterruptController, this->IntrId);
}

bool PS_IPMB::send_message(IPMI_MSG &msg) {
	uint8_t msgbuf[this->i2c_bufsize];
	int msglen = msg.unparse_message(msgbuf, this->i2c_bufsize);

	this->setup_master();
	XIicPs_MasterSend(&this->IicInst, msgbuf, msglen, msg.rqSA);

	u32 isr_result;
	xQueueReceive(this->sendresult_q, &isr_result, portMAX_DELAY);

	this->setup_slave(); // Return to slave mode.

	return isr_result == XIICPS_EVENT_COMPLETE_SEND; // Return wire-level success/failure.
}

void PS_IPMB::_HandleInterrupt(u32 StatusEvent) {
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
		(void)0;
		IPMI_MSG msg;
		if (msg.parse_message(this->i2c_inbuf, this->i2c_bufsize - LeftOverBytes)) {
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
		xQueueSendFromISR(this->sendresult_q, &StatusEvent, &isrwake);
	}

	portYIELD_FROM_ISR(isrwake);
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
static void XIicPs_VariableLengthSlaveInterruptHandler(XIicPs *InstancePtr)
{
	u32 IntrStatusReg;
	u32 IsSend = 0U;
	u32 StatusEvent = 0U;
	s32 LeftOver;
	u32 BaseAddr;

	/*
	 * Assert validates the input arguments.
	 */
	Xil_AssertVoid(InstancePtr != NULL);
	Xil_AssertVoid(InstancePtr->IsReady == (u32)XIL_COMPONENT_IS_READY);

	BaseAddr = InstancePtr->Config.BaseAddress;

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
	if (InstancePtr->RecvBufferPtr == NULL) {
		IsSend = 1U;
	}

	/* Data interrupt
	 *
	 * This means master wants to do more data transfers.
	 * Also check for completion of transfer, signal upper layer if done.
	 */
	if ((u32)0U != (IntrStatusReg & XIICPS_IXR_DATA_MASK)) {
		if (IsSend != 0x0U) {
			LeftOver = TransmitFifoFill(InstancePtr);
				/*
				 * We may finish send here
				 */
				if (LeftOver == 0) {
					StatusEvent |=
						XIICPS_EVENT_COMPLETE_SEND;
				}
		} else {
			LeftOver = SlaveRecvData(InstancePtr);

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
			if (InstancePtr->SendByteCount > 0) {
				StatusEvent |= XIICPS_EVENT_ERROR;
			}else {
				StatusEvent |= XIICPS_EVENT_COMPLETE_SEND;
			}
		} else {
			LeftOver = SlaveRecvData(InstancePtr);
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
		InstancePtr->StatusHandler(InstancePtr->CallBackRef,
					   StatusEvent);
	}
}
