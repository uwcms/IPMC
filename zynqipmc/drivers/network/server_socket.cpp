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

#include "server_socket.h"
#include <lwip/sockets.h>

ServerSocket::ServerSocket(unsigned short port, size_t backlog, const std::string& address) :
Socket(address, port), kBacklog(backlog) {
}

ServerSocket::~ServerSocket() {}

int ServerSocket::listen() {
	if (!isValid()) return -1;

	if (lwip_bind(this->socketfd, this->sockaddr, sizeof(struct sockaddr)) != 0) {
		close();
		return -errno;
	}

	if (lwip_listen(this->socketfd, kBacklog) != 0) {
		close();
		return -errno;
	}

	return 0;
}

int ServerSocket::reuse() {
	int yes = 1;
	if (lwip_setsockopt(this->socketfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) != 0) {
		close();
		return -errno;
	}

	return 0;
}

/*int ServerSocket::bind() {
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
}*/

std::shared_ptr<Socket> ServerSocket::accept() {
	if (!isValid()) return std::shared_ptr<Socket>(nullptr);

	struct sockaddr_in from;
	socklen_t l = sizeof(from);
	int clientfd = lwip_accept(this->socketfd, (struct sockaddr*)&from, &l);

	return std::shared_ptr<Socket>(new Socket(clientfd, from));
}
