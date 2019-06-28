/*
 * Lwiperf.h
 *
 *  Created on: Jun 25, 2018
 *      Author: mpv
 */
#ifndef SRC_COMMON_UW_IPMC_SERVICES_LWIPERF_LWIPERF_H
#define SRC_COMMON_UW_IPMC_SERVICES_LWIPERF_LWIPERF_H

/**
 * A simple IPERF2 server that receives and ditches ethernet packets.
 * Constructor launches thread automatically.
 */
class Lwiperf {
public:
	/**
	 * Start the IPERF2 server
	 * @param port Port associated with the server.
	 */
	Lwiperf(unsigned short port);

private:
	unsigned short port;
};


#endif /* SRC_COMMON_UW_IPMC_SERVICES_LWIPERF_LWIPERF_H */
