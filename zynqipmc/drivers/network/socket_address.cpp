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

#include "socket_address.h"
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
			throw except::host_not_found("Unable to get host '" + address + "' by name");
		}
	}
}

const std::string SocketAddress::getAddress() const {
	char str[IP4ADDR_STRLEN_MAX];

	ip4addr_ntoa_r((ip_addr_t*)&(this->sockaddr.sin_addr), str, IP4ADDR_STRLEN_MAX);

	return std::string(str);
}
