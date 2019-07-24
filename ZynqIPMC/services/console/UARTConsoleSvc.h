/*
 * UARTConsole.h
 *
 *  Created on: Jan 17, 2018
 *      Author: jtikalsky
 */

#ifndef SRC_COMMON_UW_IPMC_SERVICES_CONSOLE_UARTCONSOLESVC_H_
#define SRC_COMMON_UW_IPMC_SERVICES_CONSOLE_UARTCONSOLESVC_H_

#include <drivers/generics/uart.h>
#include <services/console/consolesvc.h>

/**
 * A UART based console service.
 */
class UARTConsoleSvc : public ConsoleSvc {
public:
	/**
	 * Factory function.  All parameters match the constructor.
	 *
	 * \note Since the parameters are specific, this function cannot be virtual.
	 */
	static std::shared_ptr<UARTConsoleSvc> create(UART &uart, CommandParser &parser, const std::string &name, LogTree &logtree, bool echo, TickType_t read_data_timeout=10) {
		std::shared_ptr<UARTConsoleSvc> ret(new UARTConsoleSvc(uart, parser, name, logtree, echo, read_data_timeout));
		ret->weakself = ret;
		return ret;
	}

protected:
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
	UARTConsoleSvc(UART &uart, CommandParser &parser, const std::string &name, LogTree &logtree, bool echo, TickType_t read_data_timeout=10)
		: ConsoleSvc(parser, name, logtree, echo, read_data_timeout), uart(uart) { this->start(); };

public:
	virtual ~UARTConsoleSvc() { };

protected:
	UART &uart; ///< The UART this console is driven by.

	/// Pass read through to the UART.
	virtual ssize_t raw_read(char *buf, size_t len, TickType_t timeout, TickType_t read_data_timeout) { return this->uart.read((u8*)buf, len, timeout, read_data_timeout); };

	/// Pass write through to the UART.
	virtual ssize_t raw_write(const char *buf, size_t len, TickType_t timeout) { return this->uart.write((const u8*)buf, len, timeout); };
};

#endif /* SRC_COMMON_UW_IPMC_SERVICES_CONSOLE_UARTCONSOLESVC_H_ */
