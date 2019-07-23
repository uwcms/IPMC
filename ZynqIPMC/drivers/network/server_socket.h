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

#ifndef SRC_COMMON_ZYNQIPMC_DRIVERS_NETWORK_SERVER_SOCKET_H_
#define SRC_COMMON_ZYNQIPMC_DRIVERS_NETWORK_SERVER_SOCKET_H_

#include "socket.h"
#include <memory>

/**
 * Extends Socket and provides an interface to bind and listen to ports.
 *
 * When connected, recv/send functions from Socket became usable.
 */
class ServerSocket : public Socket {
public:
	/**
	 * Constructs a listening socket on the specified port and address
	 * The default address is 0.0.0.0 and the default backlog is 3.
	 * @param address Address to bind to.
	 * @param port Port to listen to.
	 * @param backlog Backlog queue size.
	 */
	ServerSocket(unsigned short port, size_t backlog = 3, const std::string& address = "0.0.0.0");
	virtual ~ServerSocket();

	/**
	 * Creates the listening socket and binds to the current port and address.
	 * @return negative error code if there was a problem.
	 */
	int listen();

	/**
	 * Reuse the port, should be called before ServerSocket::listen.
	 * @return negative error code if there was a problem.
	 */
	int reuse();

	//int bind();

	/**
	 * Accepts a new incoming client (blocking call).
	 * @return A client socket instance.
	 */
	std::shared_ptr<Socket> accept();

protected:
	const size_t kBacklog;	///< Backlog.
};

#endif /* SRC_COMMON_ZYNQIPMC_DRIVERS_NETWORK_SERVER_SOCKET_H_ */
