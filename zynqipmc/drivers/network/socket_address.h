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

#ifndef SRC_COMMON_ZYNQIPMC_DRIVERS_NETWORK_SOCKET_ADDRESS_H_
#define SRC_COMMON_ZYNQIPMC_DRIVERS_NETWORK_SOCKET_ADDRESS_H_

#include <string>
#include <lwip/inet.h>
#include <lwip/sockets.h>
#include <libs/except.h>

DEFINE_GENERIC_EXCEPTION(host_not_found, std::runtime_error)

/**
 * Defines an IP pair which includes an address and a port.
 *
 * Dynamic addressing is supported by providing the target URL.
 */
class SocketAddress {

protected:
	struct sockaddr_in sockaddr;

public:
	/**
	 * Creates a new socketaddress instance based on a sockaddr_in structure.
	 * @param sockaddr The sockaddr_in structure.
	 */
	SocketAddress(struct sockaddr_in sockaddr) : sockaddr(sockaddr) {}

	/**
	 * Creates a new socketaddress instance with a specified address and port
	 * @param address The address of the socket.
	 * @param port Target port.
	 * @throws except::host_not_found if didn't manage to get host.
	 */
	SocketAddress(const std::string& address, unsigned short port);

	//! Gets the port number.
	inline const int getPort() const { return ntohs(this->sockaddr.sin_port); }

	//! Gets the address.
	const std::string getAddress() const;

	//! Gets the address in binary.
	inline const uint32_t getAddressBinary() const { return this->sockaddr.sin_addr.s_addr; }

	//! Operator overload for struct sockaddr_in assignment, returns sockaddr
	inline operator struct sockaddr_in() { return this->sockaddr; }

	//! Operator overload for struct sockaddr* assignment, returns sockaddr*
	inline operator struct sockaddr*() { return (struct sockaddr*)&(this->sockaddr); }
};

#endif /* SRC_COMMON_ZYNQIPMC_DRIVERS_NETWORK_SOCKET_ADDRESS_H_ */
