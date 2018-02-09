/*
 * UARTConsole.h
 *
 *  Created on: Jan 17, 2018
 *      Author: jtikalsky
 */

#ifndef SRC_COMMON_UW_IPMC_SERVICES_CONSOLE_CONSOLESVC_H_
#define SRC_COMMON_UW_IPMC_SERVICES_CONSOLE_CONSOLESVC_H_

#include <libs/CommandParser.h>
#include <libs/LogTree.h>
#include <libs/ANSICode.h>
#include <libs/ThreadingPrimitives.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include <deque>
#include <string>
#include <vector>

/**
 * A generic console service.
 */
class ConsoleSvc {
public:
	ConsoleSvc(CommandParser &parser, const std::string &name, LogTree &logtree, bool echo);
	virtual ~ConsoleSvc();

	CommandParser &parser; ///< The command parser for this console.
	const std::string name; ///< The name for this ConsoleSvc's thread, logging, etc.
	LogTree &logtree; ///< A log sink for this service.
	LogTree &log_input; ///< A log sink for input.
	LogTree &log_output; ///< A log sink for output
	bool echo; ///< Enable or disable echo.

	virtual bool safe_write(std::string data, TickType_t timeout=portMAX_DELAY);

protected:
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
		std::string go_back(std::string line_to_cache, std::string::size_type cursor=0, bool *moved=NULL);
		std::string go_forward(std::string line_to_cache, std::string::size_type cursor=0, bool *moved=NULL);
		std::string go_latest(std::string line_to_cache, std::string::size_type cursor=0, bool *moved=NULL);
		bool is_current();
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
		 * @param prompt The prompt to be used by this input buffer.
		 * @param maxlen The maximum size limit to this buffer.
		 */
		InputBuffer(std::string prompt, size_type maxlen=0) : cursor(0), prompt(prompt), maxlen(maxlen), cols(80), rows(24) { };
		std::string buffer; ///< The buffer.
		size_type cursor; ///< The cursor position within the buffer.
		std::string prompt; ///< The prompt used for this input line.
		const size_type maxlen; ///< A hard limit on this so we don't expand stupidly due to random garbage input.
		bool overwrite_mode; ///< Overwrite mode.

		size_type cols; ///< Terminal columns.
		size_type rows; ///< Terminal rows.

		std::string clear();
		std::string reset(size_type cols=80, size_type rows=24);
		std::string update(std::string input);
		std::string set_buffer(std::string buffer, size_type cursor = std::string::npos);
		std::string refresh();
		size_type get_cursor_row();
		size_type get_rowcount();
		std::string resize(size_type cols, size_type rows);
		std::string query_size();
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
		std::string set_cursor(size_type cursor);
		///@}
	};

	SemaphoreHandle_t linebuf_mutex; ///< A mutex protecting the linebuf and direct output.
	InputBuffer linebuf; ///< The current line buffer.
	volatile bool shutdown; ///< If true, the service will shut down.  read() and write() must return immediately, for this to succeed.
	WaitList shutdown_complete; ///< Released immediately before thread self-deletion.  Object cleanup is your responsibility.

protected:
	/// A virtual method handling input, to be overridden by implementations.
	virtual ssize_t read(char *buf, size_t len, TickType_t timeout) { configASSERT(0); return 0; };

	/// A virtual method handling output, to be overridden by implementations.
	virtual ssize_t write(const char *buf, size_t len, TickType_t timeout) { configASSERT(0); return 0; };

public:
	virtual void _run_thread(); ///< \protected Internal
};

#endif /* SRC_COMMON_UW_IPMC_SERVICES_CONSOLE_CONSOLESVC_H_ */
