/*
 * ConsoleSvc.cpp
 *
 *  Created on: Jan 17, 2018
 *      Author: jtikalsky
 */

#include <services/console/ConsoleSvc.h>
#include <IPMC.h>
#include "task.h"
#include <drivers/tracebuffer/TraceBuffer.h>
#include <libs/ThreadingPrimitives.h>

template <typename T> static inline T div_ceil(T val, T divisor) {
	return (val / divisor) + (val % divisor ? 1 : 0);
}

static void run_ConsoleSvc_thread(void *cb_ucs) {
	reinterpret_cast<ConsoleSvc*>(cb_ucs)->_run_thread();
}

/**
 * Instantiate a Console Service.
 *
 * @param parser The command parser to use.
 * @param name The name of the service for the process and such things.
 * @param logtree The log tree root for this service.
 * @param echo If true, enable echo and interactive management.
 * @param read_data_timeout The timeout for reads when data is available.
 */
ConsoleSvc::ConsoleSvc(CommandParser &parser, const std::string &name, LogTree &logtree, bool echo, TickType_t read_data_timeout)
	: parser(parser), name(name), logtree(logtree), log_input(logtree["input"]),
	  echo(echo), read_data_timeout(read_data_timeout), linebuf("> ", 2048),
	  shutdown(false), task(NULL) {
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
	vSemaphoreDelete(this->linebuf_mutex);
}

/**
 * Start the console service.  Call exactly once.
 */
void ConsoleSvc::start() {
	configASSERT(!this->task);
	configASSERT(xTaskCreate(run_ConsoleSvc_thread, name.c_str(), UWIPMC_STANDARD_STACK_SIZE, this, TASK_PRIORITY_INTERACTIVE, &this->task));
}
/**
 * Write to the console without disrupting the prompt.
 *
 * \note A timeout of 0 will be a little unlikely to succeed.  Try 1.
 *
 * @param data The data to write out.
 * @param timeout How long to wait.
 * @return
 */
bool ConsoleSvc::write(std::string data, TickType_t timeout) {
	AbsoluteTimeout abstimeout(timeout);
	if (!xSemaphoreTake(this->linebuf_mutex, abstimeout.get_timeout()))
		return false;

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
	while ( ( pos = data.find_first_of("\n") ) != std::string::npos ) {
		out += data.substr(0, pos);
		data.erase(0, pos+1);
		if (data.size())
			out += std::string("\r\n") /* back down to prompt */;

		// Either way, we just put out a line, so our cursor is 0 now.
		this->safe_write_line_cursor = 0;
	}

	// Ok, done with all LINES.  If we have a partial, we need to deal with it now.
	out += data; // All remainder.
	this->safe_write_line_cursor = data.size();

	// And now return to the prompt line.
	out += "\r\n";
	out += this->linebuf.refresh();
	out += this->linebuf.set_cursor(input_cursor);
	this->raw_write(out.data(), out.size(), abstimeout.get_timeout());
	xSemaphoreGive(this->linebuf_mutex);
	return true;
}

