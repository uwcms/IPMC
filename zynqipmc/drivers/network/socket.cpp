/*
 * This file is part of the ZYNQ-IPMC Framework.
 *
 * The ZYNQ-IPMC Framework is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The ZYNQ-IPMC Framework is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the ZYNQ-IPMC Framework.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "socket.h"
#include <fcntl.h>

#define DEFAULT_SOCKET_BUFFER 128

Socket::Socket(int socket, struct sockaddr_in sockaddr)
: socketfd(socket), sockaddr(sockaddr), recvTimeout(0), sendTimeout(0) {
	if (this->isValid()) {
#ifdef SOCKET_DEFAULT_KEEPALIVE
		this->enableKeepAlive();
#endif
	} else {
		// TODO: Shouldn't this also throw in case the socket fails to create?
	}
}

Socket::Socket(const std::string& address, unsigned short port, bool useTCP)
: sockaddr(address, port), recvTimeout(0), sendTimeout(0) {
	this->socketfd = lwip_socket(AF_INET, useTCP?SOCK_STREAM:SOCK_DGRAM, 0);

	if (this->isValid()) {
#ifdef SOCKET_DEFAULT_KEEPALIVE
	this->enableKeepAlive();
#endif
	} else {
		// TODO: Shouldn't this also throw in case the socket fails to create?
	}
}

Socket::~Socket() {
	this->close();
}

int Socket::recv(void* buf, size_t len) {
	return lwip_recv(this->socketfd, buf, len, 0);
}

int Socket::recv(void* buf, int len, unsigned int timeout_ms) {
	uint32_t old_timeout = getRecvTimeout();
	setRecvTimeout(timeout_ms); // Temporarily set the timeout

	int r = this->recv(buf, len);

	setRecvTimeout(old_timeout); // Recover to previous timeout
	if (r < 0 && ((errno == EAGAIN) || (errno == EWOULDBLOCK))) {
		throw except::timeout_error("Socket receive operation timed-out");
	}

	return r;
}

int Socket::recvn(void* buf, size_t len) {
	uint8_t *t = (uint8_t*)buf;
	size_t rem = len;
	while (rem) {
		int r = lwip_recv(this->socketfd, buf, rem, 0);
		if (r == 0) return (len - rem); // bytes received
		if (r < 0) return r; // error
		t += r;
		rem -= r;
	}
	return len;
}

int Socket::send(const void* buf, size_t len) {
	return lwip_send(this->socketfd, buf, len, 0);
}

int Socket::send(const void* buf, size_t len, unsigned int timeout_ms) {
	uint32_t old_timeout = getRecvTimeout();
	setSendTimeout(timeout_ms); // Temporarily set the timeout

	int r = this->send(buf, len);

	setSendTimeout(old_timeout); // Recover to previous timeout
	if (r < 0 && ((errno == EAGAIN) || (errno == EWOULDBLOCK))) {
		throw except::timeout_error("Socket send operation timed-out");
	}

	return r;
}

int Socket::send(const std::string& str) {
	return this->send((const uint8_t*)str.c_str(), str.length());
}

int Socket::send(const std::string& str, unsigned int timeout_ms) {
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

void Socket::enableNoDelay() {
	int optval = 1;
	lwip_setsockopt(this->socketfd, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
}

void Socket::disableNoDelay() {
	int optval = 0;
	lwip_setsockopt(this->socketfd, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
}

void Socket::enableKeepAlive() {
	int optval = 1;
	lwip_setsockopt(this->socketfd, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
}

void Socket::disableKeepAlive() {
	int optval = 0;
	lwip_setsockopt(this->socketfd, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
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
