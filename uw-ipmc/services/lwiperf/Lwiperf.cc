/*
 * lwiperf.cpp
 *
 *  Created on: Jun 25, 2018
 *      Author: mpv
 */

#include <services/lwiperf/Lwiperf.h>

#include <drivers/network/Socket.h>
#include <drivers/network/ServerSocket.h>
#include <FreeRTOS.h>
#include <task.h>
#include <libs/ThreadingPrimitives.h>

Lwiperf::Lwiperf(unsigned short port) :
port(port) {
	configASSERT(UWTaskCreate("lwiperfd", TCPIP_THREAD_HIGH_PRIO, [this]() -> void {
		ServerSocket server(this->port);

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

			uint8_t recv_buf[1500];
			while (true) {
				/* keep reading data */
				if (client->read(recv_buf, 1460) <= 0)
					break;
			}
		}
	}));
}
