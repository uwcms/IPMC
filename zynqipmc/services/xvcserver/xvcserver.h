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

#ifndef SRC_COMMON_ZYNQIPMC_SERVICES_XVCSERVER_XVCSERVER_H_
#define SRC_COMMON_ZYNQIPMC_SERVICES_XVCSERVER_XVCSERVER_H_

// TODO: Add command to turn off/on.
// TODO: Refuse all other incoming connections.

#include <memory>
#include <drivers/network/socket.h>
#include <libs/logtree/logtree.h>

/**
 * Xilinx Virtual Cable (XVC) TCP/IP server app based on XAPP1251:
 * https://github.com/Xilinx/XilinxVirtualCable
 * Server thread starts automatically object is instantiated.
 * Default port is 2542 and only one connection is allowed.
 **/
class XVCServer final {
public:
	/**
	 * Create, initialize and start the XVC server
	 * @param baseAddr Base address for the AXI-JTAG firmware IP (e.g. XPAR_AXI_JTAG_0_BASEADDR)
	 * @param port The port to be associated with the TCP/IP server
	 */
	XVCServer(void* baseAddr, LogTree& log, uint16_t port = 2542);
	~XVCServer() {};

private:
	typedef struct {
		uint32_t  length_offset;
		uint32_t  tms_offset;
		uint32_t  tdi_offset;
		uint32_t  tdo_offset;
		uint32_t  ctrl_offset;
	} jtag_t;

	bool handleClient(std::shared_ptr<Socket> s);

	volatile bool isRunning = false;	///< Indicates if an XVC client is already connected.

	const void* kBaseAddr;	///< XVC-JTAG IP base address.
	LogTree& log;			///< Log facility for debugging and status messages.
	const uint16_t kPort;	///< Server TCP port.
};

#endif /* SRC_COMMON_ZYNQIPMC_SERVICES_XVCSERVER_XVCSERVER_H_ */
