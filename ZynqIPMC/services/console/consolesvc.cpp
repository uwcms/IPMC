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

#include "consolesvc.h"
#include <IPMC.h> // TODO: Remove later on
#include <drivers/tracebuffer/tracebuffer.h>
#include <libs/printf.h>

template <typename T> static inline T divCeil(T val, T divisor) {
	return (val / divisor) + (val % divisor ? 1 : 0);
}

/**
 * 	CommandParser &parser;			///< The command parser for this console.
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
 * @param parser
 * @param name
 * @param logtree
 * @param echo
 * @param read_data_timeout
 */

ConsoleSvc::ConsoleSvc(CommandParser &parser, const std::string &name, LogTree &logtree, bool echo, TickType_t read_data_timeout)
	: parser(parser), name(name), logtree(logtree), log_input(logtree["input"]),
	  echo(echo), read_data_timeout(read_data_timeout), task(nullptr), safe_write_line_cursor(0),
	  linebuf("> ", 2048), shutdown(false) {
	this->linebuf_mutex = xSemaphoreCreateMutex();
	configASSERT(this->linebuf_mutex);

#if 0 // Debugging only.
	CommandParser::handler_t colormecmd = [](std::function<void(std::string)> print, const CommandParser::CommandParameters &parameters) -> void {
		int fg = 9;
		int bg = 9;
		bool bold = false;
		bool underline = false;
		bool inverse = false;
		parameters.parse_parameters(1, false, &fg);
		parameters.parse_parameters(2, false, &bg);
		parameters.parse_parameters(3, false, &bold);
		parameters.parse_parameters(3, false, &underline);
		parameters.parse_parameters(3, false, &inverse);
		print(ANSICode::color(static_cast<enum ANSICode::TermColor>(fg),static_cast<enum ANSICode::TermColor>(bg),bold,underline,inverse) + "Colorful, isn't it?" + ANSICode::color() + "\n");
	};
	parser.register_command("colorme", colormecmd, "colorme fgnum bgnum bold underline inverse");

	CommandParser::handler_t ansicmd = [](std::function<void(std::string)> print, const CommandParser::CommandParameters &parameters) -> void {
			std::string arg;
			if (parameters.parse_parameters(0, true, &arg))
				print(std::string("I just heard a ") + arg + "\n");
	};
	for (auto it = ANSICode::codenames.begin(), eit = ANSICode::codenames.end(); it != eit; ++it)
		parser.register_command("ANSI_" + it->second, ansicmd, "");
#endif
}

ConsoleSvc::~ConsoleSvc() {
	// TODO: Honestly I think this is bad if different services use the same logtree
	// I would change to a shared pointer or simply not delete it a all.. which is what I am opting to do right now.
	//delete &this->logtree;
	vSemaphoreDelete(this->linebuf_mutex);
}

void ConsoleSvc::start() {
	configASSERT(!this->task);
	this->task = runTask(name, TASK_PRIORITY_INTERACTIVE, [this]() -> void { this->runThread(); });
	configASSERT(this->task);
}

