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
#include "FreeRTOS.h"

#include "SocketAddress.h"

/**
 * C++ socket wrapper and automatic cleanup
 *
 * TODO
 */
class Socket {
protected:
	int socketfd;
	SocketAddress sockaddr;

public:
	Socket(int socket, struct sockaddr_in addr);
	virtual ~Socket();

	/**
	 * Reads a specified amount of data into a character pointer
	 * @param buf the character buffer
	 * @param len the length of the character buffer
	 * @param timeout A timeout for this read operation.
	 * @return The amount of data sent.
	 */
	virtual int read(char* buf, int len, TickType_t timeout=portMAX_DELAY);

	/**
	 * Sends an array of charactes to the client, with a specified start and end index
	 * @param buf the character buffer
	 * @param len the length
	 * @param timeout A timeout for this read operation.
	 * @return The amount of data sent.
	 */
	virtual int send(const char* buf, int len, TickType_t timeout=portMAX_DELAY);
	/// \overload
	virtual int send(std::string data, TickType_t timeout=portMAX_DELAY) {
		return this->send(data.c_str(), data.length(), 0);
	};

	/**
	 * Sets the socket in blocking mode
	 */
	virtual void set_blocking();

	/**
	 * Sets the socket in non-blocking mode
	 */
	virtual void set_unblocking();

	/**
	 * Closes the socket connection
	 */
	virtual void close() {
		if (socketfd == -1) {
			return;
		}

		lwip_close(socketfd);
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
	virtual int get_socket() {
		return socketfd;
	}

	/**
	 * Gets the socketaddress instance of the socket, which contains
	 * information about the socket's address and port
	 * @return the socketaddress instance
	 */
	virtual const SocketAddress& get_socketaddress() {
		return sockaddr;
	}

	/**
	 * Get the socket-specific errno.
	 *
	 * @return The errno for this socket.
	 */
	virtual int get_errno() {
		int error;
		u32_t optlen = sizeof(error);
		lwip_getsockopt(this->socketfd, SOL_SOCKET, SO_ERROR, &error, &optlen);
		return error;
	}
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_NETWORK_SOCKET_H_ */
