/*
 * TelnetConsole.h
 *
 *  Created on: Feb 9, 2018
 *      Author: jtikalsky
 */

#ifndef SRC_COMMON_UW_IPMC_SERVICES_CONSOLE_TELNETCONSOLESVC_H_
#define SRC_COMMON_UW_IPMC_SERVICES_CONSOLE_TELNETCONSOLESVC_H_

#include "ConsoleSvc.h"
#include <drivers/network/Socket.h>

namespace Telnet {

/**
 * A Telnet protocol parser to handle incoming telnet input.
 */
class InputProtocolParser {
public:
	InputProtocolParser();
	bool incompatible_client; ///< Set to true if we receive incompatible negotiations
	std::string remote_terminal_type; ///< The remote terminal type, if offered.
	std::string parse_input(char *buf, size_t *len)  __attribute__((nonnull(1,2)));
	std::string build_initial_negotiation();

protected:
	uint64_t last_keepalive; ///< A marker for last keepalive.
	std::string code_buffer; ///< A buffer for storing incomplete codes.
	std::string sb_buffer; ///< A secondary buffer to deliver data to in subnegotiations.

	std::string negotiate(uint8_t req, uint8_t feature, std::string subnegotiation = "");
};

/**
 * A telnet based console service.
 */
class TelnetConsoleSvc : public ConsoleSvc {
public:
	/**
	 * Factory function.  All parameters match the constructor.
	 *
	 * \note Since the parameters are specific, this function cannot be virtual.
	 */
	static std::shared_ptr<TelnetConsoleSvc> create(std::shared_ptr<Socket> socket, std::shared_ptr<InputProtocolParser> proto, CommandParser &parser, const std::string &name, LogTree &logtree, bool echo, TickType_t read_data_timeout=4, std::function<void(TelnetConsoleSvc&)> shutdown_complete_cb=NULL) {
		std::shared_ptr<TelnetConsoleSvc> ret(new TelnetConsoleSvc(socket, proto, parser, name, logtree, echo, read_data_timeout, shutdown_complete_cb));
		ret->weakself = ret;
		return ret;
	}
protected:
	TelnetConsoleSvc(std::shared_ptr<Socket> socket, std::shared_ptr<InputProtocolParser> proto, CommandParser &parser, const std::string &name, LogTree &logtree, bool echo, TickType_t read_data_timeout=4, std::function<void(TelnetConsoleSvc&)> shutdown_complete_cb=NULL);
public:
	virtual ~TelnetConsoleSvc();

	std::shared_ptr<Socket> socket; ///< The socket this console is driven by.
	std::shared_ptr<InputProtocolParser> proto; ///< The input protocol parser for this socket.
	SemaphoreHandle_t sock_mutex; ///< A mutex protecting the socket.

	virtual void close();
protected:
	std::function<void(TelnetConsoleSvc&)> shutdown_complete_cb;
	virtual void shutdown_complete();
	virtual ssize_t raw_read(char *buf, size_t len, TickType_t timeout, TickType_t read_data_timeout);
	virtual ssize_t raw_write(const char *buf, size_t len, TickType_t timeout);
};

/// Telnet Command Codes
///@{
static const char CODE_SE       = 240;
static const char CODE_NOP      = 241;
static const char CODE_DataMark = 242;
static const char CODE_Break    = 243;
static const char CODE_GA       = 249;
static const char CODE_SB       = 250;
static const char CODE_WILL     = 251;
static const char CODE_WONT     = 252;
static const char CODE_DO       = 253;
static const char CODE_DONT     = 254;
static const char CODE_IAC      = 255;
///@}

/// Telnet Feature Codes
///@{
static const char FEATURE_Binary_Transmission         = 0;
static const char FEATURE_Echo                        = 1;
static const char FEATURE_Suppress_Go_Ahead           = 3;
static const char FEATURE_CR_Use                      = 10;
static const char FEATURE_HorizTab_Use                = 12;
static const char FEATURE_FF_Use                      = 13;
static const char FEATURE_VertTab_Use                 = 15;
static const char FEATURE_Logout                      = 18; // Force Logout
static const char FEATURE_Terminal_Type               = 24;
static const char FEATURE_Negotiate_About_Window_Size = 31;
static const char FEATURE_Remote_Flow_Control         = 33;
///@}

} // namespace Telnet

#endif /* SRC_COMMON_UW_IPMC_SERVICES_CONSOLE_TELNETCONSOLESVC_H_ */
