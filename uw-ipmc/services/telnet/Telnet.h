/*
 * Telnet.h
 *
 *  Created on: Feb 8, 2018
 *      Author: mpv
 */

#ifndef SRC_COMMON_UW_IPMC_SERVICES_TELNET_TELNET_H_
#define SRC_COMMON_UW_IPMC_SERVICES_TELNET_TELNET_H_

#include "drivers/network/Socket.h"

class TelnetServer {
	const unsigned int TELNET_MAX_INSTANCES = 1;

public:
	TelnetServer();

public:
	void thread_telnetd();
};

class TelnetClient {
public:
	TelnetClient(Socket *s);

private:
	Socket *socket;

public:
	void thread_telnetc();
};


#endif /* SRC_COMMON_UW_IPMC_SERVICES_TELNET_TELNET_H_ */
