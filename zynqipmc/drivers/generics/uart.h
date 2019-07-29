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

#ifndef SRC_COMMON_ZYNQIPMC_DRIVERS_GENERICS_UART_H_
#define SRC_COMMON_ZYNQIPMC_DRIVERS_GENERICS_UART_H_

#include <FreeRTOS.h>

/**
 * A generic UART abstract interface driver.
 */
class UART {
public:
	UART() { };
	virtual ~UART() { };

	/**
	 * Read a fixed number of bytes from UART or until a certain timeout is reach.
	 * @param buf Buffer to read into.
	 * @param len Maximum number of bytes to read.
	 * @param timeout Timeout for this read, in standard FreeRTOS format.
	 * @param data_timeout A second timeout used to shorten `timeout` if data is present.
	 * @return Number of bytes read stored in the buffer.
	 * @note This function needs to be interrupt and critical safe if timeout = 0.
	 */
	virtual size_t read(uint8_t *buf, size_t len, TickType_t timeout = portMAX_DELAY, TickType_t data_timeout = portMAX_DELAY) = 0;

	/**
	 * Write a fixed number of bytes to the UART.
	 * @param buf Buffer to write from.
	 * @param len Maximum number of bytes to write.
	 * @param timeout Timeout for this write, in standard FreeRTOS format.
	 * @return Number of bytes successfully written.
	 */
	virtual size_t write(const uint8_t *buf, size_t len, TickType_t timeout = portMAX_DELAY) = 0;

	//! Clear the input queue. Returns false if it fails.
	virtual bool clear() = 0;
};

#endif /* SRC_COMMON_ZYNQIPMC_DRIVERS_GENERICS_UART_H_ */
