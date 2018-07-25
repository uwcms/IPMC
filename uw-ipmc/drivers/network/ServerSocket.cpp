/*
 * ServerSocket.cpp
 *
 *  Created on: Feb 8, 2018
 *      Author: mpv
 */

#include <lwip/sockets.h>

#include <drivers/network/ServerSocket.h>

ServerSocket::ServerSocket(unsigned short port, std::string address, int backlog) :
Socket(-1, address, port) {
	this->backlog = backlog;
}

ServerSocket::~ServerSocket() {
	//delete sockaddr;
	//close();
}

int ServerSocket::listen() {
	struct sockaddr_in addr = sockaddr->getStruct();

	this->socketfd = lwip_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (this->socketfd == -1) {
		return errno;
	}

	/*if (lwip_setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) != 0) {
		close();
		return errno;
	}*/

	if (lwip_bind(this->socketfd, (struct sockaddr*)&addr, sizeof(struct sockaddr)) != 0) {
		close();
		return errno;
	}

	if (lwip_listen(this->socketfd, backlog) != 0) {
		close();
		return errno;
	}

	return 0;
}

int ServerSocket::bind() {
	// TODO: TCP/UDP
	struct sockaddr_in addr = sockaddr->getStruct();

	this->socketfd = lwip_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	if (this->socketfd == -1) {
		return errno;
	}

	if (lwip_bind(this->socketfd, (struct sockaddr*)&addr, sizeof(struct sockaddr)) != 0) {
		close();
		return errno;
	};

	return 0;
}

std::shared_ptr<Socket> ServerSocket::accept() {
	struct sockaddr_in from;
	socklen_t l = sizeof(from);
	int clientfd = lwip_accept(this->socketfd, (struct sockaddr*)&from, &l);

	return std::shared_ptr<Socket>(new Socket(clientfd, from));
}
