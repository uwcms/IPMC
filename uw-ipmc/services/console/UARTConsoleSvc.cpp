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

static void run_uartconsolesvc_thread(void *cb_ucs) {
	reinterpret_cast<UARTConsoleSvc*>(cb_ucs)->_run_thread();
}

UARTConsoleSvc::UARTConsoleSvc(UART &uart, CommandParser &parser, const std::string &name, LogTree &logtree, bool echo, TickType_t read_data_timeout)
	: uart(uart), parser(parser), name(name), logtree(logtree),
	  log_input(logtree["input"]), log_output(logtree["output"]),
	  echo(echo), read_data_timeout(read_data_timeout) {
	configASSERT(xTaskCreate(run_uartconsolesvc_thread, name.c_str(), configMINIMAL_STACK_SIZE+256, this, TASK_PRIORITY_SERVICE, NULL));
}

UARTConsoleSvc::~UARTConsoleSvc() {
	// Nothing to do.
}

void UARTConsoleSvc::_run_thread() {
	this->logtree.log("Starting UART Console Service \"" + this->name + "\"", LogTree::LOG_INFO);
	const std::string ctrlc_erased_facility = this->logtree.path + ".ctrlc_erased";
	const std::string prompt = "> ";
	std::function<void(std::string)> console_output = [this](std::string data) -> void {
		this->log_output.log(data, LogTree::LOG_DIAGNOSTIC);
		windows_newline(data);
		this->uart.write(data.data(), data.size(), portMAX_DELAY);
	};
	std::string linebuf;
	while (true) {
		char readbuf[128];
		size_t bytes_read = this->uart.read(readbuf, 128, portMAX_DELAY, this->read_data_timeout);
		std::string rawbuffer(readbuf, bytes_read);

		std::string::size_type rst_before = rawbuffer.find_last_of("\x03\x04\x12");
		if (rst_before != std::string::npos) {
			TRACE.log(ctrlc_erased_facility.c_str(), ctrlc_erased_facility.size(), LogTree::LOG_TRACE, rawbuffer.substr(0, rst_before+1).data(), rst_before+1, true);
			rawbuffer.replace(0, rst_before+1, "\r"); // Clear all buffer found before any Ctrl-C (or D, or R), and replace it with an empty line to retrigger the prompt.
		}

		std::string echobuf;
		for (auto it = rawbuffer.begin(), eit = rawbuffer.end(); it != eit; ++it) {
			if (*it == '\r') {
				// Newlines are received as \r, sent as \r\n.
				echobuf.append("\r\n");

				// Flush echo buffer.
				if (this->echo)
					this->uart.write(echobuf.data(), echobuf.size(), portMAX_DELAY);
				echobuf.clear();

				// Parse & run the command line.
				if (!linebuf.empty()) {
					log_input.log(linebuf, LogTree::LOG_INFO);
					bool command_known = this->parser.parse(console_output, linebuf);
					if (!command_known)
						console_output("Unknown command!\n");
				}

				// Ready next commandline.
				linebuf.clear();
				this->uart.write(prompt.data(), prompt.size(), portMAX_DELAY);
			}
			else if (*it == '\n') {
				// Ignore it.  We don't understand that so, if you sent us \r\n, we'll just trigger on the \r.
			}
			else if (*it == '\x7f') {
				// DEL (sent by backspace key)
				if (!linebuf.empty()) {
					linebuf.pop_back();
					echobuf += "\x08 \x08"; // Move back one space, overwrite with whitespace, move back one space again.
				}
			}
			else {
				echobuf += *it;
				linebuf += *it;
			}
		}

		// Flush echo buffer.
		if (this->echo)
			this->uart.write(echobuf.data(), echobuf.size(), portMAX_DELAY);
		echobuf.clear();
	}
}