bool ConsoleSvc::write(const std::string data, TickType_t timeout) {
	if (this->shutdown)
		return true; // Discard.

	AbsoluteTimeout abstimeout(timeout);
	MutexGuard<false> lock(this->linebuf_mutex, false);
	try {
		lock.acquire(abstimeout.getTimeout());
	}
	catch (except::timeout_error &e) {
		return false;
	}

	std::string writebuf = data; // Make a copy, since we'll consume it later.

	/* 1. Move to proper position to resume writing.
	 * 2. Write.
	 * 3. Move down if needed.
	 * 4. Refresh prompt.
	 */
	InputBuffer::size_type input_cursor = this->linebuf.cursor;
	std::string out = this->linebuf.set_cursor(0); // Move to top line of prompt;
	if (this->safe_write_line_cursor) {
		// Restore cursor.
		out += ANSICode::ANSI_CURSOR_UP_ONE;
		out += stdsprintf(ANSICode::ANSI_CURSOR_ABSOLUTE_HORIZONTAL_POSITION_INTFMT.c_str(), (this->safe_write_line_cursor % this->linebuf.cols) + 1 /* 1-indexed */);
	}
	else {
		// Move to the start of the prompt, and clear below us.
		out += "\r";
		out += ANSICode::ANSI_ERASE_DOWN;
	}
	std::string::size_type pos;
	while ( ( pos = writebuf.find_first_of("\n") ) != std::string::npos ) {
		out += writebuf.substr(0, pos);
		writebuf.erase(0, pos+1);
		if (writebuf.size())
			out += std::string("\r\n") /* back down to prompt */;

		// Either way, we just put out a line, so our cursor is 0 now.
		this->safe_write_line_cursor = 0;
	}

	// Ok, done with all LINES.  If we have a partial, we need to deal with it now.
	out += writebuf; // All remainder.
	this->safe_write_line_cursor = writebuf.size();

	// And now return to the prompt line.
	out += "\r\n";
	out += this->linebuf.refresh();
	out += this->linebuf.set_cursor(input_cursor);
	this->raw_write(out.data(), out.size(), abstimeout.getTimeout());
	return true;
}

