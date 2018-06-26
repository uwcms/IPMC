/*
 * Socket.cc
 *
 *  Created on: Feb 7, 2018
 *      Author: mpv
 */

#include <drivers/network/Socket.h>
#include <string.h>
#include <fcntl.h>

#define DEFAULT_SOCKET_BUFFER 128

/**
 * Creates a socket instance based upon an already existing
 * socket file descriptor and sockaddr_in structure.
 * Used for example after a call to ::accept()
 * @param the socket file descriptor
 * @param the address structure
 */
Socket::Socket(int socket, struct sockaddr_in addr) {
	socketfd = socket;
	sockaddr = new SocketAddress(addr);
}

Socket::~Socket() {
	delete sockaddr;
	close();
}

void Socket::setBlocking() {
	int opts = lwip_fcntl(socketfd, F_GETFL, 0);
	opts = opts & (~O_NONBLOCK);
	lwip_fcntl(socketfd, F_SETFL, opts);
}

void Socket::setUnblocking() {
	int opts = lwip_fcntl(socketfd, F_GETFL, 0);
	opts |= O_NONBLOCK;
	lwip_fcntl(socketfd, F_SETFL, opts);
}

void Socket::setTCPNoDelay() {
	int flag = 1;
	lwip_setsockopt(socketfd, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(int));
}

/*int Socket::read(std::string& msg) {
	int bytes_total = 0;
	char buffer[DEFAULT_SOCKET_BUFFER];

	int bytes_read = lwip_recv(socketfd, buffer, DEFAULT_SOCKET_BUFFER, 0);

	if (bytes_read <= 0) {
		return bytes_read;
	}

	msg.append(std::string(buffer, 0, bytes_read));
	bytes_total += bytes_read;

	// set non-blocking.
	set_unblocking();

	while (bytes_read > 0) {
		memset(buffer, 0, DEFAULT_SOCKET_BUFFER);
		bytes_read = lwip_recv(socketfd, buffer, DEFAULT_SOCKET_BUFFER, 0);

		if (bytes_read < 0) {
			break;
		}

		msg.append(std::string(buffer, 0, bytes_read));
		bytes_total += bytes_read;
	}

	// set back to blocking
	set_blocking();

	return bytes_total;
}*/

int Socket::read(void* buf, int len) {
	return lwip_recv(socketfd, buf, len, 0);
}

int Socket::sread(void* buf, int len) {
	uint8_t *t = (uint8_t*)buf;
	while (len) {
		int r = lwip_recv(socketfd, buf, len, 0);
		if (r <= 0)
			return r;
		t += r;
		len -= r;
	}
	return 1;
}

int Socket::send(std::string data) {
	return this->send((const uint8_t*)data.c_str(), data.length(), 0);
}

int Socket::send(const void* buf, int len, int flags) {
	return lwip_send(socketfd, buf, len, flags);
}
