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

#include "lwiperf.h"
#include <memory>
#include <drivers/network/server_socket.h>
#include <drivers/network/socket.h>
#include <libs/threading.h>

Lwiperf::Lwiperf(unsigned short port) :
kPort(port) {
	runTask("lwiperfd:" + std::to_string(port), TCPIP_THREAD_HIGH_PRIO, [this]() -> void {
		ServerSocket server(this->kPort);

		int err = server.listen();
		if (err != 0) {
			printf(strerror(err));
			return;
		}

		while (true) {
			std::shared_ptr<Socket> client = server.accept();

			if (!client->isValid()) {
				continue;
			}

			std::unique_ptr<uint8_t> recv_buf(new uint8_t[1500]);
			while (true) {
				/* keep reading data */
				if (client->recv(&*recv_buf, 1460) <= 0)
					break;
			}
		}
	});
}
