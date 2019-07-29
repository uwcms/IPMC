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

#ifndef SRC_COMMON_ZYNQIPMC_SERVICES_LWIPERF_LWIPERF_H
#define SRC_COMMON_ZYNQIPMC_SERVICES_LWIPERF_LWIPERF_H

/**
 * A simple IPERF2 server that receives and ditches ethernet packets.
 * Constructor launches thread automatically.
 */
class Lwiperf {
public:
	/**
	 * Start the IPERF2 server.
	 * @param port Port associated with the server.
	 */
	Lwiperf(unsigned short port);

private:
	unsigned short kPort;	///< Associated network port.
};


#endif /* SRC_COMMON_ZYNQIPMC_SERVICES_LWIPERF_LWIPERF_H */
