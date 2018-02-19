/*
 * Socket.cc
 *
 *  Created on: Feb 7, 2018
 *      Author: mpv
 */

#include <drivers/network/Socket.h>
#include <string.h>
#include <fcntl.h>
#include <libs/ThreadingPrimitives.h>

#define DEFAULT_SOCKET_BUFFER 128

/**
 * Creates a socket instance based upon an already existing
 * socket file descriptor and sockaddr_in structure.
 * Used for example after a call to ::accept()
 * @param the socket file descriptor
 * @param the address structure
 */
Socket::Socket(int socket, struct sockaddr_in addr)
	: socketfd(socket), sockaddr(addr) {
}

Socket::~Socket() {
	close();
}

void Socket::set_blocking() {
	int opts = lwip_fcntl(socketfd, F_GETFL, 0);
	opts = opts & (~O_NONBLOCK);
	lwip_fcntl(socketfd, F_SETFL, opts);
}

void Socket::set_unblocking() {
	int opts = lwip_fcntl(socketfd, F_GETFL, 0);
	opts |= O_NONBLOCK;
	lwip_fcntl(socketfd, F_SETFL, opts);
}

/**
 * We may need to cap this to avoid stack overflows... somehow.
 *
 * Max standard MTU = 1500 bytes (375 words)
 * Min TCP header size = 20 bytes (5 words)
 */
#define MAX_NET_RW_SIZE (1500-20)

int Socket::read(char* buf, int len, TickType_t timeout) {
	AbsoluteTimeout abstimeout(timeout);

	int bytesread = 0;
	while (len > 0) {
		if (!this->socketfd) {
			errno = EBADF;
			return -1;
		}

		timeout = abstimeout.get_timeout();

		int rv;
		if (timeout != 0) {
			struct timeval tvtimeout;
			if (timeout == portMAX_DELAY) {
				tvtimeout.tv_sec = 0;
				tvtimeout.tv_usec = 0;
			}
			else {
				tvtimeout.tv_sec = timeout / 1000;
				tvtimeout.tv_usec = (timeout % 1000) * 1000;
			}
			fd_set fds;
			FD_ZERO(&fds);
			FD_SET(this->socketfd, &fds);

			rv = lwip_select(this->socketfd+1, &fds, NULL, NULL, (timeout == portMAX_DELAY ? NULL : &tvtimeout));
			if (rv < 0) {
				errno = this->get_errno();
				if (errno == EAGAIN || errno == EINTR) {
					continue;
				}
				else if (bytesread) {
					errno = 0;
					return bytesread; // Well we managed something at least.
				}
				else {
					return -1;
				}
			}
			if (rv == 0)
				break; // Timeout.
		}

		rv = lwip_recv(this->socketfd, buf, (len > MAX_NET_RW_SIZE ? MAX_NET_RW_SIZE : len), MSG_DONTWAIT);
		if (rv < 0) {
			errno = this->get_errno();
			volatile int my_errno = errno;
			if (errno == EAGAIN || errno == EINTR) {
				continue;
			}
			else if (bytesread) {
				errno = 0;
				return bytesread; // Well we managed something at least.
			}
			else {
				return -1;
			}
		}
		buf += rv;
		len -= rv;
		bytesread += rv;
	}
	errno = (bytesread ? 0 : EAGAIN);
	return bytesread;
}

#include <drivers/tracebuffer/TraceBuffer.h> // XXX
#include <IPMC.h> // XXX

int Socket::send(const char* buf, int len, TickType_t timeout) {
	AbsoluteTimeout abstimeout(timeout);

	int byteswritten = 0;
	while (len > 0) {
		if (!this->socketfd) {
			errno = EBADF;
			TRACE.log("lwipt", 5, LogTree::LOG_TRACE, "<SsendXa", 8, false);
			return -1;
		}

		int rv;
		timeout = abstimeout.get_timeout();
		if (timeout != 0) {
			struct timeval tvtimeout;
			if (timeout == portMAX_DELAY) {
				tvtimeout.tv_sec = 0;
				tvtimeout.tv_usec = 0;
			}
			else {
				tvtimeout.tv_sec = timeout / 1000;
				tvtimeout.tv_usec = (timeout % 1000) * 1000;
			}
			fd_set fds;
			FD_ZERO(&fds);
			FD_SET(this->socketfd, &fds);

			TRACE.log("lwipt", 5, LogTree::LOG_TRACE, ">select", 7, false);
			rv = lwip_select(this->socketfd+1, NULL, &fds, NULL, (timeout == portMAX_DELAY ? NULL : &tvtimeout));
			TRACE.log("lwipt", 5, LogTree::LOG_TRACE, "<select", 7, false);
			if (rv < 0) {
				errno = this->get_errno();
				if (errno == EAGAIN || errno == EINTR) {
					continue;
				}
				else if (byteswritten) {
					errno = 0;
					TRACE.log("lwipt", 5, LogTree::LOG_TRACE, "<SsendWa", 8, false);
					return byteswritten; // Well we managed something at least.
				}
				else {
					TRACE.log("lwipt", 5, LogTree::LOG_TRACE, "<SsendXb", 8, false);
					return -1;
				}
			}
			if (rv == 0)
				break; // Timeout
		}

		TRACE.log("lwipt", 5, LogTree::LOG_TRACE, ">send", 5, false);
		rv = lwip_send(this->socketfd, buf, (len > MAX_NET_RW_SIZE ? MAX_NET_RW_SIZE : len), MSG_DONTWAIT);
		TRACE.log("lwipt", 5, LogTree::LOG_TRACE, "<send", 5, false);
		if (rv < 0) {
			errno = this->get_errno();
			if (errno == EAGAIN || errno == EINTR) {
				continue;
			}
			else if (byteswritten) {
				errno = 0;
				TRACE.log("lwipt", 5, LogTree::LOG_TRACE, "<SsendWb", 8, false);
				return byteswritten; // Well we managed something at least.
			}
			else {
				TRACE.log("lwipt", 5, LogTree::LOG_TRACE, "<SsendX", 7, false);
				return -1;
			}
		}
		buf += rv;
		len -= rv;
		byteswritten += rv;
	}
	errno = (byteswritten ? 0 : EAGAIN);
	TRACE.log("lwipt", 5, LogTree::LOG_TRACE, "<SsendWc", 8, false);
	return byteswritten;
}
