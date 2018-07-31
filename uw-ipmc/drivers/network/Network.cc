/*
 * Network.c
 *
 *  Created on: Jan 6, 2018
 *      Author: mpvl
 */

#include <IPMC.h>
#include <string.h>
#include <stdio.h>
#include "xparameters.h"
#include "netif/xadapter.h"
#include "netif/xemacpsif.h"
#include "platform_config.h"

#include <drivers/network/Network.h>
#include <services/console/CommandParser.h>
#include <services/console/ConsoleSvc.h>
#include <FreeRTOS.h>
#include <task.h>

/* lwIP core includes */
#include "lwip/opt.h"
#include "lwip/tcpip.h"
#include "lwip/inet.h"
#include "lwip/dhcp.h"
#include "lwip/init.h"
#include "lwip/stats.h"

std::string Network::ipaddr_to_string(ip_addr_t &ip) {
	std::string s = "";
	s += std::to_string(ip4_addr1(&ip)) + ".";
	s += std::to_string(ip4_addr2(&ip)) + ".";
	s += std::to_string(ip4_addr3(&ip)) + ".";
	s += std::to_string(ip4_addr4(&ip));

	return s;
}

static Network *pNetworkInstance = NULL;

Network::Network(LogTree &logtree, uint8_t mac[6], std::function<void(Network*)> net_ready_cb) :
logtree(logtree) {
	configASSERT(!pNetworkInstance);
	pNetworkInstance = this;
	memcpy(this->mac, mac, sizeof(uint8_t) * 6);
	memset(&(this->netif), 0, sizeof(struct netif));

	configASSERT(UWTaskCreate("network_start", TCPIP_THREAD_PRIO, [this,net_ready_cb]() -> void {
		// It is imperative that lwIP gets initialized before the network start thread gets launched
		// otherwise there will be problems with TCP requests
		lwip_init();
		this->thread_network_start();
		if (net_ready_cb)
			net_ready_cb(this);
	}));
}

void Network::thread_network_start() {
	ip_addr_t ipaddr, netmask, gw;

#if LWIP_DHCP==0
	// TODO: Initialize IP addresses to be used
	IP4_ADDR(&ipaddr, 192, 168, 248, 70);
	IP4_ADDR(&netmask, 255, 255, 0, 0);
	IP4_ADDR(&gw, 192, 168, 1, 1);
#else
	ipaddr.addr = 0;
	gw.addr = 0;
	netmask.addr = 0;
#endif

	// Add network interface to the netif_list, netif_add internally called by Xilinx port
	if (!xemac_add(&(this->netif), &ipaddr, &netmask, &gw, this->mac, XPAR_XEMACPS_0_BASEADDR)) {
		logtree.log("Error adding network interface\n", LogTree::LOG_ERROR);
		return;
	}

	// Set the current interface as the default one
	netif_set_default(&(this->netif));

	// TODO: Find a way to get status with a callback
	netif_set_status_callback(&(this->netif), [](struct netif *netif) {
		if (netif_is_up(netif)) {
			pNetworkInstance->logtree.log("Network interface is UP", LogTree::LOG_NOTICE);
		} else {
			pNetworkInstance->logtree.log("Network interface is DOWN", LogTree::LOG_WARNING);
		}
	});

	netif_set_link_callback(&(this->netif), [](struct netif *netif) {
		if (netif_is_link_up(netif)){
			pNetworkInstance->logtree.log("Network link is UP", LogTree::LOG_NOTICE);
		} else {
			pNetworkInstance->logtree.log("Network link is DOWN", LogTree::LOG_WARNING);
		}
	});

	// Specify that the network if is up
	netif_set_up(&(this->netif));

	// Start packet receive thread, required for lwIP operation
	configASSERT(UWTaskCreate("xemacifd", TCPIP_THREAD_XEMACIFD_PRIO, [this]() -> void { xemacif_input_thread(&this->netif); }));

#if LWIP_DHCP==1
	// If DHCP is enabled then start it
	UWTaskCreate("_dhcpd", TCPIP_THREAD_PRIO, [this]() -> void {
		unsigned int mscnt = 0;

		dhcp_start(&this->netif);
		while (1) {
			vTaskDelay(DHCP_FINE_TIMER_MSECS / portTICK_RATE_MS);
			dhcp_fine_tmr();
			mscnt += DHCP_FINE_TIMER_MSECS;
			if (mscnt >= DHCP_COARSE_TIMER_MSECS) {
				dhcp_coarse_tmr();
				mscnt = 0;
			}
		}
	});

	// TODO
	unsigned int mscnt = 0;
	while (1) {
		vTaskDelay(DHCP_FINE_TIMER_MSECS / portTICK_PERIOD_MS);
		if (this->netif.ip_addr.addr) {
			std::string s = "";
			s += "DHCP request success\n";
			s += "Address: " + ipaddr_to_string(netif.ip_addr) + "\n";
			s += "Netmask: " + ipaddr_to_string(netif.netmask) + "\n";
			s += "Gateway: " + ipaddr_to_string(netif.gw) + "\n";
			logtree.log(s, LogTree::LOG_NOTICE);
			break;
		}
		mscnt += DHCP_FINE_TIMER_MSECS;
		if (mscnt >= DHCP_TIMEOUT_SEC * 1000) {
			logtree.log("DHCP request timed out\n", LogTree::LOG_ERROR);
			break;
		}
	}
#endif
}


namespace {
	/// A "status" console command.
	class Network_status : public CommandParser::Command {
	public:
		Network &network;

		/// Instantiate
		Network_status(Network &network) : network(network) { };

		virtual std::string get_helptext(const std::string &command) const {
			return stdsprintf(
					"%s\n"
					"\n"
					"Shows network status and statistics.\n", command.c_str());
		}

		virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
			console->write(std::string("Network status: Link is ") + (pNetworkInstance->isLinkUp()?"UP":"DOWN")+ ", interface is " + (pNetworkInstance->isInterfaceUp()?"UP":"DOWN") + "\n");
			console->write(std::string("IP Address: ") + pNetworkInstance->getIP() + "\n");
			console->write(std::string("Netmask: ") + pNetworkInstance->getNetmask() + "\n");
			console->write(std::string("Gateway: ") + pNetworkInstance->getGateway() + "\n");
		}
	};
};

/**
 * Register console commands related to this storage.
 * @param parser The command parser to register to.
 * @param prefix A prefix for the registered commands.
 */
void Network::register_console_commands(CommandParser &parser, const std::string &prefix) {
	parser.register_command(prefix + "status", std::make_shared<Network_status>(*this));
}

/**
 * Unregister console commands related to this storage.
 * @param parser The command parser to unregister from.
 * @param prefix A prefix for the registered commands.
 */
void Network::deregister_console_commands(CommandParser &parser, const std::string &prefix) {
	parser.register_command(prefix + "status", NULL);
}
