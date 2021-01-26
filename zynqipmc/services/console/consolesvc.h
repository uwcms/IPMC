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

#ifndef SRC_COMMON_ZYNQIPMC_SERVICES_CONSOLE_CONSOLESVC_H_
#define SRC_COMMON_ZYNQIPMC_SERVICES_CONSOLE_CONSOLESVC_H_

#include <deque>
#include <string>
#include <vector>
#include <stdexcept>
#include <FreeRTOS.h>
#include <semphr.h>
#include <event_groups.h>
#include <libs/ansi_code/ansi_code.h>
#include <libs/logtree/logtree.h>
#include <libs/threading.h>
#include <services/console/command_parser.h>

/**
 * A generic console service.
 */
class ConsoleSvc {
public:
	virtual ~ConsoleSvc();

	ConsoleSvc(ConsoleSvc const &) = delete;      ///< Class is not assignable.
	void operator=(ConsoleSvc const &x) = delete; ///< Class is not copyable.

	//! Start the console service.  Call exactly once.
	virtual void start();

	/**
	 * Write to the console without disrupting the prompt.
	 *
	 * @note A timeout of 0 will be a little unlikely to succeed.  Try 1.
	 *
	 * @param data The data to write out.
	 * @param timeout How long to wait.
	 * @return true if success, false otherwise.
	 */
	virtual bool write(const std::string data, TickType_t timeout = portMAX_DELAY);

	//! Returns the associated log tree.
	inline LogTree& getLogTree() { return this->logtree; };

protected:
	/**
	 * A weak pointer to self, allowing us to be passed to Command::execute()
	 * in such a way that deferred callbacks can still call our .write().
	 */
	std::weak_ptr<ConsoleSvc> weakself;

	/**
	 * Instantiate a Console Service.
	 *
	 * This class must have a std::weak_ptr of itself, and therefore
	 * can only be created through a factory method.  Example create() above.
	 *
	 * Since the parameters of this are specific, this factory cannot be virtual.
	 *
	 * @param parser The command parser to use.
	 * @param name The name of the service for the process and such things.
	 * @param logtree The log tree root for this service.
	 * @param echo If true, enable echo and interactive management.
	 * @param read_data_timeout The timeout for reads when data is available.
	 */
	ConsoleSvc(CommandParser &parser, const std::string &name, LogTree &logtree, bool echo, TickType_t read_data_timeout = pdMS_TO_TICKS(100));

	/**
	 * A command history manager, complete with history, current-line cache, and managed iterator.
	 */
	class CommandHistory {
	public:
		typedef std::deque<std::string>::size_type size_type; ///< Inherit the size_type of our underlying container.

		/**
		 * Construct a command history.
		 * @param length The history length.
		 */
		CommandHistory(size_type length) : length(length) {
			this->history_position = this->history.begin();
		};

		/**
		 * Step back in time.
		 *
		 * @param line_to_cache The current input line.
		 * @param cursor The cursor position, used for prefix-search.
		 * @param moved [out] true if the history position changed, else false.
		 * @return The new input line.
		 */
		std::string goBack(std::string line_to_cache, std::string::size_type cursor=0, bool *moved = nullptr);

		/**
		 * Step forward in time.
		 *
		 * @param line_to_cache The current input line.
		 * @param cursor The cursor position, used for prefix-search.
		 * @param moved [out] true if the history position changed, else false.
		 * @return The new input line.
		 */
		std::string goForward(std::string line_to_cache, std::string::size_type cursor=0, bool *moved = nullptr);

		/**
		 * Step to the present.
		 *
		 * @param line_to_cache The current input line.
		 * @param cursor The cursor position, used for prefix-search.
		 * @param moved [out] true if the history position changed, else false.
		 * @return The new input line.
		 */
		std::string goLatest(std::string line_to_cache, std::string::size_type cursor=0, bool *moved = nullptr);

		/**
		 * Identify whether the current history position is the present.
		 * @return true if the history position is the present, else false.
		 */
		bool isCurrent();

		/**
		 * Save an entered input line.
		 * @param line The input line to record.
		 */
		void recordEntry(const std::string& line);

