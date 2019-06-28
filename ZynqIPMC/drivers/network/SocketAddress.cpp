/*
 * SocketAddress.cpp
 *
 *  Created on: Jul 25, 2018
 *      Author: mpv
 */

#include "SocketAddress.h"

#include <string.h>
#include <lwip/netdb.h>

SocketAddress::SocketAddress(const std::string& address, unsigned short port) {
	memset(&(this->sockaddr), 0, sizeof(this->sockaddr));
	this->sockaddr.sin_len = sizeof(this->sockaddr);
	this->sockaddr.sin_port = htons(port);
	this->sockaddr.sin_family = AF_INET;

	if (inet_aton(address.c_str(), &(this->sockaddr.sin_addr)) == 0) {
		// Invalid address format
		// TODO: Use lwip_gethostbyname_r to be threadsafe
		struct hostent *host = lwip_gethostbyname(address.c_str());
		if (host != NULL) {
			memcpy(&this->sockaddr.sin_addr.s_addr,
					host->h_addr,
					host->h_length);
		} else {
			throw HostNotFound();
		}
	}
}

const std::string SocketAddress::getAddress() const {
	char str[IP4ADDR_STRLEN_MAX];

	ip4addr_ntoa_r((ip_addr_t*)&(this->sockaddr.sin_addr), str, IP4ADDR_STRLEN_MAX);

	return std::string(str);
}
