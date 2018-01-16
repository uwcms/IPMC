/*
 * IPMICommandParser.h
 *
 *  Created on: Jan 9, 2018
 *      Author: jtikalsky
 */

#ifndef SRC_COMMON_UW_IPMC_SERVICES_IPMI_IPMBSVC_IPMICOMMANDPARSER_H_
#define SRC_COMMON_UW_IPMC_SERVICES_IPMI_IPMBSVC_IPMICOMMANDPARSER_H_

#include <FreeRTOS.h>
#include <semphr.h>
#include <services/ipmi/IPMI_MSG.h>
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
	 * \param ipmb The IPMB this command came in through
	 * \param message The IPMI_MSG received
	 */
	typedef std::function<void(IPMBSvc &ipmb, const IPMI_MSG &message)> ipmi_cmd_handler_t;

	IPMICommandParser(ipmi_cmd_handler_t default_handler = NULL);
	virtual ~IPMICommandParser();

	ipmi_cmd_handler_t default_handler; ///< The handler to call if the command is unknown.

	void register_handler(uint16_t command, ipmi_cmd_handler_t handler);

	void dispatch(IPMBSvc &ipmb, const IPMI_MSG &message);

protected:
	SemaphoreHandle_t mutex; ///< A mutex protecting the mapping.
	std::map<uint16_t, ipmi_cmd_handler_t> commandset; ///< The command mapping
};

#endif /* SRC_COMMON_UW_IPMC_SERVICES_IPMI_IPMBSVC_IPMICOMMANDPARSER_H_ */
