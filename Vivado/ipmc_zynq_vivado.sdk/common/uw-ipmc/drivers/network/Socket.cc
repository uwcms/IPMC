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

Socket::Socket(int socket, struct sockaddr_in sockaddr)
: socketfd(socket), sockaddr(sockaddr),
  recvTimeout(0), sendTimeout(0) {
}

Socket::Socket(std::string address, unsigned short port, bool useTCP)
: sockaddr(address, port), recvTimeout(0), sendTimeout(0) {

	this->socketfd = lwip_socket(AF_INET, useTCP?SOCK_STREAM:SOCK_DGRAM, 0);

	// TODO: Shouldn't this also throw in case the socket fails to create?
}

Socket::~Socket() {
	this->close();
}

int Socket::recv(void* buf, int len) {
	return lwip_recv(this->socketfd, buf, len, 0);
}

int Socket::recv(void* buf, int len, unsigned int timeout_ms) {
	uint32_t old_timeout = getRecvTimeout();
	setRecvTimeout(timeout_ms); // Temporarily set the timeout

	int r = this->recv(buf, len);

	setRecvTimeout(old_timeout); // Recover to previous timeout
	if (r < 0 && ((errno == EAGAIN) || (errno == EWOULDBLOCK))) {
		throw Timeout();
	}

	return r;
}

int Socket::recvn(void* buf, int len) {
	uint8_t *t = (uint8_t*)buf;
	while (len) {
		int r = lwip_recv(this->socketfd, buf, len, 0);
		if (r <= 0)
			return r;
		t += r;
		len -= r;
	}
	return 1;
}

int Socket::send(const void* buf, int len) {
	return lwip_send(this->socketfd, buf, len, 0);
}

int Socket::send(const void* buf, int len, unsigned int timeout_ms) {
	uint32_t old_timeout = getRecvTimeout();
	setSendTimeout(timeout_ms); // Temporarily set the timeout

	int r = this->send(buf, len);

	setSendTimeout(old_timeout); // Recover to previous timeout
	if (r < 0 && ((errno == EAGAIN) || (errno == EWOULDBLOCK))) {
		throw Timeout();
	}

	return r;
}

int Socket::send(std::string str) {
	return this->send((const uint8_t*)str.c_str(), str.length());
}

int Socket::send(std::string str, unsigned int timeout_ms) {
	return this->send((const uint8_t*)str.c_str(), str.length(), timeout_ms);
}

void Socket::setBlocking() {
	int opts = lwip_fcntl(this->socketfd, F_GETFL, 0);
	opts = opts & (~O_NONBLOCK);
	lwip_fcntl(this->socketfd, F_SETFL, opts);
}

void Socket::setNonblocking() {
	int opts = lwip_fcntl(this->socketfd, F_GETFL, 0);
	opts |= O_NONBLOCK;
	lwip_fcntl(this->socketfd, F_SETFL, opts);
}

void Socket::setRecvTimeout(uint32_t ms) {
	struct timeval tv;
	tv.tv_sec = ms / 1000;
	tv.tv_usec = (ms % 1000) * 1000;

	lwip_setsockopt(this->socketfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(struct timeval));

	this->recvTimeout = ms;
}


void Socket::setSendTimeout(uint32_t ms) {
	struct timeval tv;
	tv.tv_sec = ms / 1000;
	tv.tv_usec = (ms % 1000) * 1000;

	lwip_setsockopt(this->socketfd, SOL_SOCKET, SO_SNDTIMEO, (char*)&tv, sizeof(struct timeval));

	this->sendTimeout = ms;
}

void Socket::setTCPNoDelay() {
	int flag = 1;
	lwip_setsockopt(this->socketfd, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(int));
}

void Socket::close() {
	if (this->socketfd == -1) {
		return;
	}

	lwip_close(this->socketfd);

	this->socketfd = -1;
}

bool Socket::isTCP() {
	// TODO: This can be a performance hog if used a lot I think,
	// consider changed it to a class variable
	int type;
	socklen_t length = sizeof(int);
	lwip_getsockopt(this->socketfd, SOL_SOCKET, SO_TYPE, &type, &length);
	return (type == SOCK_STREAM);
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
