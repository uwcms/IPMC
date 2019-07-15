/*
 * ESM.h
 *
 *  Created on: Aug 16, 2018
 *      Author: mpv
 */

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_ESM_ESM_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_ESM_ESM_H_

#include <drivers/generics/gpio.h>
#include <FreeRTOS.h>
#include <semphr.h>
#include <string>
#include <libs/VFS.h>
#include <drivers/generics/UART.h>
#include <drivers/spi_flash/spi_flash.h>
#include <services/console/CommandParser.h>
#include <services/console/ConsoleSvc.h>

/**
 * ESM driver that implements the software layers for ESM management, including programming and monitoring.
 */
class ESM {
public:
	/**
	 * Constructs a new ESM driver instance
	 * @param uart UART interface used to communicate with the ESM.
	 * @param esm_reset ESM reset pin (from a GPIO for example). Can be assigned NULL.
	 * @param flash Flash interface used to upgrade ESM firmware if necessary.
	 * @param flash_reset Flash reset. Presently not being used, can be assigned to NULL.
	 * @note If esm_reset is NULL then ESM reset will be issued via UART.
	 * @warning If flash isn't assigned then programming features won't be available.
	 */
	ESM(UART *uart, ResetPin *esm_reset = NULL, Flash *flash = NULL, ResetPin *flash_reset = NULL);
	virtual ~ESM();

	///! Possible returns from ESM::command.
	typedef enum {
		ESM_CMD_SUCCESS,
		ESM_CMD_NOCOMMAND,
		ESM_CMD_NORESPONSE,
		ESM_CMD_OVERFLOW,
	} CommandStatus;

	/**
	 * Convert a command status to a readable string.
	 * @param s The command status returned from ESM::command.
	 * @return The human readable string.
	 */
	static const std::string commandStatusToString(const CommandStatus& s);

	/**
	 * Sends a command to the ESM and fills out a response, if any.
	 * @param command A string that makes the command.
	 * @param response String to be filled with response.
	 * @return The command status, ESM::commandStatusToString can be used to read the status.
	 * @note "?" can be sent do the ESM to obtain the list of commands available.
	 */
	CommandStatus command(const std::string& command, std::string& response);

	///! Restart the ESM. Network will go down while restart takes place.
	void restart();

	///! Returns true if the flash interface is present, false otherwise.
	inline bool isFlashPresent() { return (this->flash)? true : false; };

	/**
	 * Generates an VFS file linked to the ESM flash that can be added to the
	 * virtual file system, allowing flash programming via ethernet or console.
	 * @return The file that can be used with VFS::addFile.
	 */
	VFS::File createFlashFile();

	bool getTemperature(float &temperature);

	void register_console_commands(CommandParser &parser, const std::string &prefix="");
	void deregister_console_commands(CommandParser &parser, const std::string &prefix="");

private:
	// Console commands
	friend class esm_command;
	friend class esm_restart;
	friend class esm_flash_info;

private:
	UART *uart;
	ResetPin *esm_reset;
	Flash *flash;
	ResetPin *flash_reset;
	SemaphoreHandle_t mutex;
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_ESM_ESM_H_ */
