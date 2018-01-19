/*
 * UARTConsole.cpp
 *
 *  Created on: Jan 17, 2018
 *      Author: jtikalsky
 */

#include <services/console/UARTConsoleSvc.h>
#include <IPMC.h>
#include "task.h"
#include <drivers/tracebuffer/TraceBuffer.h>
#include <libs/ThreadingPrimitives.h>

static void run_uartconsolesvc_thread(void *cb_ucs) {
	reinterpret_cast<UARTConsoleSvc*>(cb_ucs)->_run_thread();
}

/**
 * Instantiate a UART Console Service.
 *
 * @param uart The UART to operate on.
 * @param parser The command parser to use.
 * @param name The name of the service for the process and such things.
 * @param logtree The log tree root for this service.
 * @param echo If true, enable echo and interactive management.
 * @param read_data_timeout The timeout for reads when data is waiting to be parsed.
 */
UARTConsoleSvc::UARTConsoleSvc(UART &uart, CommandParser &parser, const std::string &name, LogTree &logtree, bool echo, TickType_t read_data_timeout)
	: uart(uart), parser(parser), name(name), logtree(logtree),
	  log_input(logtree["input"]), log_output(logtree["output"]),
	  echo(echo), read_data_timeout(read_data_timeout),
	  linebuf(this->prompt, 256) {
	this->linebuf_mutex = xSemaphoreCreateMutex();
	configASSERT(this->linebuf_mutex);
	configASSERT(xTaskCreate(run_uartconsolesvc_thread, name.c_str(), configMINIMAL_STACK_SIZE+1024, this, TASK_PRIORITY_SERVICE, NULL));
}

