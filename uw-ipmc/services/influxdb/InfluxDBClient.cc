/*
 * InfluxDB.cpp
 *
 *  Created on: Jan 31, 2018
 *      Author: jtikalsky
 */

#define __INFLUXDBCLIENT_CPP
#include <services/influxdb/InfluxDBClient.h>
#include <FreeRTOS.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <lwip/err.h>
#include <lwip/dns.h>
#include <semphr.h>
#include <task.h>
#include <event_groups.h>
#include <libs/CommandParser.h>
#include <IPMC.h>
#include <string.h>
#include <lwip/sockets.h>



/**
 * TBD
 */
InfluxDBClient::InfluxDBClient(LogTree &logtree) :
host(""), port(0), sockfd(-1), logtree(logtree) {

	XAdcPs_Config *xadcps_config = XAdcPs_LookupConfig(XPAR_XADCPS_0_DEVICE_ID);

	XAdcPs_CfgInitialize(&xadc, xadcps_config, xadcps_config->BaseAddress);

}

InfluxDBClient::~InfluxDBClient() {
}

void InfluxDBClient::connect(std::string host, int port) {
	this->host = host;
	this->port = port;

	// check if current socket is open, if so close it
	if (sockfd > 0) {
		lwip_close(sockfd);
	}
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
	struct hostent *server = lwip_gethostbyname(host.c_str());
	if (server == NULL) {
		logtree.log("Failed to get DNS entry for host " + host, LogTree::LOG_ERROR);
		return;
	}

	// build the host's internet address
	struct sockaddr_in serveraddr;
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	memcpy(&serveraddr.sin_addr.s_addr, server->h_addr, server->h_length);
	serveraddr.sin_port = htons(port);

	// connect to server
	int serverlen = sizeof(serveraddr);
	int n = lwip_connect(sockfd, (struct sockaddr*)&serveraddr, serverlen);
	if (n < 0) {
		logtree.log("Failed to connect to host " + host + ":" + std::to_string(port), LogTree::LOG_ERROR);

		// close the socket
		lwip_close(sockfd);
		sockfd = -1;
		return;
	}

	// form the post message
	std::string postmsg = "";

	postmsg += "POST http://" + this->host + ":" + std::to_string(this->port) + "/write?db=" + database + " HTTP/1.0\r\n";
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

static void run_influxdbclient_deamon(void *cb_influxdb) {
	reinterpret_cast<InfluxDBClient*>(cb_influxdb)->dtask();
}

void InfluxDBClient::startd() {
	BaseType_t xReturned;
	TaskHandle_t xHandle = NULL;

	/* Create the task, storing the handle. */
	xReturned = xTaskCreate(
					run_influxdbclient_deamon,       /* Function that implements the task. */
					"influxdbd",          /* Text name for the task. */
					UWIPMC_STANDARD_STACK_SIZE,      /* Stack size in words, not bytes. */
					(void*)this,    /* Parameter passed into the task. */
					tskIDLE_PRIORITY,/* Priority at which the task is created. */
					&xHandle );      /* Used to pass out the created task's handle. */

	if( xReturned == pdFAIL )
	{
		/* The task was created.  Use the task's handle to delete the task. */
		logtree.log("Failed to create InfluxDB deamon task", LogTree::LOG_ERROR);
	}
}

static const std::string consolecmd_connect_helptext =
		"%connect $host $port\n"
		"\n"
		"Connects to an InfluxDB server with TCP/IP.\n";

static void consolecmd_connect(InfluxDBClient &influxdb, std::function<void(std::string)> print, const CommandParser::CommandParameters &parameters) {
	std::string host;
	int port;

	if (!parameters.parse_parameters(1, true, &host, &port)) {
		print("Invalid parameters.  See help.\n");
		return;
	}

	influxdb.connect(host, port);
}

static const std::string consolecmd_post_helptext =
		"%post\n"
		"\n"
		"Post a single data point to the InfluxDB database.\n";

static void consolecmd_post(InfluxDBClient &influxdb, std::function<void(std::string)> print, const CommandParser::CommandParameters &parameters) {
	std::string database, message;

	influxdb.write("ipmc", influxdb.measurement());
}

static const std::string consolecmd_measurements_helptext =
		"%measurements\n"
		"\n"
		"Get the current measurements.\n";

static void consolecmd_measurements(InfluxDBClient &influxdb, std::function<void(std::string)> print, const CommandParser::CommandParameters &parameters) {
	print(influxdb.measurement());
}

static const std::string consolecmd_startd_helptext =
		"%startd\n"
		"\n"
		"Start the deamon.\n";

static void consolecmd_startd(InfluxDBClient &influxdb, std::function<void(std::string)> print, const CommandParser::CommandParameters &parameters) {
	influxdb.startd();
}

/**
 * Register console commands related to this storage.
 * @param parser The command parser to register to.
 * @param prefix A prefix for the registered commands.
 */
void InfluxDBClient::register_console_commands(CommandParser &parser, const std::string &prefix) {
	parser.register_command(prefix + "connect", std::bind(consolecmd_connect, std::ref(*this), std::placeholders::_1, std::placeholders::_2), stdsprintf(consolecmd_connect_helptext.c_str(), prefix.c_str()));
	parser.register_command(prefix + "post", std::bind(consolecmd_post, std::ref(*this), std::placeholders::_1, std::placeholders::_2), stdsprintf(consolecmd_post_helptext.c_str(), prefix.c_str()));
	parser.register_command(prefix + "measurements", std::bind(consolecmd_measurements, std::ref(*this), std::placeholders::_1, std::placeholders::_2), stdsprintf(consolecmd_measurements_helptext.c_str(), prefix.c_str()));
	parser.register_command(prefix + "startd", std::bind(consolecmd_startd, std::ref(*this), std::placeholders::_1, std::placeholders::_2), stdsprintf(consolecmd_startd_helptext.c_str(), prefix.c_str()));
}

/**
 * Unregister console commands related to this storage.
 * @param parser The command parser to unregister from.
 * @param prefix A prefix for the registered commands.
 */
void InfluxDBClient::deregister_console_commands(CommandParser &parser, const std::string &prefix) {
}

