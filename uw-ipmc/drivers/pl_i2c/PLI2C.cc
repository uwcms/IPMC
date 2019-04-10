/*
 * PLI2C.cc
 *
 *  Created on: Feb 26, 2018
 *      Author: mpv
 */

#include "PLI2C.h"

#if XSDK_INDEXING || __has_include(<xiic.h>)

#include <libs/printf.h>
#include <libs/except.h>

/*
 * It has been observed that interrupted based IIC takes 5 times
 * more time to do read/write operations than pooling.
 * Theoretical maximum for an IP configured to work at 100kHz takes
 * 0.19ms to send/read 1-byte (19bits). Pooling takes 0.23ms for
 * this operation while interrupted based takes 0.95ms.
 * Obviously interrupt based is the way to go due to low CPU usage
 * but in a future iteration it would be worth understanding why
 * there is such a high overhead.
 * Pooling in the driver can be enabled by uncommenting the #define
 * below.
 */
#define PLI2C_USE_POOLING

void PL_I2C::_InterruptHandler() {
	XIic_InterruptHandler(&(this->IicInst));
}

static void DataHandler(PL_I2C *InstancePtr, int ByteCount)
{
	InstancePtr->wasEvent = false;
	InstancePtr->IrqStatus = ByteCount;
	xSemaphoreGiveFromISR(InstancePtr->semaphoreSync, NULL);
}

static void StatusHandler(PL_I2C *InstancePtr, int Event)
{
	/*switch (Event) {
	case XII_BUS_NOT_BUSY_EVENT:
		// Bus no longer busy
		xSemaphoreGiveFromISR(InstancePtr->semaphoreSync, NULL);
		break;
	case XII_ARB_LOST_EVENT:
		configASSERT(0);
		break;
	case XII_MASTER_READ_EVENT:
	case XII_MASTER_WRITE_EVENT:
	case XII_MASTER_READ_EVENT | XII_GENERAL_CALL_EVENT:
	case XII_MASTER_WRITE_EVENT | XII_GENERAL_CALL_EVENT:
	case XII_SLAVE_NO_ACK_EVENT:
		// TODO: When sending as slave
		configASSERT(0);
		break;
	}*/
	InstancePtr->wasEvent = true;
	InstancePtr->IrqStatus = Event;
	xSemaphoreGiveFromISR(InstancePtr->semaphoreSync, NULL);
}

PL_I2C::PL_I2C(uint16_t DeviceId, uint32_t IntrId) : InterruptBasedDriver(IntrId),
DeviceId(DeviceId) {
	XIic_Config *ConfigPtr;

	memset((void*)&(this->IicInst), 0, sizeof(IicInst));

	// Initialize the IIC driver so that it is ready to use.
	ConfigPtr = XIic_LookupConfig(this->DeviceId);
	if (ConfigPtr == NULL)
		throw except::hardware_error(stdsprintf("No XIic config available for PL_I2C(%hu, %lu)", DeviceId, IntrId));

	if (XST_SUCCESS !=
			XIic_CfgInitialize(&(this->IicInst), ConfigPtr, ConfigPtr->BaseAddress))
		throw except::hardware_error(stdsprintf("Unable to initialize PL_I2C(%hu, %lu)", DeviceId, IntrId));

	if (XST_SUCCESS != XIic_SelfTest(&(this->IicInst)))
		throw except::hardware_error(stdsprintf("Self-test failed for PL_I2C(%hu, %lu)", DeviceId, IntrId));

	// Enable dynamic mode - it seems that dynamic mode doesn't support variable length receives
	//XIic_DynamicInitialize(&(this->IicInst));

	this->semaphoreSync = xSemaphoreCreateBinary();
	configASSERT(this->semaphoreSync);

	XIic_SetSendHandler(&(this->IicInst), this, (XIic_Handler) DataHandler);
	XIic_SetRecvHandler(&(this->IicInst), this, (XIic_Handler) DataHandler);
	XIic_SetStatusHandler(&(this->IicInst), this, (XIic_StatusHandler) StatusHandler);

	// Although not used this will make sure that the status handle
	// runs when the IP is no longer busy and when arbitration is lost
	XIic_MultiMasterInclude();

	// Enable interrupts
	this->enableInterrupts();
}

PL_I2C::~PL_I2C() {
	// Stop the drive
	vSemaphoreDelete(this->semaphoreSync);
}

size_t PL_I2C::read(uint8_t addr, uint8_t *buf, size_t len, TickType_t timeout) {
#ifdef PLI2C_USE_POOLING

	return XIic_Recv(this->IicInst.BaseAddress,addr, buf, len, XIIC_STOP);

#else

	XIic_SetAddress(&(this->IicInst), XII_ADDR_TO_SEND_TYPE, addr);
	XIic_Start(&(this->IicInst));

	do {
		if (XIic_MasterRecv(&(this->IicInst), buf, len) != XST_IIC_BUS_BUSY)
			break;

		// Xilinx driver non-sense, if bus is busy when XIic_MasterRecv is called
		// then not-busy interrupt gets enabled, and so we need to wait for it until
		// we can try again.
		if (!xSemaphoreTake(this->semaphoreSync, timeout)) {
			printf("IIC read timeout");
			return 0;
		}
	} while (1);

	// Wait completion
	if (!xSemaphoreTake(this->semaphoreSync, timeout)) {
		printf("IIC read complete timeout");
		return 0;
	}

	XIic_Stop(&(this->IicInst));

	// Execute check
	if (this->wasEvent) {
		// Something went wrong
		// Check this->IrqStatus for error code if required
		return 0;
	} else {
		if (this->IrqStatus != 0)
			// Not everything was read, indicate that
			return this->IrqStatus;
		else
			return len;
	}

#endif

	return 0;
}

size_t PL_I2C::write(uint8_t addr, const uint8_t *buf, size_t len, TickType_t timeout) {
#ifdef PLI2C_USE_POOLING

	return XIic_Send(this->IicInst.BaseAddress,addr, (u8*)buf, len, XIIC_STOP);

#else

	XIic_SetAddress(&(this->IicInst), XII_ADDR_TO_SEND_TYPE, addr);
	XIic_Start(&(this->IicInst));

	do {
		if (XIic_MasterSend(&(this->IicInst), (u8*)buf, len) != XST_IIC_BUS_BUSY)
			break;

		// Xilinx driver non-sense, if bus is busy when XIic_MasterRecv is called
		// then not-busy interrupt gets enabled, and so we need to wait for it until
		// we can try again.
		if (!xSemaphoreTake(this->semaphoreSync, timeout)) {
			printf("IIC send timeout");
			return 0;
		}
	} while (1);

	// Wait completion
	if (!xSemaphoreTake(this->semaphoreSync, timeout)) {
		printf("IIC send complete timeout");
		return 0;
	}

	XIic_Stop(&(this->IicInst));

	// Execute check
	if (this->wasEvent) {
		// Something went wrong
		// Check this->IrqStatus for error code if required
		return 0;
	} else {
		if (this->IrqStatus != 0)
			// Not everything was written, indicate that
			return this->IrqStatus;
		else
			return len;
	}

#endif

	return 0;
}

#endif
