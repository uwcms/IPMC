/*
 * FTPServer.cpp
 *
 *  Created on: Jul 23, 2018
 *      Author: mpv
 */

#include <IPMC.h>
#include <FreeRTOS.h>
#include <libs/threading.h>
#include <task.h>
#include <algorithm>
#include "FTPServer.h"


FTPServer::FTPServer(AuthCallbackFunc *authcallback, const uint16_t comport, const uint16_t dataport)
: authcallback(authcallback), comport(comport), dataport(dataport) {
	runTask(std::string(FTPSERVER_THREAD_NAME ":") + std::to_string(comport), TASK_PRIORITY_SERVICE, [this]() -> void {
		this->thread_ftpserverd();
	});
}

void FTPServer::thread_ftpserverd() {
	std::unique_ptr<ServerSocket> server (new ServerSocket(comport, FTPSERVER_MAX_BACKLOG));

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

		// Start the client, only one instance allowed
		delete new FTPClient(*this, client);
	}
}

bool FTPServer::authenticateUser(const std::string &user, const std::string &pass) const {
	if (this->authcallback == NULL) return false; // Don't allow to authenticate if no authenticate callback exists
	return this->authcallback(user, pass);
}


