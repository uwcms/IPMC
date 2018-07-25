/*
 * ServerSocket.h
 *
 *  Created on: Feb 8, 2018
 *      Author: mpv
 */

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_NETWORK_SERVERSOCKET_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_NETWORK_SERVERSOCKET_H_

#include "Socket.h"
#include <memory>

class ServerSocket : public Socket {
public:

	/**
	 * Constructs a listening socket on the specified port and address
	 * The default address is 0.0.0.0 and the default backlog is 5.
	 * @param address address to bind to
	 * @param port port to listen to
	 * @param backlog number of backlogs
	 */
	ServerSocket(unsigned short port, std::string address = "0.0.0.0", int backlog = 5);

	~ServerSocket();

	/**
	 * Creates the listening socket and binds to the current port and address
	 * @return error code if there was a problem
	 */
	int listen();

	int bind();

	/**
	 * Accepts a new incoming client (blocking call).
	 * @return A client socket instance
	 */
	std::shared_ptr<Socket> accept();

protected:
	int backlog;
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_NETWORK_SERVERSOCKET_H_ */
