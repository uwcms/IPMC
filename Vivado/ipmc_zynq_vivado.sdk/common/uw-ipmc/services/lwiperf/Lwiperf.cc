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


Lwiperf::Lwiperf(unsigned short port) :
port(port) {
	xTaskCreate([](void* p) {
		Lwiperf *pInst = (Lwiperf*)p;

		ServerSocket server(pInst->port, 1);

		int err = server.listen();
		if (err != 0) {
			printf(strerror(err));
			vTaskDelete(NULL);
		}

		while (true) {
			Socket *client = server.accept();

			if (!client->valid()) {
				delete client;
				continue;
			}

			char recv_buf[1500];
			while (true) {
				/* keep reading data */
				if (client->read(recv_buf, 1460) <= 0)
					break;
			}

			delete client;
		}

		vTaskDelete(NULL);

	}, "lwiperfd", UWIPMC_STANDARD_STACK_SIZE, this, TCPIP_THREAD_HIGH_PRIO, NULL);
}
