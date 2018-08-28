/*
 * ESM.cpp
 *
 *  Created on: Aug 16, 2018
 *      Author: mpv
 */

/** In case this becomes necessary, this is the best way to disable ESM but leave the flash active:
 *
 * 	this->flash_reset->deassert();
 *  this->esm_reset->assert();
 *  vTaskDelay(pdMS_TO_TICKS(100));
 *
 *  And this is the best way to go back:
 *
 *  vTaskDelay(pdMS_TO_TICKS(200));
 *  this->esm_reset->release();
 *  vTaskDelay(pdMS_TO_TICKS(500));
 *  this->flash_reset->release();
 **/

#include "ESM.h"
#include <libs/Utils.h>
#include <services/ftp/FTPServer.h>

const std::string ESM::commandStatusToString(const CommandStatus& s) {
	switch (s) {
	case ESM_CMD_NOCOMMAND: return "No command to send";
	case ESM_CMD_NORESPONSE: return "No response";
	case ESM_CMD_OVERFLOW: return "Abnormal number of characters received";
	default: return "Success";
	}
}

ESM::ESM(UART *uart, ResetPin *esm_reset, Flash *flash, ResetPin *flash_reset)
: uart(uart), esm_reset(esm_reset), flash(flash), flash_reset(flash_reset) {
	configASSERT(this->uart);

	this->mutex = xSemaphoreCreateMutex();
	configASSERT(this->mutex);
}

ESM::~ESM() {
	vSemaphoreDelete(this->mutex);
}

FTPFile ESM::createFlashFile() {
	if (!this->flash) {
		return FTPFile(nullptr, nullptr, 0);
	}

	return FTPFile(
		[this](uint8_t *buffer, size_t size) -> size_t {
			// Read
			MutexLock(this->mutex);
			this->flash->initialize();
			this->flash->read(0, buffer, size);
			return size;
		},
		[this](uint8_t *buffer, size_t size) -> size_t {
			MutexLock(this->mutex);
			this->flash->initialize();

			// Write
			if (!this->flash->write(0, buffer, size))
				return 0; // Failed to write

			// Verify
			std::unique_ptr<uint8_t> tmpbuf(new uint8_t[size]);
			if (!tmpbuf.get()) return 0; // No memory

			this->flash->read(0, tmpbuf.get(), size);

			for (size_t i = 0; i < size; i++) {
				if (tmpbuf.get()[i] != buffer[i]) {
					printf("Byte 0x%08x different.", i);
					return i; // Return total number of verified bytes
				}
			}

			// Write successful
			return size;
		}, 256 * 1024);
}

ESM::CommandStatus ESM::command(const std::string& command, std::string& response) {
	// Check if there is a command to send
	if (command == "") {
		return ESM_CMD_NOCOMMAND;
	}

	// Terminate with '/r' to trigger ESM to respond
	std::string formated_cmd = command + "\r";

	MutexLock(this->mutex);

	// Clear the receiver buffer
	this->uart->clear();

	// Send the command
	this->uart->write((const u8*)formated_cmd.c_str(), formated_cmd.length(), pdMS_TO_TICKS(1000));

	// Read the incoming response
	// A single read such as:
	// size_t count = uart->read((u8*)buf, 2048, pdMS_TO_TICKS(1000));
	// .. works but reading one character at a time allows us to detect
	// the end of the response which is '\r\n>'.
	char inbuf[2048] = "";
	size_t pos = 0, count = 0;
	while (pos < 2043) {
		count = this->uart->read((u8*)inbuf+pos, 1, pdMS_TO_TICKS(1000));
		if (count == 0) break; // No character received
		if ((pos > 3) && (memcmp(inbuf+pos-2, "\r\n>", 3) == 0)) break;
		pos++;
	}

	if (pos == 0) {
		return ESM_CMD_NORESPONSE;
	} else if (pos == 2043) {
		return ESM_CMD_OVERFLOW;
	} else {
		// ESM will send back the command written and a new line.
		// At the end it will return '\r\n>', we erase this.
		size_t start = command.length() + 1;
		size_t end = strlen(inbuf);

		// Force the end of buf to be before '\r\n>'
		if (end > 3) { // We don't want a data abort here..
			inbuf[end-3] = '\0';
		}

		response = std::string(inbuf + start);
	}

	return ESM_CMD_SUCCESS;
}

void ESM::restart() {
	if (this->esm_reset){
		MutexLock(this->mutex);
		esm_reset->toggle();
	} else {
		std::string resp;
		this->command("X", resp);
	}
	vTaskDelay(pdMS_TO_TICKS(1000));
}

/// A "esm.command" console command.
class esm_command : public CommandParser::Command {
public:
	ESM &esm; ///< ESM object.

	///! Construct the command.
	esm_command(ESM &esm) : esm(esm) { };

	virtual std::string get_helptext(const std::string &command) const {
		return command + "\n\n"
				"Send a command to the ESM and see its output. Use ? to see possible commands.\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		// Prepare the command
		int argn = parameters.nargs();
		std::string command = "", response, p;
		for (int i = 1; i < argn; i++) {
			parameters.parse_parameters(i, true, &p);
			if (i == (argn-1)) {
				command += p;
			} else {
				command += p + " ";
			}
		}

		ESM::CommandStatus s = esm.command(command, response);

		if (s != ESM::ESM_CMD_SUCCESS) {
			console->write(ESM::commandStatusToString(s) + ".\n");
		} else {
			console->write(response);
		}
	}
};

/// A "esm.restart" console command.
class esm_restart : public CommandParser::Command {
public:
	ESM &esm; ///< ESM object.

	///! Construct the command.
	esm_restart(ESM &esm) : esm(esm) { };

	virtual std::string get_helptext(const std::string &command) const {
		return command + "\n\n"
				"Restart the ESM module. Network interface will go down while restart is in progress.\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		esm.restart();
	}
};

/// A "esm.flash.info" console command.
class esm_flash_info : public CommandParser::Command {
public:
	ESM &esm; ///< ESM object.

	///! Construct the command.
	esm_flash_info(ESM &esm) : esm(esm) { };

	virtual std::string get_helptext(const std::string &command) const {
		return command + "\n\n"
				"Show information about the ESM flash. Network will go down if it is the first time accessing the flash.\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		MutexLock(esm.mutex);

		if (!this->esm.flash->isInitialized()) {
			this->esm.flash->initialize();
		}

		console->write("Total flash size: " + bytesToString(esm.flash->getTotalSize()) + "\n");
	}
};

/**
 * Register console commands related to this storage.
 * @param parser The command parser to register to.
 * @param prefix A prefix for the registered commands.
 */
void ESM::register_console_commands(CommandParser &parser, const std::string &prefix) {
	parser.register_command(prefix + "command", std::make_shared<esm_command>(*this));
	parser.register_command(prefix + "restart", std::make_shared<esm_restart>(*this));
	if (this->isFlashPresent())
		parser.register_command(prefix + "flash.info", std::make_shared<esm_flash_info>(*this));
}

/**
 * Unregister console commands related to this storage.
 * @param parser The command parser to unregister from.
 * @param prefix A prefix for the registered commands.
 */
void ESM::deregister_console_commands(CommandParser &parser, const std::string &prefix) {
	parser.register_command(prefix + "command", NULL);
	parser.register_command(prefix + "restart", NULL);
	if (this->isFlashPresent())
		parser.register_command(prefix + "flash.info", NULL);
}
