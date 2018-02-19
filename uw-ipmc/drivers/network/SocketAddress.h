/*
 * SocketAddress.h
 *
 *  Created on: Feb 8, 2018
 *      Author: mpv
 */

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_NETWORK_SOCKETADDRESS_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_NETWORK_SOCKETADDRESS_H_

#include <lwip/inet.h>
#include <lwip/sockets.h>
#include <string.h>
#include <string>

class SocketAddress {
protected:
	int port;
	std::string address;

public:
	/**
	 * Creates a new socketaddress instance based on a sockaddr_in structure
	 * @param the sockaddr_in structure
	 */
	SocketAddress(struct sockaddr_in addr) {
		port = addr.sin_port;

		address += std::to_string(ip4_addr1(&(addr.sin_addr))) + ".";
		address += std::to_string(ip4_addr2(&(addr.sin_addr))) + ".";
		address += std::to_string(ip4_addr3(&(addr.sin_addr))) + ".";
		address += std::to_string(ip4_addr4(&(addr.sin_addr)));
	}

	/**
	 * Creates a new socketaddress instance with a specified address and port
	 * @param the address of the socket
	 * @param the port
	 */
	SocketAddress(std::string address, int port) {
		this->address = address;
		this->port = port;
	}

	/**
	 * Returns a sockaddr_in structure based on the information of the socketaddress instance
	 * @return sockaddr_in structure
	 */
	struct sockaddr_in get_struct() const {
		struct sockaddr_in addr;
		memset(&addr, 0, sizeof(struct sockaddr_in));

		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);

		inet_aton(address.c_str(), &addr.sin_addr);

		return addr;
	}

	/**
	 * Gets the port of the socket
	 * @return the port number
	 */
	int get_port() const {
		return port;
	}

	/**
	 * Gets the address of the socket
	 * @return the address
	 */
	std::string get_address() const {
		return address;
	}

	/**
	 * Gets the address of the socket
	 * @return the address
	 */
	uint32_t get_address_binary() const {
		struct sockaddr_in addr = this->get_struct();
		return addr.sin_addr.s_addr;
	}
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_NETWORK_SOCKETADDRESS_H_ */
