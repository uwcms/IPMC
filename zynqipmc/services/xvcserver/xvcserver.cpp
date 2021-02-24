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


#include "xvcserver.h"
#include <core.h>
#include <drivers/network/server_socket.h>
#include <libs/threading.h>

XVCServer::XVCServer(void* baseAddr, LogTree& log, uint16_t port) :
acceptAddr("none"), kBaseAddr(baseAddr), log(log), kPort(port) {
	// Start the XVC server thread
	runTask("xvcserver:" + std::to_string(port), TASK_PRIORITY_BACKGROUND, [this]() {
		// Just allow one connection at a time
		ServerSocket server(this->kPort);

		int err = server.listen();
		if (err != 0) {
			this->log.log("Unable to listen to port: " + std::string(strerror(err)), LogTree::LOG_ERROR);
			return;
		}

		while (true) {
			std::shared_ptr<Socket> client = server.accept();

			if (!client->isValid()) {
				continue;
			}

			if (this->acceptAddr == "none" || (this->acceptAddr != "*" && client->getSocketAddress().getAddress() != this->acceptAddr)) {
				this->log.log("XVC connection from "  + client->getSocketAddress().getAddress() + " refused as it does not match the current accepted address.", LogTree::LOG_WARNING);
				client->close();
			}
			else if (this->isRunning) {
				// There is already a connection established, refuse new one
				this->log.log("XVC connection from "  + client->getSocketAddress().getAddress() + " refused, only one connection allowed", LogTree::LOG_WARNING);
				client->close();
			} else {
				this->log.log("XVC connection established from " + client->getSocketAddress().getAddress(), LogTree::LOG_NOTICE);

				// Set TCP_NODELAY to socket
				client->enableNoDelay();

				this->isRunning = true;
				runTask("xvc", TASK_PRIORITY_BACKGROUND, [this, client]() {
					this->handleClient(client);
					this->isRunning = false;

					this->log.log("XVC connection closed", LogTree::LOG_NOTICE);
				});
			}
		}
	});
}


bool XVCServer::handleClient(std::shared_ptr<Socket> s) {
	const char xvcInfo[] = "xvcServer_v1.0:2048\n";
	volatile jtag_t *jtag = (volatile jtag_t*)kBaseAddr;

	do {
		char cmd[16];
		unsigned char buffer[2048], result[1024];
		memset(cmd, 0, 16);

		if (s->recvn(cmd, 2) != 2)
			return true;

		if (memcmp(cmd, "ge", 2) == 0) {
			if (s->recvn(cmd, 6) != 6)
				return 1;
			memcpy(result, xvcInfo, strlen(xvcInfo));
			if (s->send((char*)result, strlen(xvcInfo)) != (int)strlen(xvcInfo)) {
				this->log.log("Unable to write to socket", LogTree::LOG_ERROR);
				return 1;
			}

		} else if (memcmp(cmd, "se", 2) == 0) {
			if (s->recvn(cmd, 9) != 9)
				return 1;
			memcpy(result, cmd + 5, 4);
			if (s->send((char*)result, 4) != 4) {
				this->log.log("Unable to write to socket", LogTree::LOG_ERROR);
				return 1;
			}

		} else if (memcmp(cmd, "sh", 2) == 0) {
			if (s->recvn(cmd, 4) != 4)
				return 1;

			int len;
			if (s->recvn((char*)&len, 4) != 4) {
				this->log.log("Reading length failed", LogTree::LOG_ERROR);
				return 1;
			}

			size_t nr_bytes = (len + 7) / 8;
			if (nr_bytes * 2 > sizeof(buffer)) {
				this->log.log("Buffer size exceeded", LogTree::LOG_ERROR);
				return 1;
			}

			if (s->recvn((char*)buffer, nr_bytes * 2) != (int)(nr_bytes * 2)) {
				this->log.log("Reading data failed", LogTree::LOG_ERROR);
				return 1;
			}

			memset(result, 0, nr_bytes);

			int bytesLeft = nr_bytes;
			int bitsLeft = len;
			int byteIndex = 0;
			int tdi, tms, tdo;

			while (bytesLeft > 0) {
				tms = 0;
				tdi = 0;
				tdo = 0;
				if (bytesLeft >= 4) {
					memcpy(&tms, &buffer[byteIndex], 4);
					memcpy(&tdi, &buffer[byteIndex + nr_bytes], 4);

					jtag->length_offset = 32;
					jtag->tms_offset = tms;
					jtag->tdi_offset = tdi;
					jtag->ctrl_offset = 0x01;

					// Switch this to interrupt in next revision
					while (jtag->ctrl_offset);

					tdo = jtag->tdo_offset;
					memcpy(&result[byteIndex], &tdo, 4);

					bytesLeft -= 4;
					bitsLeft -= 32;
					byteIndex += 4;

				} else {
					memcpy(&tms, &buffer[byteIndex], bytesLeft);
					memcpy(&tdi, &buffer[byteIndex + nr_bytes], bytesLeft);

					jtag->length_offset = bitsLeft;
					jtag->tms_offset = tms;
					jtag->tdi_offset = tdi;
					jtag->ctrl_offset = 0x01;

					// Switch this to interrupt in next revision
					while (jtag->ctrl_offset);

					tdo = jtag->tdo_offset;
					memcpy(&result[byteIndex], &tdo, bytesLeft);

					break;
				}
			}

			if (s->send((char*)result, nr_bytes) != (int)nr_bytes) {
				this->log.log("Unable to write to socket", LogTree::LOG_ERROR);
				return 1;
			}
		} else {
			this->log.log("Invalid command received: " + std::string(cmd), LogTree::LOG_ERROR);
			return 1;
		}
	} while (1);
	/* Note: Need to fix JTAG state updates, until then no exit is allowed */
}

namespace {
/// Change the override handle status
class AcceptCommand : public CommandParser::Command {
public:
	AcceptCommand(XVCServer &xvcserver) : xvcserver(xvcserver) { };

	virtual std::string getHelpText(const std::string &command) const {
		return command + " [$ipv4_address]\n\n"
				"Set the host from which XVC connections will be accepted.\n"
				"\n"
				"\"none\" will reject all connections.\n"
				"\"*\" will accept all connections.\n"
				"\n"
				"WARNING: JTAG (and therefore XVC) is capable of causing hardware damage.\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		if (parameters.nargs() <= 1) {
			if (!this->xvcserver.acceptAddr.size() || this->xvcserver.acceptAddr == "none")
				console->write("XVC connections are not currently accepted.\n");
			else if (this->xvcserver.acceptAddr == "*")
				console->write("XVC connections are currently accepted from anywhere.\n");
			else
				console->write("XVC connections are currently accepted from \"" + this->xvcserver.acceptAddr + "\"\n");
		}
		else if (parameters.nargs() == 2) {
			this->xvcserver.acceptAddr = parameters.parameters[1];
		}
		else {
			console->write("Invalid arguments, see help.\n");
		}
	}

private:
	XVCServer &xvcserver;
};
}; // namespace <anonymous>

void XVCServer::registerConsoleCommands(CommandParser &parser, const std::string &prefix) {
	parser.registerCommand(prefix + "accept", std::make_shared<AcceptCommand>(*this));
}

void XVCServer::deregisterConsoleCommands(CommandParser &parser, const std::string &prefix) {
	parser.registerCommand(prefix + "accept", nullptr);
}
