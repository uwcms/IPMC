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

#ifndef SRC_COMMON_ZYNQIPMC_DRIVERS_PL_SPI_PL_SPI_H_
#define SRC_COMMON_ZYNQIPMC_DRIVERS_PL_SPI_PL_SPI_H_

// Only include driver if PL SPI is detected in the BSP.
#if XSDK_INDEXING || __has_include("xspi.h")

#include "xspi.h"
#include <drivers/generics/spi.h>
#include <drivers/interrupt_based_driver.h>
#include <queue.h>

// TODO: Consider supporting dual or quad SPI as well by default.

/**
 * Driver for Xilinx AXI QSPI IP using interrupts.
 * Only single mode and master is supported.
 */
class PLSPI final : public SPIMaster, protected InterruptBasedDriver {
public:
	/**
	 * Constructs and initializes the PL SPI driver.
	 * @param device_id Device ID, normally XPAR_AXI_QUAD_SPI_<>_DEVICE_ID. Check xparameters.h.
	 * @param intr_id Device interrupt ID from GIC, normally XPAR_FABRIC_AXI_QUAD_SPI_<>_IP2INTC_IRPT_INTR. Check xparameters.h.
	 * @throw except::hardware_error if low level driver fails to configure.
	 */
	PLSPI(uint16_t device_id, uint16_t intr_id);
	virtual ~PLSPI();

	// From base class SPIMaster:
	virtual bool transfer(size_t chip, const uint8_t *sendbuf, uint8_t *recvbuf, size_t bytes, TickType_t timeout = portMAX_DELAY);
	virtual bool transfer(const uint8_t *sendbuf, uint8_t *recvbuf, size_t bytes, TickType_t timeout = portMAX_DELAY);
	virtual bool isQuadSupported() const { return false; };
	virtual void select(size_t cs);
	virtual void deselect();

private:
	void start();	///< Used to activate the SPI bus, useful when the multiple masters share the bus.
	void stop();	///< Used to deactivate the SPI bus, allowing other masters to also drive the bus.

	void _InterruptHandler();

	XSpi xspi;			///< Low-level SPI driver data.
	QueueHandle_t sync;	///< Internal use. Synchronizes interrupts with waiting threads.
};

#endif

#endif /* SRC_COMMON_ZYNQIPMC_DRIVERS_PL_SPI_PL_SPI_H_ */
