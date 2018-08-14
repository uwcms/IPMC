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

#define SOCKET_DEFAULT_KEEPALIVE ///< All new sockets start with keep alive enabeld by default

/**
 * C++ socket wrapper and automatic cleanup
 *
 * TODO
 */
class Socket {
public:
	struct Timeout : public std::exception {};

public:
	/**
	 * Creates a socket instance based upon an already existing
	 * socket file descriptor and sockaddr_in structure.
	 * Used for example after a call to ServerSocket::accept()
	 * @param socket socket file descriptor
	 * @param sockaddr address structure
	 */
	Socket(int socket, struct sockaddr_in sockaddr);

	/**
	 * Creates a new socket based on an address, port and TCP or UDP
	 * @param address The target address.
	 * @param port The port to use.
	 * @param useTCP true if TCP/IP, false if UDP/IP.
	 */
	Socket(std::string address, unsigned short port, bool useTCP = true);

	virtual ~Socket();

	/**
	 * Receives data if present.
	 * @param ptr The data buffer.
	 * @param len The length of the buffer in buffer.
	 * @return The Total number of read bytes.
	 */
	int recv(void *ptr, int len);

	/**
	 * Same as Socket::recv but with a custom timeout. For permanent timeout in
	 * all operation use Socket::setRecvTimeout instead and then the regular
	 * Socket::recv function.
	 * @param buf The data buffer.
	 * @param len The length of the buffer in buffer.
	 * @param timeout_ms Timeout in milliseconds.
	 * @return The total number of read bytes.
	 * @throws Socket::Timeout if there was a timeout.
	 */
	int recv(void* buf, int len, unsigned int timeout_ms);

	/**
	 * Will receive the number of requested bytes.
	 * @param ptr The data buffer.
	 * @param len Number of bytes available in buffer and quantity to read.
	 * @return 1 if read successful.
	 */
	int recvn(void *ptr, int len);

	/**
	 * Sends an array of characters to the client.
	 * @param buf The character buffer.
	 * @param len The starting position.
	 */
	int send(const void* buf, int len);

	/**
	 * Sends an array of characters to the client with a timeout set. This
	 * will override the timeout (if set) by Socket::setSendTimout.
	 * @param buf The character buffer.
	 * @param len The starting position.
	 * @param timeout_ms Timeout in milliseconds.
	 * @return The total number of read bytes.
	 * @throws Socket::Timeout if there was a timeout.
	 */
	int send(const void* buf, int len, unsigned int timeout_ms);

	/**
	 * Sends a string to the client.
	 * @param str The string to send.
	 */
	int send(std::string str);

	/**
	 * Sends a string to the client with a timeout. This
	 * will override the timeout (if set) by Socket::setSendTimout.
	 * @param str The string to send.
	 * @param timeout_ms Timeout in milliseconds.
	 * @throws Socket::Timeout if there was a timeout.
	 */
	int send(std::string str, unsigned int timeout_ms);

	void setBlocking(); ///< Sets the socket in blocking mode.
	void setNonblocking(); ///< Sets the socket in non-blocking mode.

	///! Sets the receiving timeout in milliseconds.
	void setRecvTimeout(uint32_t ms);
	///! Disables receiving timeout.
	inline void disableRecvTimeout() { setRecvTimeout(0); };
	///! Retrieve the current receive timeout configuration.
	inline uint32_t getRecvTimeout() { return this->recvTimeout; };

	///! Sets the transmitting timeout in milliseconds.
	void setSendTimeout(uint32_t ms);
	///! Disables transmitting timeout.
	inline void disableSendTimeout() { setRecvTimeout(0); };
	///! Retrieve the current transmit timeout configuration.
	inline uint32_t getSendTimeout() { return this->sendTimeout; };

	/**
	 * Enables no delay on TCP incoming packets.
	 * Useful when all incoming packets will be likely small in size.
	 */
	void enableNoDelay();
	void disableNoDelay(); ///< Disable no delay on TCP incoming packets.

	/**
	 * Enable keep alive packets.
	 * Allows detection of dropped connections and permits long lasting inactive connections.
	 */
	void enableKeepAlive();
	void disableKeepAlive(); ///< Disable keep alive packets.

	/**
	 * Closes the socket connection
	 */
	void close();

	/**
	 * Checks whether the socket is valid
	 * @return true of the socket is valid, false otherwise
	 */
	inline bool isValid() { return socketfd != -1; }

	/**
	 * Checks if the socket is configured for TCP/IP operation,
	 * if false it means it is UDP
	 * @return true if TCP/IP socket, UDP/IP otherwise
	 */
	bool isTCP();

	/**
	 * Gets the socket file descriptor
	 * @return the socket file descriptor
	 */
	inline int getSocket() { return socketfd; }

	/**
	 * Gets the socketaddress instance of the socket, which contains
	 * information about the socket's address and port
	 * @return the socketaddress instance
	 */
	inline const SocketAddress& getSocketAddress() { return sockaddr; }

	///! Operator overload for int assignment, returns socketfd
	inline operator int() { return this->getSocket(); }

protected:
	int socketfd;				///< The socket file descriptor number.
	SocketAddress sockaddr;		///< The socket address and port.
	unsigned int recvTimeout;	///< Receive timeout in ms.
	unsigned int sendTimeout;	///< Transmit timeout in ms.
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_NETWORK_SOCKET_H_ */
