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

#ifndef SRC_COMMON_ZYNQIPMC_DRIVERS_PSSPI_PSSPI_H_
#define SRC_COMMON_ZYNQIPMC_DRIVERS_PSSPI_PSSPI_H_

// Only include driver if PS SPI is detected in the BSP.
#if XSDK_INDEXING || __has_include("xspips.h")

#include "xspips.h"
#include <FreeRTOS.h>
#include <queue.h>
#include <drivers/generics/spi.h>
#include <drivers/interrupt_based_driver.h>

/**
 * An interrupt-based driver for the PS SPI.
 */
class PSSPI final: public SPIMaster, protected InterruptBasedDriver {
public:
	/**
	 * Constructs and initializes the PS SPI driver.
	 * @param device_id Device ID, normally XPAR_PS7_SPI_<>_DEVICE_ID. Check xparameters.h.
	 * @param intr_id Device interrupt ID from GIC, normally XPAR_PS7_SPI_<>_INTR. Check xparameters.h.
	 * @throw except::hardware_error if low level driver fails to configure.
	 */
	PSSPI(uint16_t device_id, uint16_t intr_id);
	virtual ~PSSPI();

	// From base class SPIMaster:
	virtual bool transfer(size_t chip, const uint8_t *sendbuf, uint8_t *recvbuf, size_t bytes, TickType_t timeout = portMAX_DELAY);
	virtual bool transfer(const uint8_t *sendbuf, uint8_t *recvbuf, size_t bytes, TickType_t timeout = portMAX_DELAY);
	virtual bool isQuadSupported() const { return false; };
	virtual void select(size_t cs);
	virtual void deselect();

	PSSPI(PSSPI const &) = delete;               ///< Class is not assignable.
    void operator=(PSSPI const &x) = delete;       ///< Class is not copyable.

private:
    //! Used to interface with the Xilinx low-level driver from interrupts.
    static void _InterruptPassthrough(PSSPI *ps_spi, uint32_t event_status, uint32_t byte_count);

    //! Handles a low-level interrupt.
    void _HandleInterrupt(uint32_t event_status, uint32_t byte_count); ///< \protected Internal.

    XSpiPs xspips;					///< Low-level driver data.
    uint32_t error_not_done;		///< Error containing accumulated errors of non completed transfers. TODO: allow read.
    uint32_t error_byte_count;		///< Error containing accumulated errors of byte count mismatches. TODO: allow read.
	QueueHandle_t irq_sync_q;       ///< IRQ-task syncronization queue.
	volatile bool transfer_running; ///< Indicates whether an interrupt-driven transfer is in progress.

	void _InterruptHandler();

	//! IRQ transfer status.
	struct trans_st_t
	{
		uint32_t byte_count;    ///< Transfer byte count provided to IRQ handler by Xilinx ps_spi drive.
		uint32_t event_status;  ///< Transfer status provided to IRQ handler by Xilinx ps_spi driver.
	};
};

#endif

#endif /* SRC_COMMON_ZYNQIPMC_DRIVERS_PSSPI_PSSPI_H_ */
