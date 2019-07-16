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

#ifndef SRC_COMMON_ZYNQIPMC_DRIVERS_PS_QSPI_PS_QSPI_H_
#define SRC_COMMON_ZYNQIPMC_DRIVERS_PS_QSPI_PS_QSPI_H_

// Only include driver if PS QSPI is detected in the BSP.
#if XSDK_INDEXING || __has_include("xqspips.h")

#include "xqspips.h"
#include <FreeRTOS.h>
#include <queue.h>
#include <drivers/generics/spi.h>
#include <drivers/interrupt_based_driver.h>

/**
 * An interrupt-based driver for the PS QSPI.
 */
class PSQSPI : public SPIMaster, protected InterruptBasedDriver {
public:
	/**
	 * Constructs and initializes the PS QSPI driver.
	 * @param device_id Device ID, normally XPAR_PS7_QSPI_<>_DEVICE_ID. Check xparameters.h.
	 * @param intr_id Device interrupt ID from GIC, normally XPAR_PS7_QSPI_<>_INTR. Check xparameters.h.
	 * @throw except::hardware_error if low level driver fails to configure.
	 */
	PSQSPI(uint16_t device_id, uint16_t intr_id);
	virtual ~PSQSPI();

	// From base class SPIMaster:
	virtual bool transfer(size_t chip, const uint8_t *sendbuf, uint8_t *recvbuf, size_t bytes, TickType_t timeout = portMAX_DELAY);
	virtual bool transfer(const uint8_t *sendbuf, uint8_t *recvbuf, size_t bytes, TickType_t timeout = portMAX_DELAY);
	virtual bool isQuadSupported() const { return true; };
	virtual void select(size_t cs);
	virtual void deselect();

	PSQSPI(PSQSPI const &) = delete;               ///< Class is not assignable.
    void operator=(PSQSPI const &x) = delete;       ///< Class is not copyable.

protected:
    void _InterruptHandler();

    QueueHandle_t sync;			///< Synchronizes interrupts with waiting threads.
    volatile bool selected;		///< Indicates when the slave is selected.
    volatile bool started;		///< Indicates when the transfer has started and the instruction OP has been sent.
	XQspiPs qspi;				///< Low-level QSPI driver data.

	//! Possible QSPI operation modes.
	enum Mode {
		SINGLE, FAST, DUAL, QUAD
	} OpMode;
};

#endif

#endif /* SRC_COMMON_ZYNQIPMC_DRIVERS_PS_QSPI_PS_QSPI_H_ */
