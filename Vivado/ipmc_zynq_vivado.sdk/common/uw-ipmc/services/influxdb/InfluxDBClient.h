/*
 * InfluxDBClient.h
 *
 *  Created on: Jan 31, 2018
 *      Author: mpv
 */

#ifndef SRC_COMMON_UW_IPMC_SERVICES_INFLUXDBCLIENT_INFLUXDBCLIENT_H_
#define SRC_COMMON_UW_IPMC_SERVICES_INFLUXDBCLIENT_INFLUXDBCLIENT_H_

#include <FreeRTOS.h>
#include <IPMC.h>

/**
 * TBD
 *
 * TBD
 */
class InfluxDBClient {
public:
	InfluxDBClient(LogTree &logtree);
	virtual ~InfluxDBClient();

	void connect(std::string host, int port);
	void write(std::string database, std::string message);

	void register_console_commands(CommandParser &parser, const std::string &prefix="");
	void deregister_console_commands(CommandParser &parser, const std::string &prefix="");

protected:
	int sockfd;
	std::string host;
	int port;

	LogTree &logtree; ///< Log target
};

#endif /* SRC_COMMON_UW_IPMC_SERVICES_INFLUXDBCLIENT_INFLUXDBCLIENT_H_ */
