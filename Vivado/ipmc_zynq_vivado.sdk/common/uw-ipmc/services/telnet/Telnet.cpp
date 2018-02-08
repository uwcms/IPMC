/*
 * Telnet.cpp
 *
 *  Created on: Feb 8, 2018
 *      Author: mpv
 */

#include <IPMC.h>
#include <FreeRTOS.h>
#include <task.h>

#include "Telnet.h"

#include "drivers/network/ServerSocket.h"


void _thread_telnetd(void *p) {
	reinterpret_cast<TelnetServer*>(p)->thread_telnetd();
}

TelnetServer::TelnetServer() {
	xTaskCreate(_thread_telnetd, "telnetd", UWIPMC_STANDARD_STACK_SIZE, this, TASK_PRIORITY_SERVICE, NULL);
}

void TelnetServer::thread_telnetd() {
	ServerSocket *server = new ServerSocket(23, TELNET_MAX_INSTANCES);

	int err = server->listen();

	if (err != 0) {
		printf(strerror(err));
		vTaskDelete(NULL);
	}

	while (true) {
		Socket *client = server->accept();

		if (!client->valid()) {
			delete client;
			continue;
		}

		// Launch a new telnet instance if client is valid
		TelnetClient *c = new TelnetClient(client);
	}

	vTaskDelete(NULL);
}

void _thread_telnetc(void *p) {
	reinterpret_cast<TelnetClient*>(p)->thread_telnetc();
}

TelnetClient::TelnetClient(Socket *s) :
socket(s) {
	xTaskCreate(_thread_telnetc, "telnetc", UWIPMC_STANDARD_STACK_SIZE, this, TASK_PRIORITY_SERVICE, NULL);
}

void TelnetClient::thread_telnetc() {
	// Echo for now
	//printf("New client\n");

	socket->send("Telnet from IPMC\n");

	while (true) {
		char buffer[32] = "";

		if (socket->read(buffer, 31) <= 0) {
			break;
		}

		if (socket->send(buffer, strlen(buffer) + 1, 0) <= 0) {
			break;
		}
	}

	delete socket;
	//printf("Client disconnected\n");

	// TODO: I am not sure if this is correct or not, goal
	//   is to destroy class that is created in
	//   TelnetServer::thread_telnetd when a client connects
	delete this;

	vTaskDelete(NULL);
}
