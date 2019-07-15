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

#ifndef SRC_COMMON_ZYNQIPMC_DRIVERS_PS_UART_PL_UART_H_
#define SRC_COMMON_ZYNQIPMC_DRIVERS_PS_UART_PL_UART_H_

// Only include driver if PS UART in the BSP.
#if XSDK_INDEXING || __has_include("xuartps.h")

#include "xuartps.h"
#include <FreeRTOS.h>
#include <semphr.h>
#include <drivers/generics/uart.h>
#include <drivers/interrupt_based_driver.h>
#include <libs/RingBuffer.h>

/**
 * An interrupt based driver wrapper for the Zynq7000 PS UART devices.
 *
 * This driver will maintain an input and output buffer of user-specified
 * byte length.
 */
class PSUART final : public UART, protected InterruptBasedDriver {
public:
	/**
	 * Instantiate and initialize a PSUART driver.
	 * @param device_id Device ID, normally XPAR_PS7_UART_<>_DEVICE_ID. Check xparameters.h.
	 * @param intr_id Device interrupt ID from GIC, normally XPAR_PS7_UART_<>_INTR. Check xparameters.h.
	 * @param ibufsize Input ring buffer size.
	 * @param obufsize Output ring buffer size.
	 * @throw except::hardware_error if low level driver fails to configure.
	 */
	PSUART(uint16_t device_id, uint16_t intr_id, size_t ibufsize = 4096, size_t obufsize = 4096);
	virtual ~PSUART();

	// From base class UART:
	virtual size_t read(uint8_t *buf, size_t len, TickType_t timeout, TickType_t data_timeout=portMAX_DELAY);
	virtual size_t write(const uint8_t *buf, size_t len, TickType_t timeout);
	virtual bool clear();

	//! Returns the total number of errors.
	inline size_t getErrors() { return this->error_count; };

	PSUART(PSUART const &) = delete;               ///< Class is not assignable.
    void operator=(PSUART const &x) = delete;       ///< Class is not copyable.

private:
    virtual void _InterruptHandler();	///< Internal interrupt handler for UART IRQ.
	void recv();						///< Internally called to receive to the ring buffer.
	void send();						///< Internally called to send from the ring buffer.

	volatile size_t error_count;       ///< Error counter containing accumulated errors from recent operations.
	XUartPs uartps;      			///< XUartPs handle of the driven device.
	RingBuffer<uint8_t> inbuf;  			///< Input ring buffer.
	RingBuffer<uint8_t> outbuf; 			///< Output ring buffer.
	WaitList readwait;     			///< A waitlist for blocking read operations.
	WaitList writewait;    			///< A waitlist for blocking write operations.
};

#endif

#endif /* SRC_COMMON_ZYNQIPMC_DRIVERS_PS_UART_PL_UART_H_ */
