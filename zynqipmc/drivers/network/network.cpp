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

#include "network.h"
#include <FreeRTOS.h>
#include <task.h>
#include <libs/printf.h>
#include <libs/threading.h>
#include <libs/utils.h>
#include <services/console/consolesvc.h>

/* lwIP core includes */
#include "netif/xemacpsif.h"
#include "lwip/opt.h"
#include "lwip/tcpip.h"
#include "lwip/inet.h"
#include "lwip/dhcp.h"
#include "lwip/init.h"
#include "lwip/stats.h"

//! Generate a string form a MAC address.
static const std::string macToString(const uint8_t mac[6]) {
	std::string str;

	for (size_t i = 0; i < 6; i++) {
		str += stdsprintf("%02x", (char)mac[i]);
	    if (i != 5) str += ":";
	}

	return str;
}

const std::string Network::ipaddrToString(const ip_addr_t &ip) {
	std::string s = "";
	s += std::to_string(ip4_addr1(&ip)) + ".";
	s += std::to_string(ip4_addr2(&ip)) + ".";
	s += std::to_string(ip4_addr3(&ip)) + ".";
	s += std::to_string(ip4_addr4(&ip));

	return s;
}

//! We keep an internal copy of the network object for exclusive use and to prevent other initializations.
Network* Network::global_instance = nullptr;

Network::Network(LogTree &logtree, const uint8_t mac[6], std::function<void(Network*)> net_ready_cb) :
logtree(logtree) {
	if (this->global_instance) {
		throw std::logic_error("Network already constructed & initialized.");
	}

	this->global_instance = this;
	memcpy(this->mac, mac, sizeof(uint8_t) * 6);
	memset(&(this->netif), 0, sizeof(struct netif));

	runTask("network_start", TCPIP_THREAD_PRIO, [this,net_ready_cb]() -> void {
		// It is imperative that lwIP gets initialized before the network start thread gets launched
		// otherwise there will be problems with TCP requests
		lwip_init();
		this->threadNetworkStart();
		if (net_ready_cb)
			net_ready_cb(this);
	});
}

void Network::threadNetworkStart() {
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
			Network::getInstance()->logtree.log("Network interface is UP", LogTree::LOG_NOTICE);
		} else {
			Network::getInstance()->logtree.log("Network interface is DOWN", LogTree::LOG_WARNING);
		}
	});

	netif_set_link_callback(&(this->netif), [](struct netif *netif) {
		if (netif_is_link_up(netif)){
			Network::getInstance()->logtree.log("Network link is UP", LogTree::LOG_NOTICE);
		} else {
			Network::getInstance()->logtree.log("Network link is DOWN", LogTree::LOG_WARNING);
		}
	});

	// Specify that the network if is up
	netif_set_up(&(this->netif));

	// Start packet receive thread, required for lwIP operation
	runTask("xemacifd", TCPIP_THREAD_XEMACIFD_PRIO, [this]() -> void { xemacif_input_thread(&this->netif); });

#if LWIP_DHCP==1
	// If DHCP is enabled then start it
	runTask("_dhcpd", TCPIP_THREAD_PRIO, [this]() -> void {
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
			s += "Address: " + ipaddrToString(netif.ip_addr) + "\n";
			s += "Netmask: " + ipaddrToString(netif.netmask) + "\n";
			s += "Gateway: " + ipaddrToString(netif.gw) + "\n";
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

/// A "status" console command.
class Network::Status : public CommandParser::Command {
public:
	Status(Network &network) : network(network) { };

	virtual std::string getHelpText(const std::string &command) const {
		return command + "\n\n"
				"Shows network status and statistics.\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		// See http://www.ieee802.org/3/cf/public/jan17/wilton_3cf_03_0117.pdf to add better statistics
		static uint64_t rxbytes = 0, txbytes = 0;
		static uint32_t emac_cksum_err = 0;

		emac_cksum_err += Xil_In32(XPAR_XEMACPS_0_BASEADDR + XEMACPS_RXTCPCCNT_OFFSET);

		uint64_t rxbytes_h = Xil_In32(XPAR_XEMACPS_0_BASEADDR + XEMACPS_OCTRXH_OFFSET);
		uint64_t rxbytes_l = Xil_In32(XPAR_XEMACPS_0_BASEADDR + XEMACPS_OCTRXL_OFFSET);
		rxbytes += (rxbytes_h << 32) | rxbytes_l;

		uint64_t txbytes_h = Xil_In32(XPAR_XEMACPS_0_BASEADDR + XEMACPS_OCTTXH_OFFSET);
		uint64_t txbytes_l = Xil_In32(XPAR_XEMACPS_0_BASEADDR + XEMACPS_OCTTXL_OFFSET);
		txbytes += (txbytes_h << 32) | txbytes_l;

		console->write(std::string("Network status: Link is ") + (Network::getInstance()->isLinkUp()?"UP":"DOWN")+ ", interface is " + (Network::getInstance()->isInterfaceUp()?"UP":"DOWN") + "\n");
		console->write(std::string("MAC Address: ") + macToString(network.mac) + "\n");
		console->write(std::string("IP Address:  ") + Network::getInstance()->getIPString() + "\n");
		console->write(std::string("Netmask:     ") + Network::getInstance()->getNetmaskString() + "\n");
		console->write(std::string("Gateway:     ") + Network::getInstance()->getGatewayString() + "\n");
		console->write(std::string("TX bytes:    ") + std::to_string(txbytes) + " (" + bytesToString(txbytes) + ")\n");
		console->write(std::string("RX bytes:    ") + std::to_string(rxbytes) + " (" + bytesToString(rxbytes) + ")\n");
		console->write(std::string("Checksum Err (emac): ") + std::to_string(emac_cksum_err) + "\n");
#if LWIP_STATS
		console->write(std::string("Checksum Err (lwip): ") + std::to_string(lwip_stats.tcp.chkerr) + "\n");
#endif
	}

private:
	Network &network;
};

void Network::registerConsoleCommands(CommandParser &parser, const std::string &prefix) {
	parser.registerCommand(prefix + "status", std::make_shared<Network::Status>(*this));
}

void Network::deregisterConsoleCommands(CommandParser &parser, const std::string &prefix) {
	parser.registerCommand(prefix + "status", nullptr);
}
