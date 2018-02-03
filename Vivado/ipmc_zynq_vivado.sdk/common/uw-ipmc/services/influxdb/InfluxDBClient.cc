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

	// create the socket
	sockfd = lwip_socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		logtree.log("Failed to create socket", LogTree::LOG_ERROR);
	}

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
}

void InfluxDBClient::write(std::string database, std::string message) {
	// check if connection is establish is open
	if (sockfd < 0) {
		logtree.log("Not connected", LogTree::LOG_ERROR);
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
		if (bytes < 0)
			logtree.log("Failed to write to socket", LogTree::LOG_ERROR);
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
		if (bytes < 0)
			logtree.log("Failed to read response from socket", LogTree::LOG_ERROR);
		if (bytes == 0)
			break;
		received+=bytes;
	} while (received < total);

	if (received == total)
		logtree.log("Error storing complete response from socket", LogTree::LOG_ERROR);

	logtree.log(stdsprintf("Response:\n%s",response), LogTree::LOG_NOTICE);
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
		"%post $database $message\n"
		"\n"
		"Posts a new entry to the InfluxDB database.\n";

static void consolecmd_post(InfluxDBClient &influxdb, std::function<void(std::string)> print, const CommandParser::CommandParameters &parameters) {
	std::string database, message;

	if (!parameters.parse_parameters(1, true, &database, &message)) {
		print("Invalid parameters.  See help.\n");
		return;
	}

	influxdb.write(database, message);
}

static const std::string consolecmd_udphello_helptext =
		"%sping $url \n"
		"\n"
		"Sent a hello message through a UDP port.\n";

static void consolecmd_udphello(InfluxDBClient &influxdb, std::function<void(std::string)> print, const CommandParser::CommandParameters &parameters) {
	std::string url;
	int port;
	int socket_fd;
	struct hostent *server;
	int serverlen, n;
	struct sockaddr_in serveraddr;
	char msg[] = "Hello from IPMC!";

	if (!parameters.parse_parameters(1, true, &url, &port)) {
		print("Invalid parameters.  See help.\n");
		return;
	}
	print("Pinging " + url + "...\n");

	/* gethostbyname: get the server's DNS entry */
	server = lwip_gethostbyname(url.c_str());
	if (server == NULL) {
		print("ERROR, no such host as " + url + "\n");
		return;
	}

	/* build the server's Internet address */
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	memcpy(&serveraddr.sin_addr.s_addr, server->h_addr, server->h_length);
	serveraddr.sin_port = htons(port);

	/* socket: create the socket */
	socket_fd = lwip_socket(AF_INET, SOCK_STREAM, 0);
	if (socket_fd < 0) {
		print("Failed to create socket.");
	}

	/* send the message to the server */
	serverlen = sizeof(serveraddr);
	n = lwip_sendto(socket_fd, msg, strlen(msg), 0, (struct sockaddr*)&serveraddr, serverlen);
	if (n < 0) {
		print("ERROR in sendto");
	}

	lwip_close(socket_fd);
}

static const std::string consolecmd_tcphello_helptext =
		"%sping $url \n"
		"\n"
		"Sent a hello message through a TCP port.\n";

static void consolecmd_tcphello(InfluxDBClient &influxdb, std::function<void(std::string)> print, const CommandParser::CommandParameters &parameters) {
	std::string url;
	int port;
	int socket_fd;
	struct hostent *server;
	int serverlen, n;
	struct sockaddr_in serveraddr;
	char msg[] = "Hello from IPMC!";

	if (!parameters.parse_parameters(1, true, &url, &port)) {
		print("Invalid parameters.  See help.\n");
		return;
	}
	print("Pinging " + url + "...\n");

	/* gethostbyname: get the server's DNS entry */
	server = lwip_gethostbyname(url.c_str());
	if (server == NULL) {
		print("ERROR, no such host as " + url + "\n");
		return;
	}

	/* build the server's Internet address */
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	memcpy(&serveraddr.sin_addr.s_addr, server->h_addr, server->h_length);
	serveraddr.sin_port = htons(port);

	/* socket: create the socket */
	socket_fd = lwip_socket(AF_INET, SOCK_STREAM, 0);
	if (socket_fd < 0) {
		print("Failed to create socket.");
	}

	/* connect to server */
	serverlen = sizeof(serveraddr);
	n = lwip_connect(socket_fd, (struct sockaddr*)&serveraddr, serverlen);
	if (n < 0) {
		print("Failed to connect to host.");
	}

	/* send the message to the server */
	n = lwip_write(socket_fd, msg, strlen(msg));
	if (n < 0) {
		print("ERROR in send");
	}

	lwip_close(socket_fd);
}

/**
 * Register console commands related to this storage.
 * @param parser The command parser to register to.
 * @param prefix A prefix for the registered commands.
 */
void InfluxDBClient::register_console_commands(CommandParser &parser, const std::string &prefix) {
	parser.register_command(prefix + "connect", std::bind(consolecmd_connect, std::ref(*this), std::placeholders::_1, std::placeholders::_2), stdsprintf(consolecmd_connect_helptext.c_str(), prefix.c_str()));
	parser.register_command(prefix + "post", std::bind(consolecmd_post, std::ref(*this), std::placeholders::_1, std::placeholders::_2), stdsprintf(consolecmd_post_helptext.c_str(), prefix.c_str()));
}

/**
 * Unregister console commands related to this storage.
 * @param parser The command parser to unregister from.
 * @param prefix A prefix for the registered commands.
 */
void InfluxDBClient::deregister_console_commands(CommandParser &parser, const std::string &prefix) {
}