UARTConsoleSvc::~UARTConsoleSvc() {
	vSemaphoreDelete(this->linebuf_mutex);
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
bool UARTConsoleSvc::safe_write(std::string data, TickType_t timeout) {
	AbsoluteTimeout abstimeout(timeout);
	if (!xSemaphoreTake(this->linebuf_mutex, abstimeout.get_timeout()))
		return false;

	/* 1. Push prompt line (assumed current) up one to create a throwaway line.
	 * 2. Move cursor up to prompt line.
	 * 3. Insert, pushing prompt line down overwriting throwaway line.
	 */
	static const std::string insertline = std::string("\r\n") + ANSICode::ANSI_CURSOR_UP_ONE + ANSICode::VT102_INSERT_LINE;
	std::string out;
	if (this->safe_write_line_cursor) {
		// Restore cursor.
		out += ANSICode::ANSI_CURSOR_UP_ONE + stdsprintf(ANSICode::ANSI_ABSOLUTE_HORIZONTAL_POSITION_INTFMT.c_str(), this->safe_write_line_cursor + 1 /* 1-indexed */);
	}
	else {
		// Create line.
		out += insertline;
	}
	std::string::size_type pos;
	while ( ( pos = data.find_first_of("\n") ) != std::string::npos ) {
		out += data.substr(0, pos);
		data.erase(0, pos+1);
		if (data.size())
			out += std::string("\r\n") /* back down to prompt */ + insertline;

		// Either way, we just put out a line, so our cursor is 0 now.
		this->safe_write_line_cursor = 0;
	}

	// Ok, done with all LINES.  If we have a partial, we need to deal with it now.
	out += data; // All remainder.
	this->safe_write_line_cursor = data.size();

	// And now return to the prompt line.
	out += "\r\n";
	out += this->linebuf.refresh_cursor();
	this->uart.write(out.data(), out.size(), abstimeout.get_timeout());
	xSemaphoreGive(this->linebuf_mutex);
	return true;
}

void UARTConsoleSvc::_run_thread() {
	this->logtree.log("Starting UART Console Service \"" + this->name + "\"", LogTree::LOG_INFO);
	const std::string ctrlc_erased_facility = this->logtree.path + ".ctrlc_erased";
	std::function<void(std::string)> console_output = [this](std::string data) -> void {
		this->log_output.log(data, LogTree::LOG_DIAGNOSTIC);
		this->safe_write(data, portMAX_DELAY);
	};

	// We will hold this semaphore as the rule, not the exception, releasing it only for other transactions.
	xSemaphoreTake(this->linebuf_mutex, portMAX_DELAY);

	if (this->echo)
		this->uart.write(this->prompt.data(), this->prompt.size(), portMAX_DELAY);

	ANSICode ansi_code;
	uint64_t last_ansi_tick = 0;
	CommandHistory history(50);
	while (true) {
		char readbuf[128];
		xSemaphoreGive(this->linebuf_mutex);
		size_t bytes_read = this->uart.read(readbuf, 128, portMAX_DELAY, this->read_data_timeout);
		xSemaphoreTake(this->linebuf_mutex, portMAX_DELAY);
		std::string rawbuffer(readbuf, bytes_read);
		std::string echobuf;

		std::string::size_type rst_before = rawbuffer.find_last_of("\x03\x04\x12");
		if (rst_before != std::string::npos) {
			// Clear all buffer found before any Ctrl-C (or D, or R), and replace it with an empty line to retrigger the prompt.
			std::string tracebuf = this->linebuf.buffer + rawbuffer.substr(0, rst_before+1);
			TRACE.log(ctrlc_erased_facility.c_str(), ctrlc_erased_facility.size(), LogTree::LOG_TRACE, tracebuf.data(), tracebuf.size(), true);
			echobuf.append("\r\n");
			echobuf.append(this->linebuf.clear());
			rawbuffer.erase(0, rst_before+1);
			this->linebuf.overwrite_mode = false; // Force disable this, to return to normal state.
		}

		for (auto it = rawbuffer.begin(), eit = rawbuffer.end(); it != eit; ++it) {
			if (*it == '\r') {
				// Newlines aren't valid in ANSI sequences.
				if (!ansi_code.buffer.empty()) {
					echobuf.append(this->linebuf.update(ansi_code.buffer.substr(1))); // Don't include the ESC.
					ansi_code.buffer.clear();
				}

				// Newlines are received as \r, sent as \r\n.
				echobuf.append("\r\n");

				// Flush echo buffer.
				if (this->echo)
					this->uart.write(echobuf.data(), echobuf.size(), portMAX_DELAY);
				echobuf.clear();

				// Ready next commandline.
				std::string cmdbuf = this->linebuf.buffer;
				echobuf.append(this->linebuf.clear());

				// Parse & run the command line.
				if (!cmdbuf.empty()) {
					log_input.log(cmdbuf, LogTree::LOG_INFO);
					history.record_entry(cmdbuf);
					xSemaphoreGive(this->linebuf_mutex);
					if (!this->parser.parse(console_output, cmdbuf))
						console_output("Unknown command!\n");
					xSemaphoreTake(this->linebuf_mutex, portMAX_DELAY);
				}
			}
			else if (*it == '\n') {
				// Ignore it.  We don't understand that so, if you sent us \r\n, we'll just trigger on the \r.

				// Newlines aren't valid in ANSI sequences.
				if (!ansi_code.buffer.empty()) {
					echobuf.append(this->linebuf.update(ansi_code.buffer.substr(1))); // Don't include the ESC.
					ansi_code.buffer.clear();
				}
			}
			else if (*it == '\x0c') {
				// Ctrl L is customarily "screen redraw".  We will rerender the prompt.
				echobuf.append(this->linebuf.refresh());
			}
			else if (*it == '\x7f') {
				// DEL (sent by backspace key)

				// DEL isn't valid in ANSI sequences.
				if (!ansi_code.buffer.empty()) {
					echobuf.append(this->linebuf.update(ansi_code.buffer.substr(1))); // Don't include the ESC.
					ansi_code.buffer.clear();
				}

				echobuf.append(this->linebuf.backspace());
			}
			else {
				if (!ansi_code.buffer.empty() && last_ansi_tick + 50 < get_tick64()) {
					// This control code took too long to come through.  Invalidating it.
					echobuf.append(this->linebuf.update(ansi_code.buffer.substr(1))); // Don't include the ESC.
					ansi_code.buffer.clear();
				}
				enum ANSICode::ParseState ansi_state = ansi_code.parse(*it);
				switch (ansi_state) {
				case ANSICode::PARSE_EMPTY:
					configASSERT(0);
					continue;
				case ANSICode::PARSE_INCOMPLETE:
					 // Oh good.  Continue without adding it to the buffers yet.
					last_ansi_tick = get_tick64();
					continue;
				case ANSICode::PARSE_INVALID:
					// Guess it wasn't an ANSI code.  Put it back in the buffers.
					if (ansi_code.buffer[0] == '\x1b')
						ansi_code.buffer.erase(0,1); // Discard the ESC.
					echobuf.append(this->linebuf.update(ansi_code.buffer));
					ansi_code.buffer.clear();
					continue;
				case ANSICode::PARSE_COMPLETE:
					// Well that IS interesting now, isn't it? Let's handle that.
					break; // See below.
				}

				if (ansi_code.name == "ARROW_LEFT") {
					echobuf.append(this->linebuf.left());
				}
				else if (ansi_code.name == "ARROW_RIGHT") {
					echobuf.append(this->linebuf.right());
				}
				else if (ansi_code.name == "HOME") {
					echobuf.append(this->linebuf.home());
				}
				else if (ansi_code.name == "END") {
					echobuf.append(this->linebuf.end());
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
				}
				else {
					// For now we don't support any, so we'll just pass them as commands.
					// We won't pass parameters for this type of code.  It'll be things like F1, F2.
					xSemaphoreGive(this->linebuf_mutex);
					this->parser.parse(console_output, std::string("ANSI_") + ansi_code.name);
					xSemaphoreTake(this->linebuf_mutex, portMAX_DELAY);
				}
				ansi_code.buffer.clear();
			}
		}

		// Flush echo buffer.
		if (this->echo)
			this->uart.write(echobuf.data(), echobuf.size(), portMAX_DELAY);
		echobuf.clear();
	}
	xSemaphoreGive(this->linebuf_mutex); // Never reached.
}

/**
 * Step back in time.
 *
 * @param line_to_cache The current input line.
 * @return The new input line.
 */
std::string UARTConsoleSvc::CommandHistory::go_back(std::string line_to_cache) {
	if (this->history_position == this->history.end())
		this->cached_line = line_to_cache;
	if (this->history_position != this->history.begin())
		--this->history_position; // Go back if we can, else stay at the beginning of time.
	return *this->history_position;
}

/**
 * Step forward in time.
 *
 * @param line_to_cache The current input line.
 * @return The new input line.
 */
std::string UARTConsoleSvc::CommandHistory::go_forward(std::string line_to_cache) {
	if (this->history_position == this->history.end()) {
		return line_to_cache; // We can't go forward more.
	}
	else {
		++history_position;
		if (this->history_position == this->history.end())
			return this->cached_line; // Out of history.
		else
			return *this->history_position; // Here you go.
	}
}

/**
 * Save an entered input line.
 *
 * @param line The input line to record
 */
void UARTConsoleSvc::CommandHistory::record_entry(std::string line) {
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
std::string UARTConsoleSvc::InputBuffer::clear() {
	this->buffer.clear();
	this->cursor = 0;
	return this->refresh();
}

/**
 * Insert characters into the buffer.
 * @param input The data to insert.
 * @return echo data
 */
std::string UARTConsoleSvc::InputBuffer::update(std::string input) {
	if (this->buffer.size() >= this->maxlen)
		return ""; // Ignore keystrokes, buffer full.
	if (this->overwrite_mode && this->cursor != this->buffer.size())
		this->buffer.erase(this->cursor, input.size());

	this->buffer.insert(this->cursor, input);
	this->cursor += input.size();
	if (this->cursor == this->buffer.size())
		return input;

	if (this->overwrite_mode)
		return input; // Skip the whole "creating space" business.

	// Looks like we're doing a midline insert.  Gotta create spaces so we don't overwrite.
	std::string out;
	for (auto it = input.begin(), eit = input.end(); it != eit; ++it)
		out += ANSICode::VT102_INSERT_CHARACTER_POSITION + *it;
	return out;
}

/**
 * Return echo data to erase and reprint the line.
 * @return echo data
 */
std::string UARTConsoleSvc::InputBuffer::refresh() {
	std::string out = "\r";
	out += ANSICode::ANSI_ERASE_TO_END_OF_LINE;
	out += this->prompt + this->buffer;
	if (this->cursor != this->buffer.size())
		out += this->refresh_cursor();
	return out;
}

/**
 * Return echo data to restore the correct cursor position.
 * @return echo data
 */
std::string UARTConsoleSvc::InputBuffer::refresh_cursor() {
	return stdsprintf(ANSICode::ANSI_ABSOLUTE_HORIZONTAL_POSITION_INTFMT.c_str(), this->prompt.size() + this->cursor + 1 /* 1-indexed */);
}

std::string UARTConsoleSvc::InputBuffer::home() {
	std::string out;
	if (this->cursor != 0)
		out = stdsprintf(ANSICode::ANSI_CURSOR_BACK_INTFMT.c_str(), this->cursor);
	this->cursor = 0;
	return out;
}

std::string UARTConsoleSvc::InputBuffer::end() {
	std::string out;
	if (this->cursor != this->buffer.size())
		out = stdsprintf(ANSICode::ANSI_CURSOR_FORWARD_INTFMT.c_str(), this->buffer.size() - this->cursor);
	this->cursor = this->buffer.size();
	return out;
}

std::string UARTConsoleSvc::InputBuffer::left() {
	if (this->cursor == 0)
		return ""; // Nothing to do...
	this->cursor--;
	return ANSICode::ANSI_CURSOR_BACK_ONE;
}

std::string UARTConsoleSvc::InputBuffer::right() {
	if (this->cursor == this->buffer.size())
		return ""; // Nothing to do...
	this->cursor++;
	return ANSICode::ANSI_CURSOR_FORWARD_ONE;
}

std::string UARTConsoleSvc::InputBuffer::backspace() {
	if (this->overwrite_mode)
		return this->left(); // Change behavior in overwrite mode.

	if (this->cursor == 0)
		return ""; // Can't backspace at start of line.

	this->cursor--;
	this->buffer.erase(this->cursor, 1);
	return ANSICode::ANSI_CURSOR_BACK_ONE + ANSICode::VT102_DELETE_CHARACTER_POSITION; // Move back one space, then delete character slot.
}

std::string UARTConsoleSvc::InputBuffer::delkey() {
	if (this->cursor == this->buffer.size())
		return ""; // Can't delete at end of line.

	this->buffer.erase(this->cursor, 1);
	return ANSICode::VT102_DELETE_CHARACTER_POSITION;
}
