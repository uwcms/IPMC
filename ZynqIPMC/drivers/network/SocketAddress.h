/*
 * SocketAddress.h
 *
 *  Created on: Feb 8, 2018
 *      Author: mpv
 */

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_NETWORK_SOCKETADDRESS_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_NETWORK_SOCKETADDRESS_H_

#include <string>
#include <lwip/inet.h>
#include <lwip/sockets.h>

/**
 * Defines a IP address with IP and port.
 */
class SocketAddress {
public:
	struct HostNotFound : public std::exception {};

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
	 * @param port The port.
	 * @throws HostNotFound
	 */
	SocketAddress(const std::string& address, unsigned short port);

	/**
	 * Gets the port of the socket
	 * @return the port number
	 */
	inline const int getPort() const { return ntohs(this->sockaddr.sin_port); }

	/**
	 * Gets the address of the socket
	 * @return the address
	 */
	const std::string getAddress() const;

	/**
	 * Gets the address of the socket
	 * @return the address
	 */
	inline const uint32_t getAddressBinary() const { return this->sockaddr.sin_addr.s_addr; }

	///! Operator overload for struct sockaddr_in assignment, returns sockaddr
	inline operator struct sockaddr_in() { return this->sockaddr; }

	///! Operator overload for struct sockaddr* assignment, returns sockaddr*
	inline operator struct sockaddr*() { return (struct sockaddr*)&(this->sockaddr); }
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_NETWORK_SOCKETADDRESS_H_ */