void ConsoleSvc::runThread() {
	this->logtree.log("Starting Console Service \"" + this->name + "\"", LogTree::LOG_INFO);
	const std::string ctrlc_erased_facility = this->logtree.getPath() + ".ctrlc_erased";
#ifdef ANSICODE_TIMEOUT
	const std::string timed_out_ansi_facility = this->logtree.path + ".timed_out_ansi";
#endif

	// We will hold this semaphore as the rule, not the exception, releasing it only for other transactions.
	MutexGuard<false> lock(this->linebuf_mutex, true);

	if (this->echo)
		this->raw_write(this->linebuf.prompt.data(), this->linebuf.prompt.size(), portMAX_DELAY);

	if (this->echo)
		this->raw_write(ANSICode::ANSI_CURSOR_QUERY_POSITION.data(), ANSICode::ANSI_CURSOR_QUERY_POSITION.size(), portMAX_DELAY);

	static const char reset_control_keys[] = {
			ANSICode::renderASCIIControlkey('R'),
			ANSICode::renderASCIIControlkey('C'),
			ANSICode::renderASCIIControlkey('D'),
			'\0', // String Terminator
	};

	ANSICode ansi_code;
	CommandHistory history(50);
	char prevchar = '\0';
	bool history_browse = false;

	while (true) {
		char readbuf[128];
		if (this->shutdown)
			break;

		lock.release();
		ssize_t bytes_read = this->raw_read(readbuf, 128, portMAX_DELAY, this->read_data_timeout);
		lock.acquire();

		if (this->shutdown)
			break;

		if (bytes_read < 0) {
			this->logtree.log(stdsprintf("raw_read() returned negative value %d", bytes_read), LogTree::LOG_DIAGNOSTIC);
			continue;
		}

		std::string rawbuffer(readbuf, bytes_read);
		std::string echobuf;

		std::string::size_type rst_before = rawbuffer.find_last_of(reset_control_keys);
		if (rst_before != std::string::npos) {
			// Clear all buffer found before any Ctrl-C (or D, or R), and replace it with an empty line to retrigger the prompt.
			std::string tracebuf = this->linebuf.buffer + rawbuffer.substr(0, rst_before+1);
			TRACE.log(ctrlc_erased_facility.c_str(), ctrlc_erased_facility.size(), LogTree::LOG_TRACE, tracebuf.data(), tracebuf.size(), true);
			history.goLatest("",0);
			echobuf.append(this->linebuf.reset(80, 24));
			echobuf.append(this->linebuf.querySize());
			rawbuffer.erase(0, rst_before+1);
			this->linebuf.overwrite_mode = false; // Force disable this, to return to normal state.
		}

		for (auto it = rawbuffer.begin(), eit = rawbuffer.end(); it != eit; ++it) {
			if (*it == '\r') {
				// Newlines aren't valid in ANSI sequences.
				ansi_code.buffer.clear();

				// Ensure the entire (possibly multiline) command is visible in the terminal history.
				echobuf.append(this->linebuf.end());

				// Newlines are received as \r, sent as \r\n.
				echobuf.append("\r\n");

				// Flush echo buffer.
				if (this->echo)
					this->raw_write(echobuf.data(), echobuf.size(), portMAX_DELAY);
				echobuf.clear();

				// Ready next commandline.
				std::string cmdbuf = this->linebuf.buffer;
				echobuf.append(this->linebuf.clear());
				echobuf.append(this->linebuf.querySize());

				// Parse & run the command line.
				if (!cmdbuf.empty()) {
					log_input.log(cmdbuf, LogTree::LOG_INFO);
					history.recordEntry(cmdbuf);
					lock.release();
					try {
						if (!this->parser.parse(this->weakself.lock(), cmdbuf))
							this->write("Unknown command!\n", portMAX_DELAY);
					} catch (std::exception &e) {
						BackTrace* trace = BackTrace::traceException();
						std::string diag = renderExceptionReport(trace, &e, std::string("in console command"));
						this->logtree.log(diag, LogTree::LOG_TRACE);
						this->write(diag+"\n");
					} catch (...) {
						BackTrace* trace = BackTrace::traceException();
						std::string diag = renderExceptionReport(trace, nullptr, std::string("in console command"));
						this->logtree.log(diag, LogTree::LOG_TRACE);
						this->write(diag+"\n");
					}
					lock.acquire();
				}
			} else if (*it == '\n') {
				// Ignore it.  We don't understand that so, if you sent us \r\n, we'll just trigger on the \r.

				// Newlines aren't valid in ANSI sequences.
				ansi_code.buffer.clear();
			} else if (*it == ANSICode::renderASCIIControlkey('L') || *it == ANSICode::renderASCIIControlkey('K')) {
				// Ctrl-L is customarily "screen redraw".  We will rerender the prompt.
				echobuf.append(this->linebuf.refresh());
			} else if (*it == '\x7f') {
				// DEL (sent by backspace key)

				// DEL isn't valid in ANSI sequences.
				ansi_code.buffer.clear();

				echobuf.append(this->linebuf.backspace());
			} else if (*it == ANSICode::renderASCIIControlkey('O')) {
				echobuf.append(stdsprintf("\r\n%s mode.  Last detected console size: %ux%u.\r\n", (this->linebuf.overwrite_mode ? "Overwrite" : "Insert"), this->linebuf.cols, this->linebuf.rows));
				echobuf.append(this->linebuf.refresh());
			} else if (*it == '\t') {
				// Tab isn't valid in ANSI sequences.
				ansi_code.buffer.clear();

				CommandParser::CompletionResult completed = this->parser.complete(this->linebuf.buffer, this->linebuf.cursor);
				std::string compl_append = completed.common_prefix.substr(completed.cursor);
				if (!compl_append.empty()) {
					echobuf += this->linebuf.setBuffer(this->linebuf.buffer.substr(0, this->linebuf.cursor) + compl_append + this->linebuf.buffer.substr(this->linebuf.cursor), this->linebuf.cursor + compl_append.size());
				} else if (completed.completions.size() > 1 && prevchar == '\t') {
					// No extension possible, but we got at least two tabs in a row and there are completions available.
					auto old_cursor = this->linebuf.cursor;
					// Print possible completions.
					echobuf.append(this->linebuf.set_cursor(std::string::npos));
					echobuf.append("\r\n");
					for (auto it = completed.completions.begin(), eit = completed.completions.end(); it != eit; ++it) {
						echobuf.append(*it);
						if (it+1 != eit)
							echobuf.append("  ");
					}
					echobuf.append("\r\n");
					echobuf.append(this->linebuf.setBuffer(this->linebuf.buffer, old_cursor));
				}
			} else {
#ifdef ANSICODE_TIMEOUT
				if (!ansi_code.buffer.empty() && last_ansi_tick + ANSICODE_TIMEOUT < get_tick64()) {
					// This control code took too long to come through.  Invalidating it.
					TRACE.log(timed_out_ansi_facility.c_str(), timed_out_ansi_facility.size(), LogTree::LOG_TRACE, ansi_code.buffer.data(), ansi_code.buffer.size(), true);
					ansi_code.buffer.clear();
				}
#endif
				if (*it == '\x1b')
					ansi_code.buffer.clear(); // Whatever code we were building got interrupted.  Toss it.

				enum ANSICode::ParseState ansi_state = ansi_code.parse(*it);
				switch (ansi_state) {
				case ANSICode::PARSE_EMPTY:
					configASSERT(0);
					continue;
				case ANSICode::PARSE_INCOMPLETE:
					 // Oh good.  Continue without adding it to the buffers yet.
#ifdef ANSICODE_TIMEOUT
					last_ansi_tick = get_tick64();
#endif
					prevchar = *it;
					continue;
				case ANSICode::PARSE_INVALID:
					// Guess it wasn't an ANSI code.  Put it back in the buffers.
					if (ansi_code.buffer[0] != '\x1b')
						echobuf.append(this->linebuf.update(ansi_code.buffer));
					ansi_code.buffer.clear();
					history_browse = false;
					prevchar = *it;
					continue;
				case ANSICode::PARSE_COMPLETE:
					// Well that IS interesting now, isn't it? Let's handle that.
					break; // See below.
				}

				if (ansi_code.name == "ARROW_LEFT") {
					echobuf.append(this->linebuf.left());
					history_browse = false;
				} else if (ansi_code.name == "ARROW_RIGHT") {
					echobuf.append(this->linebuf.right());
					history_browse = false;
				} else if (ansi_code.name == "HOME") {
					echobuf.append(this->linebuf.home());
					history_browse = false;
				} else if (ansi_code.name == "END") {
					echobuf.append(this->linebuf.end());
					history_browse = false;
				} else if (ansi_code.name == "ARROW_UP") {
					bool moved = false;
					if (history.isCurrent())
						history_browse = this->linebuf.buffer.empty(); // Browse if starting point is null, else search.
					std::string histline = history.goBack(this->linebuf.buffer, (history_browse ? 0 : this->linebuf.cursor), &moved);
					if (moved) {
						echobuf.append(this->linebuf.setBuffer(histline, (history_browse ? histline.size() : this->linebuf.cursor)));
					} else {
						echobuf.append(ANSICode::ASCII_BELL);
					}
				} else if (ansi_code.name == "ARROW_DOWN") {
					bool moved = false;
					std::string histline = history.goForward(this->linebuf.buffer, (history_browse ? 0 : this->linebuf.cursor), &moved);
					if (moved)
						echobuf.append(this->linebuf.setBuffer(histline, (history_browse ? histline.size() : this->linebuf.cursor)));
					else
						echobuf.append(ANSICode::ASCII_BELL);
				} else if (ansi_code.name == "INSERT") {
					/* Toggle overwrite mode.
					 *
					 * I don't know how to change the prompt blink type to
					 * display this, but if I did, I'd have to .refresh().
					 */
					this->linebuf.overwrite_mode = !this->linebuf.overwrite_mode;
				} else if (ansi_code.name == "DELETE") {
					echobuf.append(this->linebuf.delkey());
					history_browse = false;
				} else if (ansi_code.name == "CURSOR_POSITION_REPORT") {
					if (ansi_code.parameters.size() == 2)
						echobuf.append(this->linebuf.resize(ansi_code.parameters[1], ansi_code.parameters[0]));
				} else {
					// For now we don't support it, so we'll just pass it as a command.
					// We won't pass parameters for this type of code.  It'll be things like F1, F2.

					// Flush echo buffer.
					if (this->echo)
						this->raw_write(echobuf.data(), echobuf.size(), portMAX_DELAY);
					echobuf.clear();

					lock.release();
					try {
						this->parser.parse(this->weakself.lock(), std::string("ANSI_") + ansi_code.name);
					} catch (std::exception &e) {
						BackTrace* trace = BackTrace::traceException();
						std::string diag = renderExceptionReport(trace, &e, std::string("in console command"));
						this->logtree.log(diag, LogTree::LOG_TRACE);
						this->write(diag+"\n");
					} catch (...) {
						BackTrace* trace = BackTrace::traceException();
						std::string diag = renderExceptionReport(trace, nullptr, std::string("in console command"));
						this->logtree.log(diag, LogTree::LOG_TRACE);
						this->write(diag+"\n");
					}
					lock.acquire();
				}
				ansi_code.buffer.clear();
			}
			prevchar = *it;
		}

		// Flush echo buffer.
		if (this->echo)
			this->raw_write(echobuf.data(), echobuf.size(), portMAX_DELAY);

		echobuf.clear();
	}

	this->shutdown_complete();
}

