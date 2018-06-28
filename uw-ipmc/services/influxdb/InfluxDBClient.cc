/*
 * InfluxDB.cpp
 *
 *  Created on: Jan 31, 2018
 *      Author: jtikalsky
 */

#define __INFLUXDBCLIENT_CPP
#include <services/influxdb/InfluxDBClient.h>
#include <services/persistentstorage/PersistentStorage.h>
#include <FreeRTOS.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <lwip/err.h>
#include <lwip/dns.h>
#include <semphr.h>
#include <task.h>
#include <event_groups.h>
#include <services/console/CommandParser.h>
#include <services/console/ConsoleSvc.h>
#include <IPMC.h>
#include <string.h>
#include <lwip/sockets.h>



/**
 * TBD
 */
InfluxDBClient::InfluxDBClient(LogTree &logtree) :
sockfd(-1), logtree(logtree) {

	XAdcPs_Config *xadcps_config = XAdcPs_LookupConfig(XPAR_XADCPS_0_DEVICE_ID);

	XAdcPs_CfgInitialize(&xadc, xadcps_config, xadcps_config->BaseAddress);

	uint16_t psver = persistent_storage->get_section_version(PersistentStorageAllocations::WISC_INFLUXDBCLIENT_CONFIG);
	configASSERT(psver <= 1); // TODO: Error handling: Unexpected version.

	this->config = (InfluxDBClient_Config*)persistent_storage->get_section(PersistentStorageAllocations::WISC_INFLUXDBCLIENT_CONFIG, 1, sizeof(InfluxDBClient_Config));
	configASSERT(this->config); // Despite verifying things, something is WRONG.

	if (psver == 0) {
		// Default configurations
		memset(this->config->host, '\0', sizeof(this->config->host));
		this->config->port = 8086;
		this->config->interval = 30;
		this->config->startOnBoot = false;
		persistent_storage->flush(this->config, sizeof(InfluxDBClient_Config));
	}
}

InfluxDBClient::~InfluxDBClient() {
}

void InfluxDBClient::set_configuration(const InfluxDBClient_Config &c) {
	// TODO: Mutex here!
	memcpy(this->config, &c, sizeof(InfluxDBClient_Config));
}

void InfluxDBClient::write(std::string database, std::string message) {
	// check if current socket is open, if so close it
	if (sockfd > 0) {
		lwip_close(sockfd);
	}

	// create the socket
	sockfd = lwip_socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		logtree.log("Failed to create socket", LogTree::LOG_ERROR);
	}

	// get the host's DNS entry
	struct hostent *server = lwip_gethostbyname(config->host);
	if (server == NULL) {
		logtree.log("Failed to get DNS entry for host " + std::string(config->host), LogTree::LOG_ERROR);
		return;
	}

	// build the host's internet address
	struct sockaddr_in serveraddr;
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	memcpy(&serveraddr.sin_addr.s_addr, server->h_addr, server->h_length);
	serveraddr.sin_port = htons(config->port);

	// connect to server
	int serverlen = sizeof(serveraddr);
	int n = lwip_connect(sockfd, (struct sockaddr*)&serveraddr, serverlen);
	if (n < 0) {
		logtree.log("Failed to connect to host " + std::string(config->host) + ":" + std::to_string(config->port), LogTree::LOG_ERROR);

		// close the socket
		lwip_close(sockfd);
		sockfd = -1;
		return;
	}

	// form the post message
	std::string postmsg = "";

	postmsg += "POST http://" + std::string(config->host) + ":" + std::to_string(config->port) + "/write?db=" + database + " HTTP/1.0\r\n";
	postmsg += "Content-Length: " + std::to_string(message.length()) + "\r\n";
	postmsg += "\r\n";
	postmsg += message;

	logtree.log(postmsg, LogTree::LOG_NOTICE);

	// send the request
	int total = postmsg.length();
	int sent = 0, bytes = 0;
	do {
		bytes = lwip_write(sockfd, postmsg.substr(sent).c_str(), total - sent);
		if (bytes < 0) {
			logtree.log("Failed to write to socket", LogTree::LOG_ERROR);
			return;
		}
		if (bytes == 0)
			break;
		sent += bytes;
	} while (sent < total);

	// receive the response
	char response[512] = "";
	total = sizeof(response)-1;
	int received = 0;
	do {
		bytes = lwip_read(sockfd, response + received, total - received);
		if (bytes < 0) {
			logtree.log("Failed to read response from socket", LogTree::LOG_ERROR);
			return;
		}
		if (bytes == 0)
			break;
		received += bytes;
	} while (received < total);

	if (received == total)
		logtree.log("Error storing complete response from socket", LogTree::LOG_ERROR);

	logtree.log(stdsprintf("Response:\n%s",response), LogTree::LOG_NOTICE);
}

std::string InfluxDBClient::measurement() {
	u16 temp = XAdcPs_GetAdcData(&xadc, XADCPS_CH_TEMP);
	u16 vccint = XAdcPs_GetAdcData(&xadc, XADCPS_CH_VCCINT);
	u16 vccaux = XAdcPs_GetAdcData(&xadc, XADCPS_CH_VCCAUX);
	u16 vbram = XAdcPs_GetAdcData(&xadc, XADCPS_CH_VBRAM);
	u16 vccpint = XAdcPs_GetAdcData(&xadc, XADCPS_CH_VCCPINT);
	u16 vccpaux = XAdcPs_GetAdcData(&xadc, XADCPS_CH_VCCPAUX);
	u16 vccpdr0 = XAdcPs_GetAdcData(&xadc, XADCPS_CH_VCCPDRO);

	std::string source = "testipmc";

	std::string s = "";

	s += "temp,host=" + source + " value=" + std::to_string(XAdcPs_RawToTemperature(temp)) + "\n";
	s += "vccint,host=" + source + " value=" + std::to_string(XAdcPs_RawToVoltage(vccint)) + "\n";
	s += "vccaux,host=" + source + " value=" + std::to_string(XAdcPs_RawToVoltage(vccaux)) + "\n";
	s += "vbram,host=" + source + " value=" + std::to_string(XAdcPs_RawToVoltage(vbram)) + "\n";
	s += "vccpint,host=" + source + " value=" + std::to_string(XAdcPs_RawToVoltage(vccpint)) + "\n";
	s += "vccpaux,host=" + source + " value=" + std::to_string(XAdcPs_RawToVoltage(vccpaux)) + "\n";
	s += "vccpdr0,host=" + source + " value=" + std::to_string(XAdcPs_RawToVoltage(vccpdr0)) + "\n";


	return s;
}

