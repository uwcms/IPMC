/*
 * xvcserver.cc
 *
 *  Created on: Jun 26, 2018
 *      Author: mpv
 */

#include <IPMC.h>
#include <services/xvcserver/XVCServer.h>
#include <drivers/network/ServerSocket.h>
#include <FreeRTOS.h>
#include <libs/threading.h>
#include <task.h>

XVCServer::XVCServer(uint32_t baseAddr, uint16_t port) :
baseAddr(baseAddr), port(port), verbose(false) {
	// Generate a unique thread name based on the port
	std::string threadName = "xvcserver:";
	threadName += std::to_string(port);

	// Start the XVC server thread
	runTask(threadName, TASK_PRIORITY_BACKGROUND, [this]() {
		// Just allow one connection at a time
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

			// Set TCP_NODELAY to socket
			client->enableNoDelay();

			this->HandleClient(client);
		}
	});
}


bool XVCServer::HandleClient(std::shared_ptr<Socket> s) {
	const char xvcInfo[] = "xvcServer_v1.0:2048\n";
	volatile jtag_t *jtag = (volatile jtag_t*)baseAddr;

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
			if (s->send((char*)result, strlen(xvcInfo)) != strlen(xvcInfo)) {
				perror("write");
				return 1;
			}
			if (verbose) {
				printf("Received command: 'getinfo'\n");
				printf("\t Replied with %s\n", xvcInfo);
			}
		} else if (memcmp(cmd, "se", 2) == 0) {
			if (s->recvn(cmd, 9) != 9)
				return 1;
			memcpy(result, cmd + 5, 4);
			if (s->send((char*)result, 4) != 4) {
				perror("write");
				return 1;
			}
			if (verbose) {
				printf("Received command: 'settck'\n");
				printf("\t Replied with '%.*s'\n\n", 4, cmd + 5);
			}
		} else if (memcmp(cmd, "sh", 2) == 0) {
			if (s->recvn(cmd, 4) != 4)
				return 1;
			if (verbose) {
				printf("Received command: 'shift'\n");
			}

			int len;
			if (s->recvn((char*)&len, 4) != 4) {
				fprintf(stderr, "reading length failed\n");
				return 1;
			}

			int nr_bytes = (len + 7) / 8;
			if (nr_bytes * 2 > sizeof(buffer)) {
				fprintf(stderr, "buffer size exceeded\n");
				return 1;
			}

			if (s->recvn((char*)buffer, nr_bytes * 2) != nr_bytes * 2) {
				fprintf(stderr, "reading data failed\n");
				return 1;
			}
			memset(result, 0, nr_bytes);

			if (verbose) {
				printf("\tNumber of Bits  : %d\n", len);
				printf("\tNumber of Bytes : %d \n", nr_bytes);
				printf("\n");
			}

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

					if (verbose) {
						printf("LEN : 0x%08x\n", 32);
						printf("TMS : 0x%08x\n", tms);
						printf("TDI : 0x%08x\n", tdi);
						printf("TDO : 0x%08x\n", tdo);
					}

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

					if (verbose) {
						printf("LEN : 0x%08x\n", bitsLeft);
						printf("TMS : 0x%08x\n", tms);
						printf("TDI : 0x%08x\n", tdi);
						printf("TDO : 0x%08x\n", tdo);
					}
					break;
				}
			}

			if (s->send((char*)result, nr_bytes) != nr_bytes) {
				perror("write");
				return 1;
			}
		} else {
			fprintf(stderr, "invalid cmd '%s'\n", cmd);
			return 1;
		}
	} while (1);
	/* Note: Need to fix JTAG state updates, until then no exit is allowed */
}

