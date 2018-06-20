/*
 * PLI2C.cc
 *
 *  Created on: Feb 26, 2018
 *      Author: mpv
 */

#include <drivers/pl_i2c/PLI2C.h>
#include "IPMC.h"
#include <xiic.h>

static void XIic_HandlerIRQ(void *pl_i2c) {
	reinterpret_cast<PL_I2C*>(pl_i2c)->_HandleIRQ();
}

/**
 * Instantiate a PL_I2C driver.
 *
 * \note This performs hardware setup (mainly interrupt configuration).
 *
 * \param DeviceId             The DeviceId, used for XIicPs_LookupConfig(), etc
 * \param IntrId               The interrupt ID, for configuring the GIC.
 */
PL_I2C::PL_I2C(u16 DeviceId, u32 IntrId) :
DeviceId(DeviceId), IntrId(IntrId) {
	XIic_Config *ConfigPtr;

	memset((void*)&(this->IicInst), 0, sizeof(IicInst));
	memset((void*)&(this->HandlerInfo), 0, sizeof(PL_I2C::HandlerInfo));

	// Initialize the IIC driver so that it is ready to use.
	ConfigPtr = XIic_LookupConfig(this->DeviceId);
	configASSERT(ConfigPtr != NULL);

	configASSERT(XST_SUCCESS ==
			XIic_CfgInitialize(&(this->IicInst), ConfigPtr, ConfigPtr->BaseAddress));

	configASSERT(XST_SUCCESS == XIic_SelfTest(&(this->IicInst)));

	// Enable dynamic mode - it seems that dynamic mode doesn't support variable length receives
	//XIic_DynamicInitialize(&(this->IicInst));

	// Create the required mutex to keep things safe
	this->Mutex = xSemaphoreCreateMutex();
	configASSERT(this->Mutex);

	this->IRQ_q = xQueueCreate(1, 0);
	configASSERT(this->IRQ_q);

	// Enable interrupts
	configASSERT(XST_SUCCESS ==
			XScuGic_Connect(&xInterruptController, this->IntrId, XIic_HandlerIRQ, (void*)&(this->IicInst)));
	XScuGic_Enable(&xInterruptController, this->IntrId);
}

PL_I2C::~PL_I2C() {
	// Stop the drive
	vSemaphoreDelete(this->Mutex);

	// Disable the interrupts associated with IIC
	XScuGic_Disconnect(&xInterruptController, this->IntrId);
	XScuGic_Disable(&xInterruptController, this->IntrId);
}

size_t PL_I2C::read(u8 addr, u8 *buf, size_t len, TickType_t timeout) {
	size_t r = 0;

	xSemaphoreTake(this->Mutex, portMAX_DELAY);

	this->HandlerInfo.RecvBytesUpdated = false;
	this->HandlerInfo.EventStatusUpdated = false;

	XIic_SetAddress(&(this->IicInst), XII_ADDR_TO_SEND_TYPE, addr);

	XIic_Start(&(this->IicInst));
	if (XIic_MasterRecv(&(this->IicInst), buf, len) == XST_IIC_BUS_BUSY) {
		// Busy
		printf("IIC read is busy");
		xSemaphoreGive(this->Mutex);
		return 0;
	}

	if (!xQueueReceive(this->IRQ_q, NULL, timeout)) {
		// Timeout
		printf("IIC read timeout");
		xSemaphoreGive(this->Mutex);
		return 0;
	}

	if (XIic_IsIicBusy(&(this->IicInst)) == TRUE) {
		printf("IIC read got busy");
		XIic_Stop(&(this->IicInst));
		xSemaphoreGive(this->Mutex);
		return 0;
	}

	if (this->HandlerInfo.EventStatusUpdated) {
		printf("IIC read Failed");
		r = 0;
	}

	if (this->HandlerInfo.RecvBytesUpdated) {
		r = len - this->HandlerInfo.RemainingRecvBytes;
	}

	XIic_Stop(&(this->IicInst));

	xSemaphoreGive(this->Mutex);

	return r;

	//return XIic_Recv(this->IicInst.BaseAddress,addr, buf, len, XIIC_STOP);
}

size_t PL_I2C::write(u8 addr, const u8 *buf, size_t len, TickType_t timeout) {
	size_t r = 0;

	xSemaphoreTake(this->Mutex, portMAX_DELAY);


	// TODO: Check if device is busy

	// Write address to TX FIFO
	XIic_WriteReg(this->IicInst.BaseAddress, XIIC_DTR_REG_OFFSET, 0x7F & addr);

	// Write data to TX FIFO
	// TODO: throttle data if FIFO is not big enough
	for (unsigned int i = 0; i < len; i++) {
		XIic_WriteReg(this->IicInst.BaseAddress, XIIC_DTR_REG_OFFSET, buf[i]);
	}

	XIic_WriteReg(this->IicInst.BaseAddress, XIIC_CR_REG_OFFSET, XIIC_CR_MSMS_MASK | XIIC_CR_DIR_IS_TX_MASK);


	/*this->HandlerInfo.SendBytesUpdated = false;
	this->HandlerInfo.EventStatusUpdated = false;

	XIic_SetAddress(&(this->IicInst), XII_ADDR_TO_SEND_TYPE, addr);

	XIic_Start(&(this->IicInst));
	if (XIic_MasterSend(&(this->IicInst), (u8*)buf, len) == XST_IIC_BUS_BUSY) {
		// TODO: This is an error condition, IIC is busy, so data cannot be send
		printf("IIC write is busy");
		xSemaphoreGive(this->Mutex);
		return 0;
	}

	if (!xQueueReceive(this->IRQ_q, NULL, timeout)) {
		// Timeout
		printf("IIC write timeout");
		xSemaphoreGive(this->Mutex);
		return 0;
	}

	if (XIic_IsIicBusy(&(this->IicInst)) == TRUE) {
		// TODO: This is an error condition, IIC got locked in busy (unknown why), so data cannot be send
		// Inertial delays are likely too low and the IIC core locked
		printf("IIC write got busy");
		xSemaphoreGive(this->Mutex);
		return 0;
	}

	if (this->HandlerInfo.EventStatusUpdated) {
		printf("IIC write Failed");
		r = 0;
	}

	if (this->HandlerInfo.SendBytesUpdated) {
		r = len - this->HandlerInfo.RemainingSendBytes;
	}

	XIic_Stop(&(this->IicInst));*/

	xSemaphoreGive(this->Mutex);

	return r;
	//return XIic_Send(this->IicInst.BaseAddress,addr, (u8*)buf, len, XIIC_STOP);
}

void PL_I2C::_HandleIRQ() {

}