std::string ConsoleSvc::CommandHistory::goBack(std::string line_to_cache, std::string::size_type cursor, bool *moved) {
	if (moved)
		*moved = true;

	if (this->history_position == this->history.end())
		this->cached_line = line_to_cache;

	if (this->history_position == this->history.begin()) {
		if (moved)
			*moved = false;
		return line_to_cache; // We can't go backward more.
	} else {
		auto it = this->history_position;
		while (--it != this->history.begin())
			if (it->substr(0, cursor) == line_to_cache.substr(0, cursor))
				break;

		if (it->substr(0, cursor) == line_to_cache.substr(0, cursor)) {
			// Oh good, we found something.
			this->history_position = it;
			return *this->history_position;
		} else {
			// No match found.
			// Ok, well.  Don't move.
			if (moved)
				*moved = false;
			return line_to_cache;
		}
	}
}

std::string ConsoleSvc::CommandHistory::goForward(std::string line_to_cache, std::string::size_type cursor, bool *moved) {
	if (moved)
		*moved = true;

	if (this->history_position == this->history.end()) {
		if (moved)
			*moved = false;

		return line_to_cache; // We can't go forward more.
	} else {
		auto it = this->history_position;
		while (++it != this->history.end())
			if (it->substr(0, cursor) == line_to_cache.substr(0, cursor))
				break;

		if (it == this->history.end()) {
			// No match found.
			if (this->cached_line.substr(0, cursor) == line_to_cache.substr(0, cursor)) {
				// Nevermind, the cached line matches!
				this->history_position = this->history.end();
				return this->cached_line;
			} else {
				// Ok, well.  Don't move.
				if (moved)
					*moved = false;
				return line_to_cache;
			}
		} else {
			// Oh good, we found something.
			this->history_position = it;
			return *this->history_position;
		}
	}
}

