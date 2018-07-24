/*
 * ServerSocket.cpp
 *
 *  Created on: Feb 8, 2018
 *      Author: mpv
 */

#include <lwip/sockets.h>

#include <drivers/network/ServerSocket.h>

ServerSocket::~ServerSocket() {
	delete sockaddr;
	close();
}

int ServerSocket::listen() {
	struct sockaddr_in addr = sockaddr->get_struct();

	socketfd = lwip_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (socketfd == -1) {
		return errno;
	}

	/*if (lwip_setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) != 0) {
		close();
		return errno;
	}*/

	if (lwip_bind(socketfd, (struct sockaddr*)&addr, sizeof(struct sockaddr)) != 0) {
		close();
		return errno;
	}

	if (lwip_listen(socketfd, backlog) != 0) {
		close();
		return errno;
	}

	return 0;
}

int ServerSocket::bind() {
	struct sockaddr_in addr = sockaddr->get_struct();

	socketfd = lwip_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	if (socketfd == -1) {
		return errno;
	}

	if (lwip_bind(socketfd, (struct sockaddr*)&addr, sizeof(struct sockaddr)) != 0) {
		close();
		return errno;
	};

	return 0;
}

std::shared_ptr<Socket> ServerSocket::accept() {
	struct sockaddr_in from;
	socklen_t l = sizeof(from);
	int clientfd = lwip_accept(socketfd, (struct sockaddr*)&from, &l);

	return std::shared_ptr<Socket>(new Socket(clientfd, from));
}
