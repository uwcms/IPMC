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

#ifndef SRC_COMMON_ZYNQIPMC_DRIVERS_NETWORK_CLIENTSOCKET_H_
#define SRC_COMMON_ZYNQIPMC_DRIVERS_NETWORK_CLIENTSOCKET_H_

#include "socket.h"

/**
 * A client socket implementation practical to establish a connection to a server.
 *
 * When connected, recv/send functions from Socket became usable.
 */
class ClientSocket : public Socket {
public:
	/**
	 * Create a new socket by proving an address or a port.
	 * @param address The target address.
	 * @param port The desired port.
	 * @throws except::host_not_found if DNS fails for an address that is an URL.
	 */
	ClientSocket(const std::string& address, unsigned short port);
	virtual ~ClientSocket() {};

	/**
	 * Connect to the target server.
	 * @return -1 if failed to connect.
	 */
	int connect();
};

#endif /* SRC_COMMON_ZYNQIPMC_DRIVERS_NETWORK_CLIENTSOCKET_H_ */
