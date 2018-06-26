/*
 * xvcserver.h
 *
 *  Created on: Jun 26, 2018
 *      Author: mpv
 */

#ifndef SRC_COMMON_UW_IPMC_SERVICES_XVCSERVER_XVCSERVER_H_
#define SRC_COMMON_UW_IPMC_SERVICES_XVCSERVER_XVCSERVER_H_

#include <drivers/network/Socket.h>

/**
 * Xilinx Virtual Cable (XVC) TCP/IP server app based on XAPP1251:
 * https://github.com/Xilinx/XilinxVirtualCable
 * Server thread starts automatically object is instantiated
 * Default port is 2542
 **/
class XVCServer {
public:
	/**
	 * Create, initialize and start the XVC server
	 * @param baseAddr Base address for the AXI-JTAG firmware IP (e.g. XPAR_AXI_JTAG_0_BASEADDR)
	 * @param port The port to be associated with the TCP/IP server
	 */
	XVCServer(uint32_t baseAddr, uint16_t port = 2542);
	virtual ~XVCServer() {};

private:
	typedef struct {
		uint32_t  length_offset;
		uint32_t  tms_offset;
		uint32_t  tdi_offset;
		uint32_t  tdo_offset;
		uint32_t  ctrl_offset;
	} jtag_t;

	bool HandleClient(Socket *s);

	bool verbose;
	uint32_t baseAddr;
	uint16_t port;
};

#endif /* SRC_COMMON_UW_IPMC_SERVICES_XVCSERVER_XVCSERVER_H_ */
