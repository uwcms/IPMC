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

class ServerSocket {
protected:
	int backlog;
	int socketfd;
	SocketAddress* sockaddr;

public:
	/**
	 * Constructs a listening socket on the specified port.
	 * The default address is 0.0.0.0 and the default backlog is 10.
	 *
	 * @param the port to listen to
	 */
	ServerSocket(int port) {
		sockaddr = new SocketAddress("0.0.0.0", port);
		backlog = 10;
	}

	/**
	 * Constructs a listening socket on the specified port with a backlog.
	 * The default address is 0.0.0.0.
	 *
	 * @param the port to listen to
	 * @param the number of backlogs
	 */
	ServerSocket(int port, int backlog) {
		sockaddr = new SocketAddress("0.0.0.0", port);
		this->backlog = backlog;
	}

	/**
	 * Constructs a listening socket on the specified port and address
	 * @param the port to listen to
	 * @param the number of backlogs
	 * @param the address to bind to
	 */
	ServerSocket(int port, int backlog, std::string address) {
		sockaddr = new SocketAddress(address, port);
		this->backlog = backlog;
	}

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

	/**
	 * Closes the listening socket
	 */
	void close() {
		if (socketfd == -1) {
			return;
		}

		lwip_close(socketfd);
	}

	/**
	 * Checks whether or not the socket is valid
	 * @return true the socket is valid, false otherwise
	 */
	bool valid() {
		return socketfd != -1;
	}

	/**
	 * Gets the socket file descriptor
	 * @return the socket file descriptor
	 */
	int get_socket() {
		return socketfd;
	}
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_NETWORK_SERVERSOCKET_H_ */
