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

/**
 * Extends Socket and provides an interface to bind and listen to ports.
 */
class ServerSocket : public Socket {
public:

	/**
	 * Constructs a listening socket on the specified port and address
	 * The default address is 0.0.0.0 and the default backlog is 3.
	 * @param address address to bind to
	 * @param port port to listen to
	 * @param backlog number of backlogs
	 */
	ServerSocket(unsigned short port, int backlog = 3, std::string address = "0.0.0.0");

	~ServerSocket();

	/**
	 * Creates the listening socket and binds to the current port and address
	 * @return negative error code if there was a problem
	 */
	int listen();

	/**
	 * Reuse the port, should be called before ServerSocket::listen.
	 * @return negative error code if there was a problem
	 */
	int reuse();

	//int bind();

	/**
	 * Accepts a new incoming client (blocking call).
	 * @return A client socket instance
	 */
	std::shared_ptr<Socket> accept();

protected:
	int backlog;	///< Backlog.
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_NETWORK_SERVERSOCKET_H_ */