std::string ConsoleSvc::CommandHistory::goLatest(std::string line_to_cache, std::string::size_type cursor, bool *moved) {
	if (moved)
		*moved = true;

	if (this->history_position == this->history.end()) {
		if (moved)
			*moved = false;

		return line_to_cache; // We can't go forward more.
	} else {
		this->history_position = this->history.end();
		return this->cached_line; // Out of history.
	}
}

bool ConsoleSvc::CommandHistory::isCurrent() {
	return this->history_position == this->history.end();
}

void ConsoleSvc::CommandHistory::recordEntry(const std::string& line) {
	this->cached_line.clear();
	this->history.push_back(line);
	this->history_position = this->history.end();
	while (this->history.size() > this->length)
		this->history.pop_front();
}

std::string ConsoleSvc::InputBuffer::clear() {
	this->buffer.clear();
	this->cursor = 0;
	return this->refresh();
}

std::string ConsoleSvc::InputBuffer::reset(size_type cols, size_type rows) {
	this->clear();
	this->cols = cols;
	this->rows = rows;
	return this->refresh();
}

std::string ConsoleSvc::InputBuffer::update(std::string input) {
	if (this->buffer.size() + input.size() > this->maxlen)
		input.erase(this->maxlen - this->buffer.size()); // Ignore keystrokes, buffer full.

	if (input.empty())
		return ""; // Nothing to do.

	if (this->overwrite_mode && this->cursor != this->buffer.size())
		this->buffer.erase(this->cursor, input.size());

	this->buffer.insert(this->cursor, input);
	this->cursor += input.size();

	if (this->cursor == this->buffer.size())
		return input; // Appending.

	if (this->overwrite_mode)
		return input; // Overwriting. Skip the whole "creating space" business.

	// Looks like we're doing a midline insert.  Gotta create spaces so we don't overwrite.
	std::string out;
	for (auto it = input.begin(), eit = input.end(); it != eit; ++it)
		out += ANSICode::ANSI_INSERT_CHARACTER_POSITION + *it;

	if (this->buffer.size() >= this->cols) {
		// And, Sigh.  We have to manually rerender from here to end of line.
		out += this->buffer.substr(this->cursor) + ANSICode::ANSI_ERASE_TO_END_OF_LINE;
		for (size_type i = this->buffer.size(); i > this->cursor; --i)
			out += ANSICode::ASCII_BACKSPACE;
	}
	return out;
}

