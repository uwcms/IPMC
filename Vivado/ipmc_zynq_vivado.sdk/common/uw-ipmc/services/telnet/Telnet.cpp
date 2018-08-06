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
	ServerSocket *server = new ServerSocket(23);

	int err = server->listen();

	if (err != 0) {
		printf(strerror(err));
		return;
	}

	while (true) {
		std::shared_ptr<Socket> client = server->accept();

		if (!client->isValid()) {
			continue;
		}

		// Launch a new telnet instance if client is valid
		new TelnetClient(client);
	}
}

TelnetClient::TelnetClient(std::shared_ptr<Socket> s) :
socket(s) {
	const std::string name = std::string("telnetc:") + std::to_string(socket->getSocketAddress()->getPort());
	configASSERT(UWTaskCreate(name, TASK_PRIORITY_INTERACTIVE, [this]() -> void { this->thread_telnetc(); }));
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

	//printf("Client disconnected\n");

	// TODO: I am not sure if this is correct or not, goal
	//   is to destroy class that is created in
	//   TelnetServer::thread_telnetd when a client connects
	delete this;
}
