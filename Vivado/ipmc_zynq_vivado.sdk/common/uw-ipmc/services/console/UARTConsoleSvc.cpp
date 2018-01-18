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
	  command_history_length(100) {
	this->linebuf_mutex = xSemaphoreCreateMutex();
	configASSERT(this->linebuf_mutex);
	configASSERT(xTaskCreate(run_uartconsolesvc_thread, name.c_str(), configMINIMAL_STACK_SIZE+256, this, TASK_PRIORITY_SERVICE, NULL));
}

UARTConsoleSvc::~UARTConsoleSvc() {
	vSemaphoreDelete(this->linebuf_mutex);
}

bool UARTConsoleSvc::safe_write(std::string data, TickType_t timeout) {
	AbsoluteTimeout abstimeout(timeout);
	if (!xSemaphoreTake(this->linebuf_mutex, abstimeout.get_timeout()))
		return false;
	windows_newline(data);
	data.replace(0, 0, std::string("\r") + ANSICode::ANSI_ERASE_TO_END_OF_LINE);
	data.append(this->prompt);
	data.append(this->linebuf);
	this->uart.write(data.data(), data.size(), abstimeout.get_timeout());
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
	while (true) {
		char readbuf[128];
		xSemaphoreGive(this->linebuf_mutex);
		size_t bytes_read = this->uart.read(readbuf, 128, portMAX_DELAY, this->read_data_timeout);
		xSemaphoreTake(this->linebuf_mutex, portMAX_DELAY);
		std::string rawbuffer(readbuf, bytes_read);

		std::string::size_type rst_before = rawbuffer.find_last_of("\x03\x04\x12");
		if (rst_before != std::string::npos) {
			// Clear all buffer found before any Ctrl-C (or D, or R), and replace it with an empty line to retrigger the prompt.
			this->linebuf.append(rawbuffer.substr(0, rst_before+1));
			TRACE.log(ctrlc_erased_facility.c_str(), ctrlc_erased_facility.size(), LogTree::LOG_TRACE, this->linebuf.data(), this->linebuf.size(), true);
			this->linebuf.clear();
			rawbuffer.replace(0, rst_before+1, "\r");
		}

		std::string echobuf;
		for (auto it = rawbuffer.begin(), eit = rawbuffer.end(); it != eit; ++it) {
			if (*it == '\r') {
				// Newlines aren't valid in ANSI sequences.
				echobuf.append(ansi_code.buffer);
				this->linebuf.append(ansi_code.buffer);
				ansi_code.buffer.clear();

				// Newlines are received as \r, sent as \r\n.
				echobuf.append("\r\n");

				// Flush echo buffer.
				if (this->echo)
					this->uart.write(echobuf.data(), echobuf.size(), portMAX_DELAY);
				echobuf.clear();

				// Ready next commandline.
				std::string cmdbuf = linebuf;
				linebuf.clear();
				if (this->echo)
					this->uart.write(this->prompt.data(), this->prompt.size(), portMAX_DELAY);

				// Parse & run the command line.
				if (!cmdbuf.empty()) {
					log_input.log(cmdbuf, LogTree::LOG_INFO);
					this->command_history.push_front(cmdbuf);
					if (this->command_history.size() > this->command_history_length)
						this->command_history.resize(this->command_history_length);
					xSemaphoreGive(this->linebuf_mutex);
					if (!this->parser.parse(console_output, cmdbuf))
						console_output("Unknown command!\n");
					xSemaphoreTake(this->linebuf_mutex, portMAX_DELAY);
				}
			}
			else if (*it == '\n') {
				// Ignore it.  We don't understand that so, if you sent us \r\n, we'll just trigger on the \r.

				// Newlines aren't valid in ANSI sequences.
				echobuf.append(ansi_code.buffer);
				this->linebuf.append(ansi_code.buffer);
				ansi_code.buffer.clear();
			}
			else if (*it == '\x7f') {
				// DEL (sent by backspace key)

				// DEL isn't valid in ANSI sequences.
				echobuf.append(ansi_code.buffer);
				this->linebuf.append(ansi_code.buffer);
				ansi_code.buffer.clear();

				if (!this->linebuf.empty()) {
					this->linebuf.pop_back();
					echobuf += "\x08 \x08"; // Move back one space, overwrite with whitespace, move back one space again.
				}
			}
			else {
				if (!ansi_code.buffer.empty() && last_ansi_tick + 50 < get_tick64()) {
					// This control code took too long to come through.  Invalidating it.
					echobuf.append(ansi_code.buffer);
					this->linebuf.append(ansi_code.buffer);
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
					echobuf.append(ansi_code.buffer);
					this->linebuf.append(ansi_code.buffer);
					ansi_code.buffer.clear();
					continue;
				case ANSICode::PARSE_COMPLETE:
					// Well that IS interesting now, isn't it? Let's handle that.
					break; // See below.
				}
				if (0) {
					// TODO
				}
				else {
					// For now we don't support any, so we'll just pass them as commands.
					// We won't pass parameters for this type of code.  It'll be things like F1, F2.
					xSemaphoreGive(this->linebuf_mutex);
					this->parser.parse(console_output, std::string("ANSI_") + ansi_code.name);
					xSemaphoreTake(this->linebuf_mutex, portMAX_DELAY);
					ansi_code.buffer.clear();
				}
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
 * Parse the current buffer as an ANSI control code.
 *
 * @return The status of the parsing.
 */
enum ANSICode::ParseState ANSICode::parse() {
	this->code.clear();
	this->name.clear();
	this->parameters.clear();

	if (this->buffer.empty())
		return PARSE_EMPTY;

	if (this->buffer[0] != '\x1B')
		return PARSE_INVALID; // Not a control sequence.
	else if (this->buffer.size() == 1)
		return PARSE_INCOMPLETE; // We have an escape code, but nothing else yet.
	else if (this->buffer.size() == 2) {
		if (this->buffer[1] == 'O')
			return PARSE_INCOMPLETE; // O is apparently an annoying special case, starting a two letter code.
		else if (
				('A' <= this->buffer[1] && this->buffer[1] <= 'Z') ||
				('a' <= this->buffer[1] && this->buffer[1] <= 'z')
				) {
			this->code = this->buffer[1];
			this->name = this->code;
			if (this->codenames.count(this->name))
				this->name = ANSICode::codenames.at(this->name);
			return PARSE_COMPLETE;
		}
		else if (this->buffer[1] == '[')
			return PARSE_INCOMPLETE; // Continuation.
		else {
			return PARSE_INVALID;
		}
	}
	else if (this->buffer.size() == 3 && this->buffer[1] == 'O') {
		// Handle that annoying special case, of the two letter O_ code.
		if ('P' <= this->buffer[2] && this->buffer[2] <= 'S') {
			this->code = this->buffer.substr(1,2);
			this->name = this->code;
			if (this->codenames.count(this->name))
				this->name = ANSICode::codenames.at(this->name);
			return PARSE_COMPLETE;
		}
		else
			return PARSE_INVALID; // Turns out it wasn't valid after all.
	}

	std::string param_buf;
	for (auto it = this->buffer.begin()+2, eit = this->buffer.end(); it != eit; ++it) {
		if ('0' <= *it && *it <= '9') {
			param_buf += *it;
		}
		else if (*it == ';') {
			if (param_buf.empty())
				this->parameters.push_back(0);
			else {
				init_stdlib_mutex();
				xSemaphoreTake(stdlib_mutex, portMAX_DELAY);
				this->parameters.push_back(atoi(param_buf.c_str()));
				xSemaphoreGive(stdlib_mutex);
				param_buf.clear();
			}
		}
		else if ( ('A' <= *it && *it <= 'Z') || ('a' <= *it && *it <= 'z') || (*it == '~') ) {
			if (!param_buf.empty()) {
				init_stdlib_mutex();
				xSemaphoreTake(stdlib_mutex, portMAX_DELAY);
				this->parameters.push_back(atoi(param_buf.c_str()));
				xSemaphoreGive(stdlib_mutex);
				param_buf.clear();
			}

			if (*it == '~') {
				/* These are things like INS, DEL, HOME, END, PGUP, PGDN.
				 *
				 * They vary by the parameter number, so, for tilde, the last
				 * parameter is part of the command, and it's invalid without
				 * one.
				 */
				if (this->parameters.size() != 1)
					return PARSE_INVALID;
				this->code = stdsprintf("[%d~", this->parameters[0]);
				this->parameters.pop_back();
				this->name = this->code;
				if (this->codenames.count(this->name))
					this->name = ANSICode::codenames.at(this->name);
				if (it+1 != this->buffer.end())
					return PARSE_INVALID; // The code ended before the buffer did.
				return PARSE_COMPLETE;
			}
			else {
				this->code = std::string("[") + *it; // Don't forget this is an extended code.
				this->name = this->code;
				if (this->codenames.count(this->name))
					this->name = ANSICode::codenames.at(this->name);
				if (it+1 != this->buffer.end())
					return PARSE_INVALID; // The code ended before the buffer did.
				return PARSE_COMPLETE;
			}
		}
		else
			return PARSE_INVALID; // Duno what we got, but it's not something we want.
	}
	return PARSE_INCOMPLETE; // We got to the end without bailing as invalid, but we didn't complete either.
}

const std::map<std::string, std::string> ANSICode::codenames {
	{"D", "SCROLL_DOWN"},
	{"M", "SCROLL_UP"},
	{"OP", "F1"},
	{"OQ", "F2"},
	{"OR", "F3"},
	{"OS", "F4"},
	{"[15~", "F5"},
	{"[17~", "F6"},
	{"[18~", "F7"},
	{"[19~", "F8"},
	{"[20~", "F9"},
	{"[21~", "F10"},
	{"[23~", "F11"},
	{"[24~", "F12"},
	{"[A", "UP_ARROW"},
	{"[B", "DOWN_ARROW"},
	{"[C", "RIGHT_ARROW"},
	{"[D", "LEFT_ARROW"},
	{"[1~", "HOME"},
	{"[2~", "INSERT"},
	{"[3~", "DELETE"},
	{"[4~", "END"},
	{"[5~", "PAGE_UP"},
	{"[6~", "PAGE_DOWN"},
};

const std::string ANSICode::ANSI_ERASE_TO_END_OF_LINE = "\x1B[K";
const std::string ANSICode::ANSI_ERASE_TO_START_OF_LINE = "\x1B[1K";
const std::string ANSICode::ANSI_ERASE_LINE = "\x1B[2K";
