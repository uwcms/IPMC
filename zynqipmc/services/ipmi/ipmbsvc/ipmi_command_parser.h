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

#ifndef SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_IPMBSVC_IPMI_COMMAND_PARSER_H_
#define SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_IPMBSVC_IPMI_COMMAND_PARSER_H_

#include <FreeRTOS.h>
#include <semphr.h>
#include <services/ipmi/ipmi_message.h>
#include <functional>
#include <map>

class IPMBSvc;

/**
 * An IPMI Command Parser.  This is an index of registered IPMI commands, and
 * handles dispatch of received commands.
 */
class IPMICommandParser {
public:
	/**
	 * The supplied function will be called when a matching message is
	 * dispatched through this parser.
	 *
	 * @param ipmb The IPMB this command came in through.
	 * @param message The IPMIMessage received.
	 */
	typedef std::function<void(IPMBSvc &ipmb, const IPMIMessage &message)> ipmi_cmd_handler_t;

	/**
	 * Instantiate an IPMI command parser.
	 *
	 * @param default_handler Called when an unknown command is received. (or nullptr)
	 * @param commandset A mapping to use to populate the initial IPMI command set.
	 */
	IPMICommandParser(ipmi_cmd_handler_t default_handler = nullptr, const std::map<uint16_t, ipmi_cmd_handler_t> commandset = std::map<uint16_t, ipmi_cmd_handler_t>());
	virtual ~IPMICommandParser();

	/**
	 * Sets the handler for a given IPMI command, or nullptr to delete it.
	 *
	 * @param command The command as MSB=NetFn, LSB=Cmd.
	 * @param handler The handler for the command.
	 */
	void registerHandler(uint16_t command, ipmi_cmd_handler_t handler);

	/**
	 * Parse an IPMI command and dispatch it to the appropriate handler.
	 *
	 * @param ipmb The IPMB this command came in through.
	 * @param message The IPMIMessage received.
	 */
	void dispatch(IPMBSvc &ipmb, const IPMIMessage &message);

protected:
	SemaphoreHandle_t mutex; ///< A mutex protecting the mapping.
	ipmi_cmd_handler_t default_handler; ///< The handler to call if the command is unknown.
	std::map<uint16_t, ipmi_cmd_handler_t> commandset; ///< The command mapping
};

#endif /* SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_IPMBSVC_IPMI_COMMAND_PARSER_H_ */
