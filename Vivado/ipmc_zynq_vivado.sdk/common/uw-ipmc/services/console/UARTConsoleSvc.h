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
#include <libs/ANSICode.h>
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

	bool safe_write(std::string data, TickType_t timeout=portMAX_DELAY);

protected:
	const std::string prompt = "> "; ///< The command prompt.
	std::string::size_type safe_write_line_cursor; ///< The safe_write output cursor.

	/**
	 * A command history manager, complete with history, current-line cache, and managed iterator.
	 */
	class CommandHistory {
	public:
		typedef std::deque<std::string>::size_type size_type; ///< Inherit the size_type of our underlying container.
		size_type length; ///< The length of the command history
		/**
		 * Construct a command history.
		 * @param length The history length.
		 */
		CommandHistory(size_type length) : length(length) {
			this->history_position = this->history.begin();
		};
		std::string go_back(std::string line_to_cache);
		std::string go_forward(std::string line_to_cache);
		void record_entry(std::string line);
	protected:
		std::deque<std::string> history; ///< The history itself.
		std::deque<std::string>::iterator history_position; ///< The current back scroll position.
		std::string cached_line; ///< The cached edit line from before history was entered.
	};

	/**
	 * An input line buffer.
	 */
	class InputBuffer {
	public:
		typedef std::string::size_type size_type; ///< Inherit size_type from the underlying container.

		/**
		 * Instantiate an input buffer
		 * @param prompt The prompt to be displayed when refreshing the display.
		 * @param maxlen The maximum size limit to this buffer.
		 */
		InputBuffer(std::string prompt = "", size_type maxlen=0) : prompt(prompt), maxlen(maxlen) { };
		std::string buffer; ///< The buffer.
		size_type cursor; ///< The cursor position within the buffer.
		std::string prompt; ///< The prompt used for this input line.
		const size_type maxlen; ///< A hard limit on this so we don't get into stack overflow territory with our various operations.
		bool overwrite_mode; ///< Overwrite mode.

		std::string clear();
		std::string update(std::string input);
		std::string refresh();
		std::string refresh_cursor();
		/**
		 * Cursor Movement
		 * @return echo data
		 */
		///@{
		std::string home();
		std::string end();
		std::string left();
		std::string right();
		std::string backspace();
		std::string delkey();
		///@}
	};

	SemaphoreHandle_t linebuf_mutex; ///< A mutex protecting the linebuf and direct output.
	InputBuffer linebuf; ///< The current line buffer.

public:
	void _run_thread(); ///< \protected Internal
};

#endif /* SRC_COMMON_UW_IPMC_SERVICES_CONSOLE_UARTCONSOLESVC_H_ */