void InfluxDBClient::dtask() {
	const TickType_t xDelay = 20000 / portTICK_PERIOD_MS;


	while (1) {
		this->write("ipmc", this->measurement());

		vTaskDelay(xDelay);
	}
}

void InfluxDBClient::startd() {
	TaskHandle_t xHandle = NULL;

	/* Create the task, storing the handle. */
	xHandle = UWTaskCreate("influxdbd", TASK_PRIORITY_BACKGROUND, [this]() -> void { this->dtask(); });

	if( !xHandle )
	{
		/* The task was created.  Use the task's handle to delete the task. */
		logtree.log("Failed to create InfluxDB deamon task", LogTree::LOG_ERROR);
	}
}

namespace {
	/// A "config" console command.
	class InfluxDBCommand_config : public CommandParser::Command {
	public:
		InfluxDBClient &influxdb;

		/// Instantiate
		InfluxDBCommand_config(InfluxDBClient &influxdb) : influxdb(influxdb) { };

		virtual std::string get_helptext(const std::string &command) const {
			return stdsprintf(
					"%s $host $port $interval\n"
					"\n"
					"Configures the InfluxDB client.\n", command.c_str());
		}

		virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
			InfluxDBClient::InfluxDBClient_Config c;
			c.startOnBoot = false;

			std::string url;

			if (!parameters.parse_parameters(1, true, &url, &(c.port), &(c.interval))) {
				print("Invalid parameters.  See help.\n");
				return;
			}

			if (url.length() >= 32) {
				print("Host URL is too long, aborting.\n");
				return;
			}

			strcpy(c.host, url.c_str());

			influxdb.set_configuration(c);
		}

		//virtual std::vector<std::string> complete(const CommandParser::CommandParameters &parameters) const { };
	};

	/// A "read_config" console command.
	class InfluxDBCommand_read_config : public CommandParser::Command {
	public:
		InfluxDBClient &influxdb;

		/// Instantiate
		InfluxDBCommand_read_config(InfluxDBClient &influxdb) : influxdb(influxdb) { };

		virtual std::string get_helptext(const std::string &command) const {
			return stdsprintf(
					"%s\n"
					"\n"
					"Prints current configuration.\n", command.c_str());
		}

		virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
			const InfluxDBClient::InfluxDBClient_Config *c = influxdb.get_configuration();

			console->write("Current configuration for InfluxDB Client:");
			console->write("Host: " + std::string(c->host) + ":" + std::to_string(c->port));
		}

		//virtual std::vector<std::string> complete(const CommandParser::CommandParameters &parameters) const { };
	};

	/// A "measurement(" console command.
	class InfluxDBCommand_measurement : public CommandParser::Command {
	public:
		InfluxDBClient &influxdb;

		/// Instantiate
		InfluxDBCommand_measurement(InfluxDBClient &influxdb) : influxdb(influxdb) { };

		virtual std::string get_helptext(const std::string &command) const {
			return stdsprintf(
					"%s\n"
					"\n"
					"Get the current measurements.\n", command.c_str());
		}

		virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
			console->write(influxdb.measurement());
		}

		//virtual std::vector<std::string> complete(const CommandParser::CommandParameters &parameters) const { };
	};

	/// A "startd(" console command.
	class InfluxDBCommand_startd : public CommandParser::Command {
	public:
		InfluxDBClient &influxdb;

		/// Instantiate
		InfluxDBCommand_startd(InfluxDBClient &influxdb) : influxdb(influxdb) { };

		virtual std::string get_helptext(const std::string &command) const {
			return stdsprintf(
					"%s\n"
					"\n"
					"tart the deamon.\n", command.c_str());
		}

		virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
			influxdb.startd();
		}

		//virtual std::vector<std::string> complete(const CommandParser::CommandParameters &parameters) const { };
	};
};

/**
 * Register console commands related to this storage.
 * @param parser The command parser to register to.
 * @param prefix A prefix for the registered commands.
 */
void InfluxDBClient::register_console_commands(CommandParser &parser, const std::string &prefix) {
	parser.register_command(prefix + "config", std::make_shared<InfluxDBCommand_config>(*this));
	parser.register_command(prefix + "read_config", std::make_shared<InfluxDBCommand_read_config>(*this));
	parser.register_command(prefix + "measurement", std::make_shared<InfluxDBCommand_measurement>(*this));
	parser.register_command(prefix + "startd", std::make_shared<InfluxDBCommand_startd>(*this));
}

/**
 * Unregister console commands related to this storage.
 * @param parser The command parser to unregister from.
 * @param prefix A prefix for the registered commands.
 */
void InfluxDBClient::deregister_console_commands(CommandParser &parser, const std::string &prefix) {
	parser.register_command(prefix + "config", NULL);
	parser.register_command(prefix + "read_config", NULL);
	parser.register_command(prefix + "measurement", NULL);
	parser.register_command(prefix + "startd", NULL);
}

