/*
 * Socket.h
 *
 *  Created on: Feb 7, 2018
 *      Author: mpv
 */

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_NETWORK_SOCKET_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_NETWORK_SOCKET_H_

#include <lwip/sockets.h>
#include <string>

#include "SocketAddress.h"

/**
 * C++ socket wrapper and automatic cleanup
 *
 * TODO
 */
class Socket {
public:
protected:
	int socketfd;
	SocketAddress* sockaddr;

public:
	Socket(int socket, struct sockaddr_in addr);
	~Socket();

	/**
	 * Reads a specified amount of data into a character pointer
	 * @param the character buffer
	 * @param the length of the character buffer
	 */
	int read(char*, int);

	/**
	 * Sends a string to the client
	 * @param the string to send
	 */
	int send(std::string);

	/**
	 * Sends an array of charactes to the client, with a specified start and end index
	 * @param the character buffer
	 * @param the starting position
	 * @param the length
	 */
	int send(const char* buf, int len, int flags=0);

	/**
	 * Sets the socket in blocking mode
	 */
	void set_blocking();

	/**
	 * Sets the socket in non-blocking mode
	 */
	void set_unblocking();

	/**
	 * Closes the socket connection
	 */
	void close() {
		if (socketfd == -1) {
			return;
		}

		lwip_close(socketfd);
	}

	/**
	 * Checks whether the socket is valid
	 * @return true of the socket is valid, false otherwise
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

	/**
	 * Gets the socketaddress instance of the socket, which contains
	 * information about the socket's address and port
	 * @return the socketaddress instance
	 */
	SocketAddress* get_socketaddress() {
		return sockaddr;
	}
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_NETWORK_SOCKET_H_ */
