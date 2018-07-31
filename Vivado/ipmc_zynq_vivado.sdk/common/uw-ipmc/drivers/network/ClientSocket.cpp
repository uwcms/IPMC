/*
 * ClientSocket.cpp
 *
 *  Created on: Jul 25, 2018
 *      Author: mpv
 */

#include <drivers/network/ClientSocket.h>

ClientSocket::ClientSocket(std::string address, unsigned short port)
: Socket(address, port) {
	// TODO Auto-generated constructor stub

}

ClientSocket::~ClientSocket() {
	// TODO Auto-generated destructor stub
}

int ClientSocket::connect() {
	return lwip_connect(this->socketfd, *this->sockaddr, sizeof(struct sockaddr));
}
