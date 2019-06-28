/*
 * Telnet.h
 *
 *  Created on: Feb 8, 2018
 *      Author: mpv
 */

#ifndef SRC_COMMON_UW_IPMC_SERVICES_TELNET_TELNET_H_
#define SRC_COMMON_UW_IPMC_SERVICES_TELNET_TELNET_H_

#include <drivers/network/ServerSocket.h>
#include <libs/LogTree.h>

class TelnetServer {
public:
	TelnetServer(LogTree &logtree);
	virtual ~TelnetServer();

protected:
	LogTree &logtree;
	SemaphoreHandle_t connection_pool_limiter; ///< A counting semaphore to limit the simultaneous connections to a sensible amount.

public:
	void thread_telnetd(); ///< \protected Internal.
};

class TelnetClient {
public:
	TelnetClient(std::shared_ptr<Socket> s, LogTree &logtree, SemaphoreHandle_t connection_pool_limiter);

protected:
	std::shared_ptr<Socket> socket;
	LogTree &logtree;
	static uint32_t next_session_serial;
	uint32_t session_serial;
	SemaphoreHandle_t connection_pool_limiter; ///< A counting semaphore to limit the simultaneous connections to a sensible amount.
	static volatile uint64_t bad_password_pressure; ///< A timestamp for bad password lockouts.
	static uint64_t get_badpass_timeout();
	static void inc_badpass_timeout();

public:
	void thread_telnetc(); /// \protected Internal.
};


#endif /* SRC_COMMON_UW_IPMC_SERVICES_TELNET_TELNET_H_ */