void ConsoleSvc::_run_thread() {
	this->logtree.log("Starting Console Service \"" + this->name + "\"", LogTree::LOG_INFO);
	const std::string ctrlc_erased_facility = this->logtree.path + ".ctrlc_erased";
#ifdef ANSICODE_TIMEOUT
	const std::string timed_out_ansi_facility = this->logtree.path + ".timed_out_ansi";
#endif

	// We will hold this semaphore as the rule, not the exception, releasing it only for other transactions.
	xSemaphoreTake(this->linebuf_mutex, portMAX_DELAY);

	if (this->echo)
		this-raw_write(this->linebuf.prompt.data(), this->linebuf.prompt.size(), portMAX_DELAY);
	if (this->echo)
		this->raw_write(ANSICode::ANSI_CURSOR_QUERY_POSITION.data(), ANSICode::ANSI_CURSOR_QUERY_POSITION.size(), portMAX_DELAY);

	static const char reset_control_keys[] = {
			ANSICode::render_ascii_controlkey('R'),
			ANSICode::render_ascii_controlkey('C'),
			ANSICode::render_ascii_controlkey('D'),
			'\0', // String Terminator
	};
	ANSICode ansi_code;
	uint64_t last_ansi_tick = 0;
	CommandHistory history(50);
	char prevchar = '\0';
	bool history_browse = false;
	while (true) {
		char readbuf[128];
		if (this->shutdown)
			break;
		xSemaphoreGive(this->linebuf_mutex);
		ssize_t bytes_read = this->raw_read(readbuf, 128, portMAX_DELAY, this->read_data_timeout);
		xSemaphoreTake(this->linebuf_mutex, portMAX_DELAY);
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
			history.go_latest("",0);
			echobuf.append(this->linebuf.reset(80, 24)); // This will internally send a new terminal size query.
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

				// Parse & run the command line.
				if (!cmdbuf.empty()) {
					log_input.log(cmdbuf, LogTree::LOG_INFO);
					history.record_entry(cmdbuf);
					xSemaphoreGive(this->linebuf_mutex);
					if (!this->parser.parse(*this, cmdbuf))
						this->write("Unknown command!\n", portMAX_DELAY);
					xSemaphoreTake(this->linebuf_mutex, portMAX_DELAY);
				}
			}
			else if (*it == '\n') {
				// Ignore it.  We don't understand that so, if you sent us \r\n, we'll just trigger on the \r.

				// Newlines aren't valid in ANSI sequences.
				ansi_code.buffer.clear();
			}
			else if (*it == ANSICode::render_ascii_controlkey('L') || *it == ANSICode::render_ascii_controlkey('K')) {
				// Ctrl-L is customarily "screen redraw".  We will rerender the prompt.
				echobuf.append(this->linebuf.refresh());
			}
			else if (*it == '\x7f') {
				// DEL (sent by backspace key)

				// DEL isn't valid in ANSI sequences.
				ansi_code.buffer.clear();

				echobuf.append(this->linebuf.backspace());
			}
			else if (*it == ANSICode::render_ascii_controlkey('O')) {
				echobuf.append(stdsprintf("\r\n%s mode.  Last detected console size: %ux%u.\r\n", (this->linebuf.overwrite_mode ? "Overwrite" : "Insert"), this->linebuf.cols, this->linebuf.rows));
				echobuf.append(this->linebuf.refresh());
			}
			else if (*it == '\t') {
				// Tab isn't valid in ANSI sequences.
				ansi_code.buffer.clear();

				CommandParser::CompletionResult completed = this->parser.complete(this->linebuf.buffer, this->linebuf.cursor);
				std::string compl_append = completed.common_prefix.substr(completed.cursor);
				if (!compl_append.empty()) {
					echobuf += this->linebuf.set_buffer(this->linebuf.buffer.substr(0, this->linebuf.cursor) + compl_append + this->linebuf.buffer.substr(this->linebuf.cursor), this->linebuf.cursor + compl_append.size());
				}
				else if (completed.completions.size() > 1 && prevchar == '\t') {
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
					echobuf.append(this->linebuf.set_buffer(this->linebuf.buffer, old_cursor));
				}
			}
			else {
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
				}
				else if (ansi_code.name == "ARROW_RIGHT") {
					echobuf.append(this->linebuf.right());
					history_browse = false;
				}
				else if (ansi_code.name == "HOME") {
					echobuf.append(this->linebuf.home());
					history_browse = false;
				}
				else if (ansi_code.name == "END") {
					echobuf.append(this->linebuf.end());
					history_browse = false;
				}
				else if (ansi_code.name == "ARROW_UP") {
					bool moved = false;
					if (history.is_current())
						history_browse = this->linebuf.buffer.empty(); // Browse if starting point is null, else search.
					std::string histline = history.go_back(this->linebuf.buffer, (history_browse ? 0 : this->linebuf.cursor), &moved);
					if (moved)
						echobuf.append(this->linebuf.set_buffer(histline, (history_browse ? histline.size() : this->linebuf.cursor)));
					else
						echobuf.append(ANSICode::ASCII_BELL);
				}
				else if (ansi_code.name == "ARROW_DOWN") {
					bool moved = false;
					std::string histline = history.go_forward(this->linebuf.buffer, (history_browse ? 0 : this->linebuf.cursor), &moved);
					if (moved)
						echobuf.append(this->linebuf.set_buffer(histline, (history_browse ? histline.size() : this->linebuf.cursor)));
					else
						echobuf.append(ANSICode::ASCII_BELL);
				}
				else if (ansi_code.name == "INSERT") {
					/* Toggle overwrite mode.
					 *
					 * I don't know how to change the prompt blink type to
					 * display this, but if I did, I'd have to .refresh().
					 */
					this->linebuf.overwrite_mode = !this->linebuf.overwrite_mode;
				}
				else if (ansi_code.name == "DELETE") {
					echobuf.append(this->linebuf.delkey());
					history_browse = false;
				}
				else if (ansi_code.name == "CURSOR_POSITION_REPORT") {
					if (ansi_code.parameters.size() == 2)
						echobuf.append(this->linebuf.resize(ansi_code.parameters[1], ansi_code.parameters[0]));
				}
				else {
					// For now we don't support it, so we'll just pass it as a command.
					// We won't pass parameters for this type of code.  It'll be things like F1, F2.

					// Flush echo buffer.
					if (this->echo)
						this->raw_write(echobuf.data(), echobuf.size(), portMAX_DELAY);
					echobuf.clear();

					xSemaphoreGive(this->linebuf_mutex);
					this->parser.parse(*this, std::string("ANSI_") + ansi_code.name);
					xSemaphoreTake(this->linebuf_mutex, portMAX_DELAY);
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
	xSemaphoreGive(this->linebuf_mutex);
	this->shutdown_complete();
	if (this->shutdown & 2)
		delete this;
	vTaskDelete(NULL);
}

/**
 * Step back in time.
 *
 * @param line_to_cache The current input line.
 * @param cursor The cursor position, used for prefix-search.
 * @param moved [out] true if the history position changed, else false.
 * @return The new input line.
 */
std::string ConsoleSvc::CommandHistory::go_back(std::string line_to_cache, std::string::size_type cursor, bool *moved) {
	if (moved)
		*moved = true;

	if (this->history_position == this->history.end())
		this->cached_line = line_to_cache;

	if (this->history_position == this->history.begin()) {
		if (moved)
			*moved = false;
		return line_to_cache; // We can't go backward more.
	}
	else {
		auto it = this->history_position;
		while (--it != this->history.begin())
			if (it->substr(0, cursor) == line_to_cache.substr(0, cursor))
				break;

		if (it->substr(0, cursor) == line_to_cache.substr(0, cursor)) {
			// Oh good, we found something.
			this->history_position = it;
			return *this->history_position;
		}
		else {
			// No match found.
			// Ok, well.  Don't move.
			if (moved)
				*moved = false;
			return line_to_cache;
		}
	}
}

/**
 * Step forward in time.
 *
 * @param line_to_cache The current input line.
 * @param cursor The cursor position, used for prefix-search.
 * @param moved [out] true if the history position changed, else false.
 * @return The new input line.
 */
std::string ConsoleSvc::CommandHistory::go_forward(std::string line_to_cache, std::string::size_type cursor, bool *moved) {
	if (moved)
		*moved = true;
	if (this->history_position == this->history.end()) {
		if (moved)
			*moved = false;
		return line_to_cache; // We can't go forward more.
	}
	else {
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
			}
			else {
				// Ok, well.  Don't move.
				if (moved)
					*moved = false;
				return line_to_cache;
			}
		}
		else {
			// Oh good, we found something.
			this->history_position = it;
			return *this->history_position;
		}
	}
}

