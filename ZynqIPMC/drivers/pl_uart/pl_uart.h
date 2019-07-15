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

#ifndef SRC_COMMON_ZYNQIPMC_DRIVERS_PL_UART_PL_UART_H_
#define SRC_COMMON_ZYNQIPMC_DRIVERS_PL_UART_PL_UART_H_

// Only include driver if PL UART in the BSP.
#if XSDK_INDEXING || __has_include("xuartlite.h")

#include "xuartlite.h"
#include <stdio.h>
#include <FreeRTOS.h>
#include <semphr.h>
#include <drivers/generics/uart.h>
#include <drivers/interrupt_based_driver.h>
#include <libs/RingBuffer.h>

/**
 * A FreeRTOS interrupt based driver for Xilinx UartLite IP core.
 */
class PLUART final : public UART, protected InterruptBasedDriver {
public:
	/**
	 * Create a PL based UART interface.
	 * @param device_id Device ID, normally AXI_UARTLITE_<>_DEVICE_ID. Check xparameters.h.
	 * @param intr_id Device interrupt ID from GIC, normally XPAR_FABRIC_AXI_UARTLITE_<>_INTERRUPT_INTR. Check xparameters.h.
	 * @param ibufsize Input ring buffer size.
	 * @param obufsize Output ring buffer size.
	 * @throw except::hardware_error if low level driver fails to configure.
	 */
	PLUART(uint16_t device_id, uint16_t intr_id, size_t ibufsize = 4096, size_t obufsize = 4096);
	virtual ~PLUART();

	// From base class UART:
	virtual size_t read(uint8_t *buf, size_t len, TickType_t timeout = portMAX_DELAY, TickType_t data_timeout = portMAX_DELAY);
	virtual size_t write(const uint8_t *buf, size_t len, TickType_t timeout = portMAX_DELAY);
	virtual bool clear();

private:
	virtual void _InterruptHandler();	///< Internal interrupt handler for UART IRQ.
	void recv();						///< Internal receive function.
	void send();						///< Internal send function.

	mutable XUartLite uartlite;			///< Internal PL UART driver data.
	RingBuffer<uint8_t> inbuf;  		///< The input buffer.
	RingBuffer<uint8_t> outbuf; 		///< The output buffer.
	WaitList readwait;     				///< A waitlist for blocking read operations.
	WaitList writewait;    				///< A waitlist for blocking write operations.
};

#endif

#endif /* SRC_COMMON_ZYNQIPMC_DRIVERS_PL_UART_PL_UART_H_ */
