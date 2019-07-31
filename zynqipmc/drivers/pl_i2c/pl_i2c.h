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

#ifndef SRC_COMMON_ZYNQIPMC_DRIVERS_PLI2C_PLI2C_H_
#define SRC_COMMON_ZYNQIPMC_DRIVERS_PLI2C_PLI2C_H_

// Only include driver if PL IIC is detected in the BSP.
#if XSDK_INDEXING || __has_include("xiic.h")

#include "xiic.h"
#include <FreeRTOS.h>
#include <semphr.h>
#include <drivers/generics/i2c.h>
#include <drivers/interrupt_based_driver.h>

/**
 * A FreeRTOS interrupt based driver for Xilinx IIC IP core.
 *
 * Simple IIC wrapper supporting interrupts and I2C master operations.
 */
class PLI2C final : public I2C, protected InterruptBasedDriver {
public:
	/**
	 * Constructs and initializes the PL SPI driver.
	 * @param device_id Device ID, normally XPAR_AXI_IIC_<>_DEVICE_ID. Check xparameters.h.
	 * @param intr_id Device interrupt ID from GIC, normally XPAR_FABRIC_AXI_IIC_<>_IIC2INTC_IRPT_INTR. Check xparameters.h.
	 * @throw except::hardware_error if low level driver fails to configure.
	 */
	PLI2C(uint16_t device_id, uint16_t intr_id);
	virtual ~PLI2C();

	// From base class I2C:
	virtual size_t read(uint8_t addr, uint8_t *buf, size_t len, TickType_t timeout = portMAX_DELAY, bool repeatedStart = false);
	virtual size_t write(uint8_t addr, const uint8_t *buf, size_t len, TickType_t timeout = portMAX_DELAY, bool repeatedStart = false);

private:
	void _InterruptHandler();
	static void dataHandler(PLI2C *i2c, int byte_count);
	static void statusHandler(PLI2C *i2c, int event);

	XIic iic;						///< IIC low-level driver data.
	SemaphoreHandle_t sync;			///< Synchronization flag for interrupts.
	volatile uint32_t irqstatus;	///< IRQ status info.
	volatile bool was_event;		///< Indicates if IRQ was an event or data.
};

#endif

#endif /* SRC_COMMON_ZYNQIPMC_DRIVERS_PLI2C_PLI2C_H_ */