/**
 * Step to the present.
 *
 * @param line_to_cache The current input line.
 * @param cursor The cursor position, used for prefix-search.
 * @param moved [out] true if the history position changed, else false.
 * @return The new input line.
 */
std::string ConsoleSvc::CommandHistory::go_latest(std::string line_to_cache, std::string::size_type cursor, bool *moved) {
	if (moved)
		*moved = true;
	if (this->history_position == this->history.end()) {
		if (moved)
			*moved = false;
		return line_to_cache; // We can't go forward more.
	}
	else {
		this->history_position = this->history.end();
		return this->cached_line; // Out of history.
	}
}

/**
 * Identify whether the current history position is the present.
 * @return true if the history position is the present, else false
 */
bool ConsoleSvc::CommandHistory::is_current() {
	return this->history_position == this->history.end();
}

/**
 * Save an entered input line.
 *
 * @param line The input line to record
 */
void ConsoleSvc::CommandHistory::record_entry(std::string line) {
	this->cached_line.clear();
	this->history.push_back(line);
	this->history_position = this->history.end();
	while (this->history.size() > this->length)
		this->history.pop_front();
}

/**
 * Clear the input buffer.
 * @return echo data
 */
std::string ConsoleSvc::InputBuffer::clear() {
	this->buffer.clear();
	this->cursor = 0;
	return this->refresh();
}

