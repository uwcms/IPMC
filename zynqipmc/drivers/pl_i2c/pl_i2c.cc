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

// Only include driver if PL IIC is detected in the BSP.
#if XSDK_INDEXING || __has_include("xiic.h")

#include "pl_i2c.h"
#include <libs/except.h>

// TODO: Driver seems to hang from time to time, needs further debugging.
// TODO: Remove printfs after debugging.

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
//#define PLI2C_USE_POOLING

void PLI2C::_InterruptHandler() {
	XIic_InterruptHandler(&(this->iic));
}

void PLI2C::dataHandler(PLI2C *i2c, int byte_count)
{
	i2c->was_event = false;
	i2c->irqstatus = byte_count;
	xSemaphoreGiveFromISR(i2c->sync, nullptr);
}

void PLI2C::statusHandler(PLI2C *i2c, int event)
{
	/*switch (Event) {
	case XII_BUS_NOT_BUSY_EVENT:
		// Bus no longer busy
		xSemaphoreGiveFromISR(InstancePtr->sync, nullptr);
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
	i2c->was_event = true;
	i2c->irqstatus = event;
	xSemaphoreGiveFromISR(i2c->sync, nullptr);
}

PLI2C::PLI2C(uint16_t device_id, uint16_t intr_id) :
InterruptBasedDriver(intr_id) {
	XIic_Config *config;
	// Initialize the IIC driver so that it is ready to use.
	config = XIic_LookupConfig(device_id);
	if (config == nullptr) {
		throw except::hardware_error("Unable find configuration for PLI2C(device_id=" + std::to_string(device_id) + ")");
	}

	if (XST_SUCCESS != XIic_CfgInitialize(&(this->iic), config, config->BaseAddress)) {
		throw except::hardware_error("Unable to initialize PLI2C(device_id=" + std::to_string(device_id) + ")");
	}

	if (XST_SUCCESS != XIic_SelfTest(&(this->iic))) {
		throw except::hardware_error("Self-test failed for PLI2C(device_id=" + std::to_string(device_id) + ")");
	}

	// Enable dynamic mode - it seems that dynamic mode doesn't support variable length receives
	//XIic_DynamicInitialize(&(this->iic));

	this->sync = xSemaphoreCreateBinary();
	configASSERT(this->sync);

	XIic_SetSendHandler(&(this->iic), this, (XIic_Handler) PLI2C::dataHandler);
	XIic_SetRecvHandler(&(this->iic), this, (XIic_Handler) PLI2C::dataHandler);
	XIic_SetStatusHandler(&(this->iic), this, (XIic_StatusHandler) PLI2C::statusHandler);

	// Although not used this will make sure that the status handle
	// runs when the IP is no longer busy and when arbitration is lost
	XIic_MultiMasterInclude();

	// Enable interrupts
	this->enableInterrupts();
}

PLI2C::~PLI2C() {
	// Stop the drive
	vSemaphoreDelete(this->sync);
}

size_t PLI2C::read(uint8_t addr, uint8_t *buf, size_t len, TickType_t timeout, bool repeatedStart) {
	MutexGuard<true> lock(this->mutex);
#ifdef PLI2C_USE_POOLING

	uint8_t option = (repeatedStart)? XIIC_CR_REPEATED_START_MASK : XIIC_STOP;

	return XIic_Recv(this->iic.BaseAddress, addr, buf, len, option);

#else

	XIic_SetAddress(&(this->iic), XII_ADDR_TO_SEND_TYPE, addr);

	if (!(this->iic.Options & XII_REPEATED_START_OPTION))
		XIic_Start(&(this->iic));

	if (repeatedStart) this->iic.Options |= XII_REPEATED_START_OPTION;
	else this->iic.Options &= ~XII_REPEATED_START_OPTION;

	do {
		if (XIic_MasterRecv(&(this->iic), buf, len) != XST_IIC_BUS_BUSY)
			break;

		// Xilinx driver non-sense, if bus is busy when XIic_MasterRecv is called
		// then not-busy interrupt gets enabled, and so we need to wait for it until
		// we can try again.
		if (!xSemaphoreTake(this->sync, timeout)) {
			printf("IIC read timeout");
			return 0;
		}
	} while (1);

	// Wait completion
	if (!xSemaphoreTake(this->sync, timeout)) {
		printf("IIC read complete timeout");
		return 0;
	}

	if (!repeatedStart)
		XIic_Stop(&(this->iic));

	// Execute check
	if (this->was_event) {
		// Something went wrong
		// Check this->irqstatus for error code if required
		printf("Bad I2C reply: %d", this->irqstatus);
		return 0;
	} else {
		if (this->irqstatus != 0)
			// Not everything was read, indicate that
			return this->irqstatus;
		else
			return len;
	}

#endif

	return 0;
}

size_t PLI2C::write(uint8_t addr, const uint8_t *buf, size_t len, TickType_t timeout, bool repeatedStart) {
	MutexGuard<true> lock(this->mutex);
#ifdef PLI2C_USE_POOLING

	uint8_t option = (repeatedStart)? XIIC_CR_REPEATED_START_MASK : XIIC_STOP;

	return XIic_Send(this->iic.BaseAddress,addr, (uint8_t*)buf, len, option);

#else

	XIic_SetAddress(&(this->iic), XII_ADDR_TO_SEND_TYPE, addr);

	if (!(this->iic.Options & XII_REPEATED_START_OPTION))
		XIic_Start(&(this->iic));

	if (repeatedStart) this->iic.Options |= XII_REPEATED_START_OPTION;
	else this->iic.Options &= ~XII_REPEATED_START_OPTION;

	do {
		if (XIic_MasterSend(&(this->iic), (uint8_t*)buf, len) != XST_IIC_BUS_BUSY)
			break;

		// Xilinx driver non-sense, if bus is busy when XIic_MasterRecv is called
		// then not-busy interrupt gets enabled, and so we need to wait for it until
		// we can try again.
		if (!xSemaphoreTake(this->sync, timeout)) {
			printf("IIC send timeout");
			return 0;
		}
	} while (1);

	// Wait completion
	if (!xSemaphoreTake(this->sync, timeout)) {
		printf("IIC send complete timeout");
		return 0;
	}

	if (!repeatedStart) XIic_Stop(&(this->iic));

	// Execute check
	if (this->was_event) {
		// Something went wrong
		// Check this->irqstatus for error code if required
		printf("Bad I2C reply: %d", this->irqstatus);
		return 0;
	} else {
		if (this->irqstatus != 0) {
			// Not everything was written, indicate that
			return this->irqstatus;
		} else {
			return len;
		}
	}

#endif

	return 0;
}

#endif
