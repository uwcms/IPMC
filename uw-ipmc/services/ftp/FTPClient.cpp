/*
 * FTPClient.cpp
 *
 *  Created on: Jul 24, 2018
 *      Author: mpv
 */

#include <IPMC.h>
#include <FreeRTOS.h>
#include <task.h>
#include <algorithm>

#include "drivers/network/ServerSocket.h"
#include <libs/ThreadingPrimitives.h>

#include "lwip/sockets.h"
#include "lwip/sys.h"

#include "FTPServer.h"

#define FTP_DEBUG

#ifdef FTP_DEBUG
#define FTP_DBG_PRINTF(...) printf(__VA_ARGS__)
#else
#define FTP_DBG_PRINTF(...)
#endif


#define EOL "\r\n"

std::map<uint16_t, std::string> FTPClient::FTPCodes = {
	{110, "Restart marker reply."},
	{120, "Service ready in nnn minutes."},
	{125, "Data connection already open; transfer starting."},
	{150, "File status okay; about to open data connection."},
	{200, "Command okay."},
	{202, "Command not implemented, superfluous at this site."},
	{211, "System status, or system help reply."},
	{212, "Directory status."},
	{213, "File status."},
	{214, "Help message."},
	{215, "NAME system type."},
	{220, "Service ready for new user."},
	{221, "Service closing control connection."},
	{225, "Data connection open; no transfer in progress."},
	{226, "Closing data connection."},
	{227, "Entering Passive Mode (h1,h2,h3,h4,p1,p2)."},
	{230, "User logged in, proceed."},
	{250, "Requested file action okay, completed."},
	{257, "$PATHNAME created."},
	{331, "User name okay, need password."},
	{332, "Need account for login."},
	{350, "Requested file action pending further information."},
	{421, "Service not available, closing control connection."},
	{425, "Can't open data connection."},
	{426, "Connection closed; transfer aborted."},
	{450, "Requested file action not taken."},
	{451, "Requested action aborted: local error in processing."},
	{452, "Requested action not taken."},
	{500, "Syntax error, command unrecognized."},
	{501, "Syntax error in parameters or arguments."},
	{502, "Command not implemented."},
	{503, "Bad sequence of commands."},
	{504, "Command not implemented for that parameter."},
	{530, "Not logged in."},
	{532, "Need account for storing files."},
	{550, "Requested action not taken."},
	{551, "Requested action aborted: page type unknown."},
	{552, "Requested file action aborted."},
	{553, "Requested action not taken."},
};


bool FTPServer_TerminateCommandString(char* str, size_t length) {
	// Detect EOL and terminate string with \0
	const size_t eol_size = sizeof(EOL) - 1; // Ignore last \0

	if (length >= eol_size) {
		if (strncmp(str + length - eol_size, EOL, eol_size) != 0) {
			return false; // EOL not detected
		}
		str[length - eol_size] = '\0'; // Terminate string
		return true; // EOL detected
	}

	return false; // String too short
}

bool FTPServer_SplitCommandString(char* str, char** cmd, char** args) {
	size_t i = 0;

	while (1) {
		if (str[i] == ' ') {
			str[i] = '\0';
			*args = str + i + 1;
			break;
		} else if (str[i] == '\0') {
			*args = NULL;
			break;
		}

		i++;
	}

	*cmd = str;
	return true; // Never fails
}

FTPClient::FTPClient(std::shared_ptr<Socket> socket)
: socket(socket), state(FTP_ST_LOGIN_USER) {
	int r = 0;
	char *buf = new char[1500];

	fd_set fds;
	struct timeval timeout = {10, 0}; // 10 seconds

	FD_ZERO(&fds);
	FD_SET(*this->socket, &fds);

	FTP_DBG_PRINTF("New FTP client, sending 220\n");
	this->socket->send(buildReply(220));

	while (1) {
		// Reset the timeout
		timeout.tv_sec = 30;
		timeout.tv_usec = 0;

		r = lwip_select(*this->socket+1, &fds, NULL, NULL, &timeout);

		if (r > 0) {
			if (FD_ISSET(*this->socket, &fds)) {
				// Command socket
				char *cmd, *args;
				int rbytes = socket->read(buf, 1500);

				if (rbytes <= 0) {
					FTP_DBG_PRINTF("Client disconnected or error, exiting\n");
					break;
				}

				// Parse the command
				if (!FTPServer_TerminateCommandString(buf, rbytes) ||
					!FTPServer_SplitCommandString(buf, &cmd, &args)) {
					FTP_DBG_PRINTF("Strange command format, not EOL\n");
					this->socket->send(buildReply(500)); // Command not recognized
					break;
				}

				// Convert cmd to uppercase, just in case
				for (size_t i = 0; cmd[i] != '\0'; i++) cmd[i] = toupper(cmd[i]);

				// Check if the command is supported
				FTPCommand ftpcmd;
				try {
					ftpcmd = FTPCommands.at(cmd);
				} catch (std::out_of_range const& exc) {
					// Invalid or command not implemented
					FTP_DBG_PRINTF("Command unrecognized (%s)\n", cmd);
					socket->send(buildReply(500)); // Command unrecognized
					continue;
				}

				// Check if the command can run on the current state
				if (!((ftpcmd.second.size()) != 0 && (ftpcmd.second[0] == FTP_ST_ANY)) &&
					(std::find(ftpcmd.second.begin(), ftpcmd.second.end(), state) == ftpcmd.second.end())) {
					// Command cannot run in this state
					FTP_DBG_PRINTF("Command cannot run in this state (%s, %d)\n", cmd, state);
					socket->send(buildReply(503)); // Bad sequence
					continue;
				}

				// Finally, run the command
				if (!ftpcmd.first(*this, cmd, args)) {
					// Something went wrong with the command, terminate
					break;
				}
			}
		} else if (r == 0) {
			// Timeout, terminate the service
			FTP_DBG_PRINTF("Timeout, disconnecting\n");
			this->socket->send(buildReply(221));
			break;
		} else {
			// Error
			FTP_DBG_PRINTF("Error (errno=%d)\n", errno);
			break;
		}

	};

	FTP_DBG_PRINTF("Closing connection\n");
	delete buf;
}

