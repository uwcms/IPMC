/*
 * Socket.h
 *
 *  Created on: Feb 7, 2018
 *      Author: mpv
 */

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_NETWORK_SOCKET_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_NETWORK_SOCKET_H_

/**
 * C++ socket wrapper and automatic cleanup
 *
 * TODO
 */
class Socket {
public:
	Socket();
	virtual ~Socket();

private:
	int sockfd;
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_NETWORK_SOCKET_H_ */
