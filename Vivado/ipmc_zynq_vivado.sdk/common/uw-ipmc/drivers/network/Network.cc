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
#include "platform_config.h"

#include <atheros/atheros.h>
#include <drivers/network/Network.h>
#include <drivers/network/Network.h>
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

/* lwIP core includes */
#include "lwip/opt.h"
#include "lwip/tcpip.h"
#include "lwip/inet.h"
#include "lwip/dhcp.h"
#include "lwip/init.h"

void _thread_lwip_start(void *p) {
	reinterpret_cast<Network*>(p)->thread_lwip_start();
}

void _thread_networkd(void *p) {
	reinterpret_cast<Network*>(p)->thread_networkd();
}

std::string Network::ipaddr_to_string(struct ip_addr &ip) {
	std::string s = "";
	s += std::to_string(ip4_addr1(&ip)) + ".";
	s += std::to_string(ip4_addr2(&ip)) + ".";
	s += std::to_string(ip4_addr3(&ip)) + ".";
	s += std::to_string(ip4_addr4(&ip));

	return s;
}

Network::Network(LogTree &logtree, uint8_t mac[6]) :
logtree(logtree) {
	memcpy(this->mac, mac, sizeof(uint8_t) * 6);

	xTaskCreate(_thread_lwip_start, "lwip_start", UWIPMC_STANDARD_STACK_SIZE, this, configLWIP_TASK_PRIORITY, NULL);
}

void Network::thread_lwip_start() {
	// Initialize lwIP before calling sys_thread_new
	lwip_init();

	// Start the network daemon thread
	xTaskCreate(_thread_networkd, "networkd", UWIPMC_STANDARD_STACK_SIZE, this, DEFAULT_THREAD_PRIO, NULL);

#if LWIP_DHCP==1
	unsigned int mscnt = 0;
	while (1) {
		vTaskDelay(DHCP_FINE_TIMER_MSECS / portTICK_RATE_MS);
		if (this->netif.ip_addr.addr) {
			std::string s = "";
			s += "DHCP request success\n";
			s += "Address: " + ipaddr_to_string(netif.ip_addr) + "\n";
			s += "Netmask: " + ipaddr_to_string(netif.netmask) + "\n";
			s += "Gateway: " + ipaddr_to_string(netif.gw) + "\n";
			logtree.log(s, LogTree::LOG_INFO);
			break;
		}
		mscnt += DHCP_FINE_TIMER_MSECS;
		if (mscnt >= DHCP_TIMEOUT_SEC * 1000) {
			logtree.log("DHCP request timed out\n", LogTree::LOG_ERROR);
			// TODO: Consider applying a status configuration as Xilinx example
			/*xil_printf("Configuring default IP of 192.168.1.10\r\n");
			IP4_ADDR(&(server_netif.ip_addr), 192, 168, 1, 10);
			IP4_ADDR(&(server_netif.netmask), 255, 255, 255, 0);
			IP4_ADDR(&(server_netif.gw), 192, 168, 1, 1);
			print_ip_settings(&(server_netif.ip_addr), &(server_netif.netmask), &(server_netif.gw));*/
			break;
		}
	}
#endif

	vTaskDelete(NULL);
}

void Network::thread_networkd() {
	struct ip_addr ipaddr, netmask, gw;

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
		vTaskDelete(NULL);
		return;
	}

	// UW-IPMC specific, configure AR8035 internal delays
	ar8035_enable_internal_delays(&(this->netif));

	// Set the current interface as the default one
	netif_set_default(&(this->netif));

	// TODO: Find a way to get status with a callback
	//netif_set_status_callback(&netif, ?);

	// Specify that the network if is up
	netif_set_up(&(this->netif));

	// Start packet receive thread, required for lwIP operation
	xTaskCreate((void (*)(void*)) xemacif_input_thread, "xemacifd", UWIPMC_STANDARD_STACK_SIZE, this, DEFAULT_THREAD_PRIO, NULL);
	/*sys_thread_new("xemacifd", (void (*)(void*)) xemacif_input_thread, &(this->netif),
			UWIPMC_STANDARD_STACK_SIZE,
		DEFAULT_THREAD_PRIO);*/

#if LWIP_DHCP==1
	// If DHCP is enabled then start it
	int mscnt = 0;
	dhcp_start(&(this->netif));
	while (1) {
		vTaskDelay(DHCP_FINE_TIMER_MSECS / portTICK_RATE_MS);
		dhcp_fine_tmr();
		mscnt += DHCP_FINE_TIMER_MSECS;
		if (mscnt >= DHCP_COARSE_TIMER_SECS*1000) {
			dhcp_coarse_tmr();
			mscnt = 0;
		}
	}
#endif

	vTaskDelete(NULL);
}
