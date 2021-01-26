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

#ifndef SRC_COMMON_ZYNQ_SERVICES_CONSOLE_UARTCONSOLESVC_H_
#define SRC_COMMON_ZYNQ_SERVICES_CONSOLE_UARTCONSOLESVC_H_

#include <drivers/generics/uart.h>
#include <services/console/consolesvc.h>

/**
 * A UART based console service.
 */
class UARTConsoleSvc final : public ConsoleSvc {
public:
	/**
	 * Factory function.  All parameters match the constructor.
	 *
	 * @note Since the parameters are specific, this function cannot be virtual.
	 */
	static std::shared_ptr<UARTConsoleSvc> create(UART &uart, CommandParser &parser, const std::string &name, LogTree &logtree, bool echo, TickType_t read_data_timeout = pdMS_TO_TICKS(100)) {
		std::shared_ptr<UARTConsoleSvc> ret(new UARTConsoleSvc(uart, parser, name, logtree, echo, read_data_timeout));
		ret->weakself = ret;
		return ret;
	}

	virtual ~UARTConsoleSvc() { };

private:
	/**
	 * Instantiate a UART console service.
	 *
	 * @param uart The UART
	 * @param parser The command parser to use.
	 * @param name The name of the service for the process and such things.
	 * @param logtree The log tree root for this service.
	 * @param echo If true, enable echo and interactive management.
	 * @param read_data_timeout The timeout for reads when data is available.
	 */
	UARTConsoleSvc(UART &uart, CommandParser &parser, const std::string &name, LogTree &logtree, bool echo, TickType_t read_data_timeout=pdMS_TO_TICKS(100))
		: ConsoleSvc(parser, name, logtree, echo, read_data_timeout), uart(uart) { this->start(); };

	UART &uart; ///< The UART this console is driven by.

	//! Pass read through to the UART.
	virtual ssize_t rawRead(char *buf, size_t len, TickType_t timeout, TickType_t read_data_timeout) { return this->uart.read((uint8_t*)buf, len, timeout, read_data_timeout); };

	//! Pass write through to the UART.
	virtual ssize_t rawWrite(const char *buf, size_t len, TickType_t timeout) { return this->uart.write((const uint8_t*)buf, len, timeout); };
};

#endif /* SRC_COMMON_ZYNQ_SERVICES_CONSOLE_UARTCONSOLESVC_H_ */
