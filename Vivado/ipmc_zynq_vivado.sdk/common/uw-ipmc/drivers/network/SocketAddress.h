/*
 * SocketAddress.h
 *
 *  Created on: Feb 8, 2018
 *      Author: mpv
 */

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_NETWORK_SOCKETADDRESS_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_NETWORK_SOCKETADDRESS_H_

#include <IPMC.h>
#include <lwip/inet.h>
#include <lwip/sockets.h>
#include <string.h>
#include <string>

class SocketAddress {
protected:
	unsigned short port;
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
	 * @param address The address of the socket
	 * @param port The port
	 */
	SocketAddress(std::string address, unsigned short port) {
		this->address = address;
		this->port = port;
	}

	/**
	 * Returns a sockaddr_in structure based on the information of the socketaddress instance
	 * @return sockaddr_in structure
	 */
	struct sockaddr_in getStruct() const {
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
	inline int getPort() const { return port; }

	/**
	 * Gets the address of the socket
	 * @return the address
	 */
	inline std::string getAddress() const { return address; }

	/**
	 * Gets the address of the socket
	 * @return the address
	 */
	inline uint32_t getAddressBinary() const { return this->getStruct().sin_addr.s_addr; }
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_NETWORK_SOCKETADDRESS_H_ */