std::string ConsoleSvc::InputBuffer::setBuffer(std::string buffer, size_type cursor) {
	// This is really just refresh, with a buffer update in the middle.

	std::string out;
	size_type cursor_row = this->getCursorRow();
	if (cursor_row > 1)
		out += stdsprintf(ANSICode::ANSI_CURSOR_UP_INTFMT.c_str(), cursor_row-1);
	out += "\r";
	out += ANSICode::color(); // Reset terminal color.

	/* Only redraw if the buffer changed, else we're assuming no line count
	 * changes occurred (or matter), and our "erase to end of line" later will
	 * take care of any stray characters from oddness.  They can always request
	 * a redraw.
	 */
	if (this->buffer != buffer)
		out += ANSICode::ANSI_ERASE_DOWN;

	// The only actual setBuffer portion.
	this->buffer = buffer;
	this->cursor = (cursor == std::string::npos ? this->cursor : cursor);

	out += this->prompt + this->buffer + ANSICode::ANSI_ERASE_TO_END_OF_LINE;

	// Ensure our cursor is not past our buffer.
	if (this->cursor > this->buffer.size())
		this->cursor = this->buffer.size();

	for (size_type i = this->buffer.size(); i > this->cursor; --i)
		out += ANSICode::ASCII_BACKSPACE;

	return out;
}

std::string ConsoleSvc::InputBuffer::refresh() {
	return this->setBuffer(this->buffer, this->cursor);
}

ConsoleSvc::InputBuffer::size_type ConsoleSvc::InputBuffer::getCursorRow() {
	return divCeil<size_type>((this->prompt.size() + this->cursor), this->cols);
}

ConsoleSvc::InputBuffer::size_type ConsoleSvc::InputBuffer::getRowCount() {
	return divCeil<size_type>((this->prompt.size() + this->buffer.size()), this->cols);
}

std::string ConsoleSvc::InputBuffer::resize(size_type cols, size_type rows) {
	if (this->cols == cols && this->rows == rows)
		return ""; // NOOP

	// Update our perspective.  The terminal client will have handled any wrap.
	this->cols = cols;
	this->rows = rows;

	// Refresh for good measure.
	return this->refresh();
}

std::string ConsoleSvc::InputBuffer::querySize() {
	return ANSICode::ANSI_CURSOR_SAVE
			+ stdsprintf(ANSICode::ANSI_CURSOR_HOME_2INTFMT.c_str(), 999, 999)
			+ ANSICode::ANSI_CURSOR_QUERY_POSITION
			+ ANSICode::ANSI_CURSOR_RESTORE;
}

std::string ConsoleSvc::InputBuffer::home() {
	std::string out;

	// Reposition ourselves correctly, physically & logically.
	while (this->cursor) {
		out += ANSICode::ASCII_BACKSPACE;
		--this->cursor;
	}

	// Refresh for good measure.
	out += this->refresh();
	return out;
}

std::string ConsoleSvc::InputBuffer::end() {
	size_type old_cursor = this->cursor;
	this->cursor = this->buffer.size();
	return this->buffer.substr(old_cursor);
}

