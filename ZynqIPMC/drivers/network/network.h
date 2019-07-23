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

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_NETWORK_NETWORK_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_NETWORK_NETWORK_H_

#include <string>
#include <lwip/netif.h>
#include <services/console/CommandParser.h>
#include <libs/logtree/logtree.h>

/**
 * lwIP driver wrapper.
 *
 * Will configure and start all necessary threads for lwIP operation.
 * @note For lwIP configuration check lwipopts.h.
 */
class Network final : public ConsoleCommandSupport {
public:
	//! Only one Network instance is allowed, this will return that instance if already initialized.
	static inline const Network* getInstance() { return Network::global_instance; };

	/**
	 * Starts the network interfaces and lwIP, required for ethernet.
	 * @param logtree Logging interface.
	 * @param mac The MAC address to uniquely identify the device.
	 * @param net_ready_cb The callback function that will get called when network is up.
	 * @throw std::logic_error is network was already previously initialized.
	 */
	Network(LogTree &logtree, const uint8_t mac[6], std::function<void(Network*)> net_ready_cb = NULL);
	virtual ~Network() {};

	Network(Network const&) = delete;			///< No copy.
	void operator=(Network const&) = delete;	///< No assignment.

	//! Get link status.
	inline const bool isLinkUp() const {return netif_is_link_up(&netif);};

	//! Get interface status.
	inline const bool isInterfaceUp() const {return netif_is_up(&netif);};

	//! Get the raw IP address.
	inline const uint32_t getIP() const {return ntohl(netif.ip_addr.addr);};

	//! Get the current IP address.
	inline const std::string getIPString() const {return Network::ipaddrToString(netif.ip_addr);};

	//! Get the current Netmask address.
	inline const std::string getNetmaskString() const {return Network::ipaddrToString(netif.netmask);};

	//! Get the current Gateway address.
	inline const std::string getGatewayString() const {return Network::ipaddrToString(netif.gw);};

	/**
	 * Converts a standard ip_addr structure to a readable string.
	 * @param ip The structure to convert.
	 */
	static const std::string ipaddrToString(const ip_addr_t &ip);

	// From base class ConsoleCommandSupport:
	virtual void registerConsoleCommands(CommandParser &parser, const std::string &prefix="");
	virtual void deregisterConsoleCommands(CommandParser &parser, const std::string &prefix="");

	const unsigned int DHCP_TIMEOUT_SEC = 20; ///< DHCP timeout in seconds

private:
	// Console commands:
	class Status; ///< Network status.

	struct netif netif;	///< Internal network interface.
	uint8_t mac[6];		///< The MAC address used to configure lwIP.
	LogTree &logtree;	///< LogTree used for diagnostics.

	/**
	 * We keep an internal copy of the network object for
	 * exclusive use and to prevent other initializations.
	 */
	static Network* global_instance;

	void threadNetworkStart();	///< Network start thread.
};


#endif /* SRC_COMMON_UW_IPMC_DRIVERS_NETWORK_NETWORK_H_ */
