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

#ifndef SRC_COMMON_ZYNQIPMC_SERVICES_TELNET_TELNET_H_
#define SRC_COMMON_ZYNQIPMC_SERVICES_TELNET_TELNET_H_

#include <drivers/network/server_socket.h>
#include <libs/logtree/logtree.h>

/**
 * Instantiate and start the Telnet server which allows console access over the network.
 */
class TelnetServer final {
public:
	/**
	 * Start the server.
	 * @param logtree Log facility for debugging and status.
	 */
	TelnetServer(LogTree &logtree);
	~TelnetServer();

private:
	LogTree &logtree;							///< Log facility.
	SemaphoreHandle_t connection_pool_limiter;	///< A counting semaphore to limit the simultaneous connections to a sensible amount.

	//! Server background thread.
	void threadTelnetd();
};

/**
 * When the server receives a new connection a new client thread is launched through this class.
 */
class TelnetClient final {
public:
	/**
	 * Detaches the client socket from the server and launches the client console.
	 * @param sock Valid client and active socket.
	 * @param logtree Log facility for debugging and monitoring.
	 * @param connection_pool_limiter The limiter indicating how many slots are available.
	 */
	TelnetClient(std::shared_ptr<Socket> sock, LogTree &logtree, SemaphoreHandle_t connection_pool_limiter);

private:
	class LogoutCommand; ///< Special command to logout from Telnet.

	std::shared_ptr<Socket> socket;					///< The active socket.
	LogTree &logtree;								///< Log facility.
	static uint32_t next_session_serial;			///< The serial number of the next session.
	uint32_t session_serial;						///< This session serial number;
	SemaphoreHandle_t connection_pool_limiter;		///< A counting semaphore to limit the simultaneous connections to a sensible amount.
	static volatile uint64_t bad_password_pressure;	///< A timestamp for bad password lockouts.

	//! Get the current bad password timeout delay.
	static uint64_t getBadpasswordTimeout();

	//! Increment the current bad password timeout delay.
	static void increaseBadpasswordTimeout();

	//! Client background thread.
	void threadTelnetc();
};

#endif /* SRC_COMMON_ZYNQIPMC_SERVICES_TELNET_TELNET_H_ */