std::string ConsoleSvc::InputBuffer::left() {
	if (this->cursor == 0)
		return ""; // Nothing to do...
	this->cursor--;
	return ANSICode::ASCII_BACKSPACE;
}

std::string ConsoleSvc::InputBuffer::right() {
	if (this->cursor >= this->buffer.size())
		return ""; // Nothing to do...
	// Rerender the character to physically advance the cursor.
	return this->buffer.substr(this->cursor++, 1);
}

std::string ConsoleSvc::InputBuffer::backspace() {
	if (this->overwrite_mode)
		return this->left(); // Change behavior in overwrite mode.

	if (this->cursor == 0)
		return ""; // Can't backspace at start of line.

	this->cursor--;
	this->buffer.erase(this->cursor, 1);
	std::string out = ANSICode::ASCII_BACKSPACE + ANSICode::ANSI_DELETE_CHARACTER_POSITION; // Move back one space, then delete character slot.
	if (this->buffer.size() >= this->cols && this->cursor != this->buffer.size()) {
		// Sigh.  We have to manually rerender from here to end of line.
		out += this->buffer.substr(this->cursor) + ANSICode::ANSI_ERASE_TO_END_OF_LINE;
		for (size_type i = this->buffer.size(); i > this->cursor; --i)
			out += ANSICode::ASCII_BACKSPACE;
	}
	return out;
}

std::string ConsoleSvc::InputBuffer::delkey() {
	if (this->cursor >= this->buffer.size())
		return ""; // Can't delete at end of line.

	this->buffer.erase(this->cursor, 1);
	std::string out = ANSICode::ANSI_DELETE_CHARACTER_POSITION;
	if (this->buffer.size() >= this->cols && this->cursor != this->buffer.size()) {
		// Sigh.  We have to manually rerender from here to end of line.
		out += this->buffer.substr(this->cursor) + ANSICode::ANSI_ERASE_TO_END_OF_LINE;
		for (size_type i = this->buffer.size(); i > this->cursor; --i)
			out += ANSICode::ASCII_BACKSPACE;
	}
	return out;
}

std::string ConsoleSvc::InputBuffer::set_cursor(size_type cursor) {
	if (cursor > this->buffer.size())
		cursor = this->buffer.size();

	std::string out;
	// Move cursor backward to position.
	while (cursor < this->cursor) {
		out += ANSICode::ASCII_BACKSPACE;
		--this->cursor;
	}

	if (cursor > this->cursor) {
		// Move cursor forward to position.

		// Rerender the characters to physically advance the cursor.
		out += this->buffer.substr(this->cursor, cursor - this->cursor);
		// Logically advance the cursor.
		this->cursor = cursor;
	}

	return out;
}

std::string ConsoleSvcLogFormat(const std::string &message, enum LogTree::LogLevel level) {
	static const std::vector<std::string> colormap = {
			ANSICode::color(),                                          // LOG_SILENT:     "null" (reset) (placeholder)
			ANSICode::color(ANSICode::WHITE, ANSICode::RED, true),      // LOG_CRITICAL:   bold white on red
			ANSICode::color(ANSICode::RED, ANSICode::NOCOLOR, true),    // LOG_ERROR:      bold red
			ANSICode::color(ANSICode::YELLOW, ANSICode::NOCOLOR, true), // LOG_WARNING:    bold yellow
			ANSICode::color(ANSICode::TUROQUOISE),                      // LOG_NOTICE:     turquoise
			ANSICode::color(ANSICode::GREEN),                           // LOG_INFO:       green
			ANSICode::color(ANSICode::LIGHTGREY),                       // LOG_DIAGNOSTIC: lightgrey
			ANSICode::color(ANSICode::DARKGREY),                        // LOG_TRACE:      darkgrey
	};

	std::string color = ANSICode::color(ANSICode::BLUE);
	if (level < colormap.size())
		color = colormap.at(level);

	return color + stdsprintf("[%4.4s] ", LogTree::getLogLevelString(level)) + message + ANSICode::color() + "\n";
}
