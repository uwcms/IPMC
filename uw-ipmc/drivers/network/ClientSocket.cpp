/*
 * ClientSocket.cpp
 *
 *  Created on: Jul 25, 2018
 *      Author: mpv
 */

#include <drivers/network/ClientSocket.h>

ClientSocket::ClientSocket(const std::string& address, unsigned short port)
: Socket(address, port) {
}

int ClientSocket::connect() {
	return lwip_connect(this->socketfd, this->sockaddr, sizeof(struct sockaddr));
}
