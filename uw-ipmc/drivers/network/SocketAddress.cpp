/*
 * SocketAddress.cpp
 *
 *  Created on: Jul 25, 2018
 *      Author: mpv
 */

#include "SocketAddress.h"

#include <string.h>
#include <lwip/netdb.h>

SocketAddress::SocketAddress(std::string address, unsigned short port)
: sockaddr({0}) {
	this->sockaddr.sin_port = htons(port);
	this->sockaddr.sin_family = AF_INET;

	if (inet_aton(address.c_str(), &this->sockaddr.sin_addr) == 0) {
		// Invalid address format
		// TODO: Is this thread safe??
		// TODO: There might be a problem here, what happens if the host address
		// changes and the socket needs to reconnect??
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

const std::string SocketAddress::getAddress() {
	std::string address;
	address += std::to_string(ip4_addr1(&(this->sockaddr.sin_addr))) + ".";
	address += std::to_string(ip4_addr2(&(this->sockaddr.sin_addr))) + ".";
	address += std::to_string(ip4_addr3(&(this->sockaddr.sin_addr))) + ".";
	address += std::to_string(ip4_addr4(&(this->sockaddr.sin_addr)));
	return address;
}
