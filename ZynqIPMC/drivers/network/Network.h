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
 * \note For lwIP configuration check lwipopts.h
 */
class Network {
public:
	/**
	 * Starts the network interfaces and lwIP, required for ethernet
	 * @param logtree Logging interface.
	 * @param mac The MAC address to uniquely identify the device.
	 * @param net_ready_cb The callback function that will get called when network is up.
	 * \note Network can only be instantiated ONCE, trying it will cause an assert
	 */
	Network(LogTree &logtree, uint8_t mac[6], std::function<void(Network*)> net_ready_cb = NULL);
	virtual ~Network() {};

	Network(Network const&) = delete;			///< No copy.
	void operator=(Network const&) = delete;	///< No assignment.

	// Getters
	/// Get link status
	inline const bool isLinkUp() {return netif_is_link_up(&netif);};
	/// Get interface status
	inline const bool isInterfaceUp() {return netif_is_up(&netif);};
	/// Get the raw IP address
	inline const uint32_t getIP() {return ntohl(netif.ip_addr.addr);};
	/// Get the current IP address
	inline const std::string getIPString() {return Network::ipaddr_to_string(netif.ip_addr);};
	/// Get the current Netmask address
	inline const std::string getNetmaskString() {return Network::ipaddr_to_string(netif.netmask);};
	/// Get the current Gateway address
	inline const std::string getGatewayString() {return Network::ipaddr_to_string(netif.gw);};

	/**
	 * Converts a standard ip_addr structure to a readable string
	 * @param ip The structure to convert
	 */
	static std::string ipaddr_to_string(ip_addr_t &ip);

	void register_console_commands(CommandParser &parser, const std::string &prefix="");
	void deregister_console_commands(CommandParser &parser, const std::string &prefix="");

	const unsigned int DHCP_TIMEOUT_SEC = 20; ///< DHCP timeout in seconds

public: // TODO: Make private
	uint8_t mac[6]; ///< The MAC address used to configure lwIP

	struct netif netif;

public: // TODO: Make private
	void thread_network_start(); ///< \protected Internal. Do not use.
	LogTree &logtree; ///<\protected Internal. Do not use.
};


#endif /* _NETWORK_NETWORK_H */
