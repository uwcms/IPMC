/*
 * Network.h
 *
 *  Created on: Jan 6, 2018
 *      Author: mpvl
 */

#ifndef _NETWORK_NETWORK_H
#define _NETWORK_NETWORK_H

#include <FreeRTOS.h>
#include <IPMC.h>
#include <lwip/netif.h>

/**
 * lwIP initialization driver wrapper.
 *
 * Will configure and start all necessary threads for lwIP operation.
 * For lwIP configuration check lwipopts.h
 */
class Network {
	const unsigned int DHCP_TIMEOUT_SEC = 20; ///< DHCP timeout in seconds

private:
	uint8_t mac[6]; ///< The MAC address used to configure lwIP

	struct netif netif;

public:
	Network(LogTree &logtree, uint8_t mac[6]);
	virtual ~Network() {};

	static std::string ipaddr_to_string(struct ip_addr &ip);

public:
	void thread_lwip_start(); ///< \protected Internal. Do not use.
	void thread_networkd(); ///< \protected Internal. Do not use.

private:
	LogTree &logtree; ///< Log target
};


#endif /* _NETWORK_NETWORK_H */
