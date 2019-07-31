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

#ifndef SRC_COMMON_ZYNQIPMC_SERVICES_CONSOLE_TELNETCONSOLESVC_H_
#define SRC_COMMON_ZYNQIPMC_SERVICES_CONSOLE_TELNETCONSOLESVC_H_

#include <drivers/network/socket.h>
#include <services/console/consolesvc.h>

namespace Telnet {

/**
 * A Telnet protocol parser to handle incoming telnet input.
 */
class InputProtocolParser {
public:
	InputProtocolParser();
	bool incompatible_client; ///< Set to true if we receive incompatible negotiations
	std::string remote_terminal_type; ///< The remote terminal type, if offered.

	/**
	 * Parse telnet protocol input, prepare responses, and strip the supplied input buffer.
	 * @param buf The input buffer to process and strip codes from.
	 * @param[in,out] len A pointer to the length of the input buffer.  Updated as codes are stripped.
	 * @return Protocol response data to return to the client verbatim.
	 */
	std::string parseInput(char *buf, size_t *len)  __attribute__((nonnull(1,2)));

	/**
	 * Build an initial negotiation string for ourselves.
	 *
	 * @return The initial negotiation string.
	 */
	std::string buildInitialNegotiation();

	//! Telnet Command Codes
	enum Code : char {
		CODE_SE       = 240,
		CODE_NOP      = 241,
		CODE_DATAMARK = 242,
		CODE_BREAK    = 243,
		CODE_GA       = 249,
		CODE_SB       = 250,
		CODE_WILL     = 251,
		CODE_WONT     = 252,
		CODE_DO       = 253,
		CODE_DONT     = 254,
		CODE_IAC      = 255,
	};

	//! Telnet Feature Codes
	enum Feature : char {
		FEATURE_BINARY_TRANSMISSION         = 0,
		FEATURE_ECHO                        = 1,
		FEATURE_SUPPRESS_GO_AHEAD           = 3,
		FEATURE_CR_USE                      = 10,
		FEATURE_HORIZTAB_USE                = 12,
		FEATURE_FF_USE                      = 13,
		FEATURE_VERTTAB_USE                 = 15,
		FEATURE_LOGOUT                      = 18, // Force Logout
		FEATURE_TERMINAL_TYPE               = 24,
		FEATURE_NEGOTIATE_ABOUT_WINDOW_SIZE = 31,
		FEATURE_REMOTE_FLOW_CONTROL         = 33,
	};

protected:
	uint64_t last_keepalive;	///< A marker for last keepalive.
	std::string code_buffer;	///< A buffer for storing incomplete codes.
	std::string sb_buffer;		///< A secondary buffer to deliver data to in subnegotiations.

	std::string negotiate(uint8_t req, uint8_t feature, std::string subnegotiation = "");
};

/**
 * A telnet based console service.
 */
class TelnetConsoleSvc final : public ConsoleSvc {
public:
	/**
	 * Factory function.  All parameters match the constructor.
	 *
	 * @note Since the parameters are specific, this function cannot be virtual.
	 */
	static std::shared_ptr<TelnetConsoleSvc> create(std::shared_ptr<Socket> socket, std::shared_ptr<InputProtocolParser> proto, CommandParser &parser, const std::string &name, LogTree &logtree, bool echo, TickType_t read_data_timeout=4, std::function<void(TelnetConsoleSvc&)> shutdownComplete_cb=nullptr) {
		std::shared_ptr<TelnetConsoleSvc> ret(new TelnetConsoleSvc(socket, proto, parser, name, logtree, echo, read_data_timeout, shutdownComplete_cb));
		ret->weakself = ret;
		return ret;
	}

	virtual ~TelnetConsoleSvc();

	/**
	 * Close and delete the underlying socket.
	 *
	 * @warning This will trigger a "delete this".
	 * @param wait If true, block until shutdown is complete
	 */
	virtual void close();

private:
	/**
	 * Instantiate a Telnet console service.
	 *
	 * @note This will take ownership of the socket, and of itself, and delete
	 *       both upon termination.
	 *
	 * @param socket The socket
	 * @param proto The protocol parser associated with this socket.
	 * @param parser The command parser to use.
	 * @param name The name of the service for the process and such things.
	 * @param logtree The log tree root for this service.
	 * @param echo If true, enable echo and interactive management.
	 * @param read_data_timeout The timeout for reads when data is available.
	 * @param shutdownComplete_cb A function to call to notify external resources about a completed shutdown.
	 */
	TelnetConsoleSvc(std::shared_ptr<Socket> socket, std::shared_ptr<InputProtocolParser> proto, CommandParser &parser, const std::string &name, LogTree &logtree, bool echo, TickType_t read_data_timeout=4, std::function<void(TelnetConsoleSvc&)> shutdownComplete_cb=nullptr);

	std::shared_ptr<Socket> socket; ///< The socket this console is driven by.
	std::shared_ptr<InputProtocolParser> proto; ///< The input protocol parser for this socket.
	SemaphoreHandle_t sock_mutex; ///< A mutex protecting the socket.

	std::function<void(TelnetConsoleSvc&)> shutdownComplete_cb;
	virtual void shutdownComplete();
	virtual ssize_t rawRead(char *buf, size_t len, TickType_t timeout, TickType_t read_data_timeout);
	virtual ssize_t rawWrite(const char *buf, size_t len, TickType_t timeout);
};

} // namespace Telnet

#endif /* SRC_COMMON_ZYNQIPMC_SERVICES_CONSOLE_TELNETCONSOLESVC_H_ */
