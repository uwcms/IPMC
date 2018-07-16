/*
 * UARTConsole.h
 *
 *  Created on: Jan 17, 2018
 *      Author: jtikalsky
 */

#ifndef SRC_COMMON_UW_IPMC_SERVICES_CONSOLE_CONSOLESVC_H_
#define SRC_COMMON_UW_IPMC_SERVICES_CONSOLE_CONSOLESVC_H_

#include <services/console/CommandParser.h>
#include <libs/LogTree.h>
#include <libs/ANSICode.h>
#include <libs/ThreadingPrimitives.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include "event_groups.h"
#include <deque>
#include <string>
#include <vector>

/**
 * A generic console service.
 */
class ConsoleSvc {
public:
	/**
	 * Factory function.  All parameters match the constructor.
	 *
	 * \note Since the parameters are specific, this function cannot be virtual.
	 */
	static std::shared_ptr<ConsoleSvc> create(CommandParser &parser, const std::string &name, LogTree &logtree, bool echo, TickType_t read_data_timeout=10) {
		configASSERT(0); // ConsoleSvc is intended as an abstract class only.
		std::shared_ptr<ConsoleSvc> ret(new ConsoleSvc(parser, name, logtree, echo, read_data_timeout));
		ret->weakself = ret;
		return ret;
	}
protected:
	/**
	 * A weak pointer to self, allowing us to be passed to Command::execute()
	 * in such a way that deferred callbacks can still call our .write().
	 */
	std::weak_ptr<ConsoleSvc> weakself;

	/* Protected.  This class must have a std::weak_ptr of itself, and therefore
	 * can only be created through a factory method.  Example create() above.
	 *
	 * Since the parameters of this are specific, this factory cannot be virtual.
	 */
	ConsoleSvc(CommandParser &parser, const std::string &name, LogTree &logtree, bool echo, TickType_t read_data_timeout=10);
public:
	virtual ~ConsoleSvc();
	virtual void start();

	CommandParser &parser; ///< The command parser for this console.
	const std::string name; ///< The name for this ConsoleSvc's thread, logging, etc.
	LogTree &logtree; ///< A log sink for this service.
	LogTree &log_input; ///< A log sink for input.
	bool echo; ///< Enable or disable echo.
	TickType_t read_data_timeout; ///< The read data timeout for the UART, which influences responsiveness.
	TaskHandle_t task; ///< A random handle to our task, mainly just for double-start checks.

	virtual bool write(const std::string data, TickType_t timeout=portMAX_DELAY);

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
		InputBuffer(std::string prompt, size_type maxlen=0) : cursor(0), prompt(prompt), maxlen(maxlen), cols(80), rows(24), overwrite_mode(false) { };
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

	/**
	 * A virtual method allowing "shutdown complete" notification/handling.
	 *
	 * This is called immediately before object deletion (if relevant) and
	 * thread death.
	 */
	virtual void shutdown_complete() { };

	/// A virtual method handling input, to be overridden by implementations.
	virtual ssize_t raw_read(char *buf, size_t len, TickType_t timeout, TickType_t read_data_timeout) { configASSERT(0); return 0; };

	/// A virtual method handling output, to be overridden by implementations.
	virtual ssize_t raw_write(const char *buf, size_t len, TickType_t timeout) { configASSERT(0); return 0; };

    ConsoleSvc(ConsoleSvc const &) = delete;      ///< Class is not assignable.
    void operator=(ConsoleSvc const &x) = delete; ///< Class is not copyable.

public:
	virtual void _run_thread(); ///< \protected Internal
};

std::string ConsoleSvc_log_format(const std::string &message, enum LogTree::LogLevel level);

#endif /* SRC_COMMON_UW_IPMC_SERVICES_CONSOLE_CONSOLESVC_H_ */
