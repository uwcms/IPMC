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
protected:
	int socketfd;
	SocketAddress* sockaddr;

public:
	Socket(int socket, struct sockaddr_in addr);
	virtual ~Socket();

	/**
	 * Reads data if present
	 * @param ptr the data buffer
	 * @param len the length of the buffer in buffer
	 * @return The total number of read bytes
	 */
	virtual int read(void *ptr, int len);

	/**
	 * Will read the number of requested bytes
	 * @param ptr the data buffer
	 * @param len number of bytes available in buffer and
	 *            quantity to read
	 * @return 1 if read successful
	 */
	virtual int sread(void *ptr, int len);

	/**
	 * Sends a string to the client
	 * @param the string to send
	 */
	virtual int send(std::string);

	/**
	 * Sends an array of charactes to the client, with a specified start and end index
	 * @param the character buffer
	 * @param the starting position
	 * @param the length
	 */
	virtual int send(const void* buf, int len, int flags=0);

	/**
	 * Sets the socket in blocking mode
	 */
	virtual void setBlocking();

	/**
	 * Sets the socket in non-blocking mode
	 */
	virtual void setUnblocking();

	/**
	 * Sets the socket to have no delay on TCP reads
	 */
	virtual void setTCPNoDelay();

	/**
	 * Closes the socket connection
	 */
	virtual void close() {
		if (socketfd == -1) {
			return;
		}

		lwip_close(socketfd);

		socketfd = -1;
	}

	/**
	 * Checks whether the socket is valid
	 * @return true of the socket is valid, false otherwise
	 */
	virtual bool valid() {
		return socketfd != -1;
	}

	/**
	 * Gets the socket file descriptor
	 * @return the socket file descriptor
	 */
	virtual inline int get_socket() {
		return socketfd;
	}

	/**
	 * Gets the socketaddress instance of the socket, which contains
	 * information about the socket's address and port
	 * @return the socketaddress instance
	 */
	virtual SocketAddress* get_socketaddress() {
		return sockaddr;
	}
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_NETWORK_SOCKET_H_ */
