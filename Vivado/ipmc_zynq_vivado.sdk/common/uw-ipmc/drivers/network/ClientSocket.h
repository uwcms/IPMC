/*
 * ClientSocket.h
 *
 *  Created on: Jul 25, 2018
 *      Author: mpv
 */

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_NETWORK_CLIENTSOCKET_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_NETWORK_CLIENTSOCKET_H_

#import "Socket.h"

class ClientSocket : public Socket {
public:
	ClientSocket(std::string address, unsigned short port);
	virtual ~ClientSocket();

	int connect();
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_NETWORK_CLIENTSOCKET_H_ */
