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
#include "FreeRTOS.h"
#include "semphr.h"
#include <deque>
#include <string>

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
	std::deque<std::string>::size_type command_history_length; ///< The length of the command history, when echo mode is enabled.

	bool safe_write(std::string data, TickType_t timeout=portMAX_DELAY);

protected:
	const std::string prompt = "> ";
	SemaphoreHandle_t linebuf_mutex; ///< A mutex protecting the linebuf and direct output.
	std::string linebuf;
	std::deque<std::string> command_history; ///< The command history.

public:
	void _run_thread(); ///< \protected Internal
};

/**
 * A class representing and parsing ANSI Control Codes.
 */
class ANSICode {
public:
	std::string buffer; ///< The buffer for parsing.

	/// An enum representing the state of the current buffer.
	enum ParseState {
		PARSE_EMPTY, PARSE_INCOMPLETE, PARSE_COMPLETE, PARSE_INVALID,
	};

	enum ParseState parse();
	/// \overload
	enum ParseState parse(std::string append) { this->buffer.append(append); return this->parse(); };
	/// \overload
	enum ParseState parse(char append) { this->buffer += append; return this->parse(); };

	std::string code; ///< The actual code of the last parsed code.
	std::string name; ///< The name of the last parsed code.
	std::vector<int> parameters; ///< The parameters of the last parsed code.

	static const std::map<std::string, std::string> codenames; ///< A mapping of ANSICode().code to ANSICode().name

	static const std::string ANSI_ERASE_TO_END_OF_LINE;
	static const std::string ANSI_ERASE_TO_START_OF_LINE;
	static const std::string ANSI_ERASE_LINE;
};

#endif /* SRC_COMMON_UW_IPMC_SERVICES_CONSOLE_UARTCONSOLESVC_H_ */
