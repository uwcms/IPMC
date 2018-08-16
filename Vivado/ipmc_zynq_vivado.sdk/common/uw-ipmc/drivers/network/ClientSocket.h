/*
 * ClientSocket.h
 *
 *  Created on: Jul 25, 2018
 *      Author: mpv
 */

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_NETWORK_CLIENTSOCKET_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_NETWORK_CLIENTSOCKET_H_

#include "Socket.h"

/**
 * A client socket implementation practical to establish a connection to a server.
 */
class ClientSocket : public Socket {
public:
	/**
	 * Create a new socket by proving an address or a port
	 * @param address The target address.
	 * @param port The desired port.
	 * @throws SocketAddress::HostNotFound if DNS fails for an address that is an URL.
	 */
	ClientSocket(const std::string& address, unsigned short port);
	virtual ~ClientSocket() {};

	/**
	 * Connect to the target server.
	 * @return -1 if failed to connect.
	 */
	int connect();
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_NETWORK_CLIENTSOCKET_H_ */
