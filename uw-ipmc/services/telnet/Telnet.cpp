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
#include <libs/ThreadingPrimitives.h>

TelnetServer::TelnetServer() {
	configASSERT(UWTaskCreate("telnetd", TASK_PRIORITY_SERVICE, [this]() -> void { this->thread_telnetd(); }));
}

void TelnetServer::thread_telnetd() {
	ServerSocket *server = new ServerSocket(23, TELNET_MAX_INSTANCES);

	int err = server->listen();

	if (err != 0) {
		printf(strerror(err));
		return;
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
}

TelnetClient::TelnetClient(Socket *s) :
socket(s) {
	configASSERT(UWTaskCreate("telnetc", TASK_PRIORITY_INTERACTIVE, [this]() -> void { this->thread_telnetc(); }));
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
}
