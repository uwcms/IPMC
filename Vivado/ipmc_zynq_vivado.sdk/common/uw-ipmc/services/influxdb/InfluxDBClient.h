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
#include <xadcps.h>

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
	std::string measurement();

	void dtask();

	void startd();
	void stopd();

	void register_console_commands(CommandParser &parser, const std::string &prefix="");
	void deregister_console_commands(CommandParser &parser, const std::string &prefix="");


	XAdcPs xadc;

	typedef struct {
		char host[32];
		uint16_t port;
		uint16_t interval; // in seconds
		bool startOnBoot : 1;
	} InfluxDBClient_Config;

protected:
	InfluxDBClient_Config *config;
	int sockfd;

	LogTree &logtree; ///< Log target

public:
	void set_configuration(const InfluxDBClient_Config &c);
	inline const InfluxDBClient_Config* read_configuration() {
		return this->config;
	}
};

#endif /* SRC_COMMON_UW_IPMC_SERVICES_INFLUXDBCLIENT_INFLUXDBCLIENT_H_ */
