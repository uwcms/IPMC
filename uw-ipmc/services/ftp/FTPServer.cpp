/*
 * FTPServer.cpp
 *
 *  Created on: Jul 23, 2018
 *      Author: mpv
 */

#include <IPMC.h>
#include <FreeRTOS.h>
#include <task.h>
#include <algorithm>

#include "drivers/network/ServerSocket.h"
#include <libs/ThreadingPrimitives.h>

#include "FTPServer.h"

FTPServer::FTPServer() {
	configASSERT(UWTaskCreate("ftpserverd:" + std::to_string(FTP_PORT), TASK_PRIORITY_SERVICE, [this]() -> void {
		this->thread_ftpserverd();
	}));

}

FTPServer::~FTPServer() {
	// TODO
}

void FTPServer::thread_ftpserverd() {
	std::unique_ptr<ServerSocket> server (new ServerSocket(21, FTP_MAX_INSTANCES));

	int err = server->listen();

	if (err != 0) {
		printf(strerror(err));
		return;
	}

	while (true) {
		std::shared_ptr<Socket> client = server->accept();

		if (!client->valid()) {
			continue;
		}

		// TODO
		delete new FTPClient(client);
	}
}




