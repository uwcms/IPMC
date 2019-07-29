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

#include "ftp.h"
#include <core.h>
#include <libs/threading.h>


FTPServer::FTPServer(AuthCallbackFunc *authcallback, LogTree &log, const uint16_t comport, const uint16_t dataport, const std::string& thread_name, size_t backlog)
: authcallback(authcallback), log(log), kComPort(comport), kDataPort(dataport), kBacklog(backlog) {
	runTask(thread_name + ":" + std::to_string(comport), TASK_PRIORITY_SERVICE, [this]() -> void {
		this->threadFTPserverd();
	});
}

void FTPServer::threadFTPserverd() {
	std::unique_ptr<ServerSocket> server (new ServerSocket(kComPort, this->kBacklog));

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
		delete new FTPClient(*this, log[client->getSocketAddress().getAddress()], client);
	}
}

bool FTPServer::authenticateUser(const std::string &user, const std::string &pass) const {
	if (this->authcallback == nullptr) {
		return false; // Don't allow to authenticate if no authenticate callback exists
	}

	return this->authcallback(user, pass);
}