	protected:
		size_type length;									///< The length of the command history
		std::deque<std::string> history;					///< The history itself.
		std::deque<std::string>::iterator history_position;	///< The current back scroll position.
		std::string cached_line;							///< The cached edit line from before history was entered.
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
		InputBuffer(std::string prompt, size_type maxlen=0) : cursor(0), prompt(prompt), maxlen(maxlen), overwrite_mode(false), cols(80), rows(24) { };

		//! Clear the input buffer.
		std::string clear();

		/**
		 * Reset the input buffer.
		 * @return Echo data.
		 */
		std::string reset(size_type cols=80, size_type rows=24);

		/**
		 * Insert characters into the buffer.
		 * @param input The data to insert.
		 * @return Echo data.
		 */
		std::string update(std::string input);

		/**
		 * Update the buffer and refresh the screen.
		 * @param buffer New buffer.
		 * @param cursor New cursor.
		 * @return Echo data.
		 */
		std::string setBuffer(std::string buffer, size_type cursor = std::string::npos);

		//! Return echo data to erase and reprint the line.
		std::string refresh();

		//! Return the current row position of the cursor, relative to the start, one-indexed.
		size_type getCursorRow();

		//! Return the number of rows occupied by this input buffer.
		size_type getRowCount();

		/**
		 * Resize the buffer's perception of the console size.
		 * @param cols Columns.
		 * @param rows Rows.
		 * @return Echo data.
		 */
		std::string resize(size_type cols, size_type rows);

		/**
		 * Query the terminal size.
		 * @return Echo data.
		 */
		std::string querySize();

		/**
		 * Cursor Movement
		 * @return echo data
		 */
		///@{
		std::string home();
		std::string end();
		std::string left();
		std::string right();
		std::string leftword();
		std::string rightword();
		std::string backspace();
		std::string delkey();
		std::string deltoend();
		std::string set_cursor(size_type cursor);
		///@}

		friend class ConsoleSvc;

	protected:
		std::string buffer;		///< The buffer.
		size_type cursor;		///< The cursor position within the buffer.
		std::string prompt;		///< The prompt used for this input line.
		const size_type maxlen; ///< A hard limit on this so we don't expand stupidly due to random garbage input.
		bool overwrite_mode;	///< Overwrite mode.

		size_type cols;			///< Terminal columns.
		size_type rows;			///< Terminal rows.

		// Characters that delimit what is considered a word
		static const std::string word_delimitor;

	};

	/**
	 * A virtual method allowing "shutdown complete" notification/handling.
	 *
	 * This is called immediately before object deletion (if relevant) and
	 * thread death.
	 */
	virtual void shutdownComplete() { };

	//! A pure virtual method handling input, to be overridden by implementations.
	virtual ssize_t rawRead(char *buf, size_t len, TickType_t timeout, TickType_t read_data_timeout) = 0;

	//! A pure virtual method handling output, to be overridden by implementations.
	virtual ssize_t rawWrite(const char *buf, size_t len, TickType_t timeout) = 0;

	CommandParser &parser;			///< The command parser for this console.
	const std::string name;			///< The name for this ConsoleSvc's thread, logging, etc.
	LogTree &logtree;				///< A log sink for this service.
	LogTree &log_input;				///< A log sink for input.
	bool echo;						///< Enable or disable echo.
	TickType_t read_data_timeout;	///< The read data timeout for the UART, which influences responsiveness.
	TaskHandle_t task;				///< A random handle to our task, mainly just for double-start checks.

	std::string::size_type safe_write_line_cursor; ///< The safe_write output cursor.

    SemaphoreHandle_t linebuf_mutex;	///< A mutex protecting the linebuf and direct output.
	InputBuffer linebuf;				///< The current line buffer.

	//! If true, the service will shut down.  read() and write() must return immediately, for this to succeed.
	volatile bool shutdown;

	virtual void runThread(); ///< \protected Internal
};

/**
 * Format a log message for console output.
 *
 * @param message The log message
 * @param level The loglevel of the message
 * @return The message formatted for console output
 */
std::string consoleSvcLogFormat(const std::string &message, enum LogTree::LogLevel level);

#endif /* SRC_COMMON_ZYNQIPMC_SERVICES_CONSOLE_CONSOLESVC_H_ */