/**
 * Reset the input buffer.
 * @return echo data
 */
std::string ConsoleSvc::InputBuffer::reset(size_type cols, size_type rows) {
	this->clear();
	this->cols = cols;
	this->rows = rows;
	return this->refresh();
}

/**
 * Insert characters into the buffer.
 * @param input The data to insert.
 * @return echo data
 */
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

/**
 * Update the buffer and refresh the screen
 * @param buffer New buffer
 * @param cursor New cursor
 * @return echo data
 */
std::string ConsoleSvc::InputBuffer::set_buffer(std::string buffer, size_type cursor) {
	// This is really just refresh, with a buffer update in the middle.

	std::string out;
	size_type cursor_row = this->get_cursor_row();
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

	// The only actual set_buffer portion.
	this->buffer = buffer;
	this->cursor = (cursor == std::string::npos ? this->cursor : cursor);

	out += this->prompt + this->buffer + ANSICode::ANSI_ERASE_TO_END_OF_LINE;

	// Ensure our cursor is not past our buffer.
	if (this->cursor > this->buffer.size())
		this->cursor = this->buffer.size();

	out += this->query_size();

	for (size_type i = this->buffer.size(); i > this->cursor; --i)
		out += ANSICode::ASCII_BACKSPACE;
	return out;
}

/**
 * Return echo data to erase and reprint the line.
 * @return echo data
 */
std::string ConsoleSvc::InputBuffer::refresh() {
	return this->set_buffer(this->buffer, this->cursor);
}

/**
 * Return the current row position of the cursor, relative to the start, one-indexed.
 * @return The row the cursor is presently on.
 */
ConsoleSvc::InputBuffer::size_type ConsoleSvc::InputBuffer::get_cursor_row() {
	return div_ceil<size_type>((this->prompt.size() + this->cursor), this->cols);
}

/**
 * Return the number of rows occupied by this input buffer.
 * @return The number of rows
 */
ConsoleSvc::InputBuffer::size_type ConsoleSvc::InputBuffer::get_rowcount() {
	return div_ceil<size_type>((this->prompt.size() + this->buffer.size()), this->cols);
}

/**
 * Resize the buffer's perception of the console size.
 * @param cols columns
 * @param rows rows
 * @return echo data
 */
std::string ConsoleSvc::InputBuffer::resize(size_type cols, size_type rows) {
	if (this->cols == cols && this->rows == rows)
		return ""; // NOOP

	// Update our perspective.  The terminal client will have handled any wrap.
	this->cols = cols;
	this->rows = rows;

	// Refresh for good measure.
	return this->refresh();
}

/**
 * Query the terminal size.
 * @return echo data
 */
std::string ConsoleSvc::InputBuffer::query_size() {
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

/**
 * Format a log message for console output.
 *
 * @param message The log message
 * @param level The loglevel of the message
 * @return The message formatted for console output
 */
std::string ConsoleSvc_log_format(const std::string &message, enum LogTree::LogLevel level) {
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

	return color + stdsprintf("[%4.4s] ", LogTree::LogLevel_strings[level]) + message + ANSICode::color() + "\n";
}
