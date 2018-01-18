/*
 * UARTConsole.h
 *
 *  Created on: Jan 17, 2018
 *      Author: jtikalsky
 */

#ifndef SRC_COMMON_UW_IPMC_SERVICES_CONSOLE_UARTCONSOLESVC_H_
#define SRC_COMMON_UW_IPMC_SERVICES_CONSOLE_UARTCONSOLESVC_H_

#include <drivers/generics/UART.h>
#include <libs/CommandParser.h>
#include <libs/LogTree.h>

/**
 * A UART based console service.
 */
class UARTConsoleSvc {
public:
	UARTConsoleSvc(UART &uart, CommandParser &parser, const std::string &name, LogTree &logtree, bool echo, TickType_t read_data_timeout=10);
	virtual ~UARTConsoleSvc();

	UART &uart; ///< The UART this console is driven by.
	CommandParser &parser; ///< The command parser for this console.
	const std::string name; ///< The name for this UARTConsoleSvc's thread, logging, etc.
	LogTree &logtree; ///< A log sink for this service.
	LogTree &log_input; ///< A log sink for input.
	LogTree &log_output; ///< A log sink for output
	bool echo; ///< Enable or disable echo.
	TickType_t read_data_timeout; ///< The read data timeout for the UART, which influences responsiveness.

	void _run_thread(); ///< \protected
};

#endif /* SRC_COMMON_UW_IPMC_SERVICES_CONSOLE_UARTCONSOLESVC_H_ */
