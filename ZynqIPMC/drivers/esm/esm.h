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

#ifndef SRC_COMMON_ZYNQIPMC_DRIVERS_ESM_ESM_H_
#define SRC_COMMON_ZYNQIPMC_DRIVERS_ESM_ESM_H_

#include <FreeRTOS.h>
#include <semphr.h>
#include <string>
#include <drivers/generics/uart.h>
#include <drivers/generics/gpio.h>
#include <drivers/spi_flash/spi_flash.h>
#include <libs/vfs/vfs.h>
#include <services/console/command_parser.h>
/**
 * ESM driver that implements the software layers for ESM management, including programming and monitoring.
 *
 * The driver uses 4 different interfaces that span the entire connectivity scheme but they can be set
 * as nullptr if not being used in the application.
 *
 */
class ESM final : public ConsoleCommandSupport {
public:
	/**
	 * Constructs a new ESM driver instance
	 * @param uart UART interface used to communicate with the ESM.
	 * @param esm_reset ESM reset pin (from a GPIO for example). Can be assigned to nullptr if not used.
	 * @param flash Flash interface used to upgrade ESM firmware. Can be assigned to nullptr if not used.
	 * @param flash_reset Flash reset. Presently not being used, should be assigned to nullptr.
	 * @note If esm_reset is nullptr then ESM reset will be issued via UART instead.
	 * @warning If flash isn't assigned then programming features won't be available.
	 */
	ESM(UART &uart, ResetPin *esm_reset = nullptr, Flash *flash = nullptr, ResetPin *flash_reset = nullptr);
	virtual ~ESM();

	//! Possible returns from ESM::command.
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
	 * @param command String that makes the command.
	 * @param response String to be filled with response.
	 * @return The command status, ESM::commandStatusToString can be used to read the status.
	 * @note "?" can be sent do the ESM to obtain the list of commands available.
	 */
	CommandStatus command(const std::string& command, std::string& response);

	//! Restart the ESM. Network will go down while restart takes place.
	void restart();

	//! Returns true if the flash interface is present, false otherwise.
	inline bool isFlashPresent() const { return (this->flash)? true : false; };

	/**
	 * Generates an VFS file linked to the ESM flash that can be added to the
	 * virtual file system, allowing flash programming via ethernet or console.
	 * @return The file that can be used with VFS::addFile.
	 */
	VFS::File createFlashFile();

	/**
	 * Returns the internal ESM temperature in Celcius.
	 * @param temperature Reference float where to store the temperature.
	 * @return true if successful, false otherwise.
	 */
	bool getTemperature(float &temperature);

	// From base class ConsoleCommandSupport;
	virtual void registerConsoleCommands(CommandParser &parser, const std::string &prefix="");
	virtual void deregisterConsoleCommands(CommandParser &parser, const std::string &prefix="");

private:
	// Console commands
	class Command;		///< ESM remote management command.
	class Restart;		///< Restart command.
	class FlashInfo;	///< Flash info command.

	UART &uart;					///< UART interface to talk to the ESM.
	ResetPin *esm_reset;		///< ESM reset pin, if available.
	Flash *flash;				///< ESM flash interface, if available.
	ResetPin *flash_reset;		///< ESM flash reset pin, if available.
	SemaphoreHandle_t mutex;	///< Mutex for thread safe operation.
};

#endif /* SRC_COMMON_ZYNQIPMC_DRIVERS_ESM_ESM_H_ */