std::string FTPClient::buildReply(uint16_t code) {
	return std::to_string(code) + " " + FTPCodes[code] + EOL;
}


bool FTPClient::CommandNotImplemented(FTPClient &client, char *cmd, char *args) {
	FTP_DBG_PRINTF("Command not implemented (%s)\n", cmd);
	client.socket->send(buildReply(502)); // Not implemented
	return true;
}

bool FTPClient::CommandUSER(FTPClient &client, char *cmd, char* user) {
	// Possible replies: 120->220, 220, 421
	FTP_DBG_PRINTF("User is %s\n", user);
	client.socket->send(buildReply(331)); // Password required
	client.state = FTP_ST_LOGIN_PASS;
	return true;
}

bool FTPClient::CommandPASS(FTPClient &client, char *cmd, char* pass) {
	// Possible replies: 230, 530, 500, 501, 421, 331, 332
	FTP_DBG_PRINTF("Pass is %s\n", pass);
	client.socket->send(buildReply(230)); // Logged in
	client.state = FTP_ST_IDLE;
	return true;
}

bool FTPClient::CommandQUIT(FTPClient &client, char *cmd, char* args) {
	// Possible replies: 221, 500
	FTP_DBG_PRINTF("Quitting\n");
	client.socket->send(buildReply(221)); // Closing connection
	client.state = FTP_ST_IDLE;
	return false;
}

bool FTPClient::CommandPORT(FTPClient &client, char *cmd, char* args) {
	// Possible replies: 200, 500, 501, 421, 530
	FTP_DBG_PRINTF("Setting port to %s\n", args);
	uint8_t addr[4], port[2];
	if (sscanf(args, "%hhu,%hhu,%hhu,%hhu,%hhu,%hhu",
			&(addr[3]), &(addr[2]), &(addr[1]), &(addr[0]),
			&(port[1]), &(port[0])) != 6) {
		// Incorrect arguments
		client.socket->send(buildReply(501)); // Invalid parameters
	} else {
		client.socket->send(buildReply(200)); // Okay
	}
	return true;
}

std::map<std::string, FTPClient::FTPCommand> FTPClient::FTPCommands = {
	// Minimal implementation
	{"USER", {FTPClient::CommandUSER, {FTP_ST_LOGIN_USER}}},
	{"PASS", {FTPClient::CommandPASS, {FTP_ST_LOGIN_PASS}}},
	{"QUIT", {FTPClient::CommandQUIT, {FTP_ST_IDLE}}},
	{"PORT", {FTPClient::CommandPORT, {FTP_ST_IDLE}}},
	{"TYPE", {FTPClient::CommandNotImplemented, {FTP_ST_ANY}}},
	{"MODE", {FTPClient::CommandNotImplemented, {FTP_ST_ANY}}},
	{"STRU", {FTPClient::CommandNotImplemented, {FTP_ST_ANY}}},
	{"RETR", {FTPClient::CommandNotImplemented, {FTP_ST_ANY}}},
	{"STOR", {FTPClient::CommandNotImplemented, {FTP_ST_ANY}}},
	{"NOOP", {FTPClient::CommandNotImplemented, {FTP_ST_ANY}}},

	// Extended implementation
	{"SYST", {FTPClient::CommandNotImplemented, {FTP_ST_ANY}}},
	{"PASV", {FTPClient::CommandNotImplemented, {FTP_ST_ANY}}},
};

