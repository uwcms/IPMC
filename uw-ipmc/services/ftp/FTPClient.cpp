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
#include "drivers/network/ClientSocket.h"
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

// TODO: I believe there is a problem with keeping client.dir because if a directory
// gets removed and there is a client pointing to the position it can cause a segfault.
// Maybe just use paths with strings all the time? FTPServer::get(..)

std::vector<std::string> stringSplit(std::string str, char delimiter) {
	std::vector<std::string> v;

	size_t pos = 0, end;

	do {
		end = str.find(delimiter, pos);
		std::string ss = str.substr(pos, end);
		pos = end + 1; // Skip the delimiter

		if (ss.length() > 0) v.push_back(ss);
	} while (end != std::string::npos);

	return v;
}


FTPFile::FTPDirContents* FTPServer::getDirContentsFromPath(std::string path, FTPFile::FTPDirContents *curDir) {
	// If first character is '/' then we are in root
	if (path[0] == '/') {
		curDir = &(FTPServer::files);
		path = path.substr(1);
	}

	std::vector<std::string> pathtree = stringSplit(path, '/');

	for (auto it : pathtree) {
		try {
			FTPFile& f = curDir->at(it);

			if (f.isDirectory) curDir = &(f.contents);
			else return NULL; // Not a directory
		} catch (std::out_of_range& e) {
			// Doesn't exist
			return NULL;
		}
	}

	return curDir;
}

FTPFile* FTPServer::getFileFromPath(std::string filepath, FTPFile::FTPDirContents *curDir) {
	if (filepath[0] == '/') {
		// File path is pointing to root
		curDir = &(FTPServer::files);
		filepath = filepath.substr(1);
	}

	std::vector<std::string> pathtree = stringSplit(filepath, '/');
	size_t treesize = pathtree.size();
	if (treesize == 0) return NULL;

	for (size_t i = 0; i < treesize; i++) {
		try {
			FTPFile* f = &(curDir->at(pathtree[i]));

			if (i == (treesize - 1)) {
				// If this is the last item on the list then it must be the file
				if (f->isDirectory) return NULL;
				else return f;
			} else {
				if (f->isDirectory) curDir = &(f->contents);
				else return NULL;
			}
		} catch (std::out_of_range& e) {
			// Doesn't exist
			return NULL;
		}
	}

	return NULL;
}


// TODO: Welcome message?

FTPFile::FTPDirContents FTPServer::files = {};

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
	{450, "Requested file action not taken. File unavailable (e.g., file busy)."},
	{451, "Requested action aborted: local error in processing."},
	{452, "Requested action not taken. Insufficient storage space in system."},
	{500, "Syntax error, command unrecognized."},
	{501, "Syntax error in parameters or arguments."},
	{502, "Command not implemented."},
	{503, "Bad sequence of commands."},
	{504, "Command not implemented for that parameter."},
	{530, "Not logged in."},
	{532, "Need account for storing files."},
	{550, "Requested action not taken. File unavailable (e.g., file not found, no access)."},
	{551, "Requested action aborted: page type unknown."},
	{552, "Requested file action aborted. Exceeded storage allocation (for current directory or dataset)."},
	{553, "Requested action not taken. File name not allowed."},
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
: socket(socket), data(nullptr), state(FTP_ST_LOGIN_USER), path("/"), dir(&(FTPServer::files)),
  file(NULL), filebuf(nullptr), filebuf_len(0) {
	const size_t max_pkt_size = TCP_MSS;
	int r = 0;
	char *buf = new char[max_pkt_size];

	fd_set fds, fderrs;
	struct timeval timeout;

	FTP_DBG_PRINTF("New FTP client, sending 220\n");
	this->socket->send(buildReply(220));

	while (1) {
		// Reset the timeout
		timeout.tv_sec = 60;
		timeout.tv_usec = 0;

		// FD bits get reset every time, reinitialize
		FD_ZERO(&fds);
		FD_SET(*this->socket, &fds);

		FD_ZERO(&fderrs);
		FD_SET(*this->socket, &fderrs);

		int maxfd = *this->socket;
		if (this->data.get()) {
			FD_SET(*this->data, &fds);
			FD_SET(*this->data, &fderrs);
			if (*this->data > *this->socket)
				maxfd = *this->data;
		}

		//TODO: Why does this doesnt work:
		//r = lwip_select(maxfd+1, &fds, NULL, &fderrs, &timeout);
		r = lwip_select(maxfd+1, &fds, NULL, NULL, &timeout);

		if (r > 0) {
			if (FD_ISSET(*this->socket, &fds)) {
				// Command socket
				char *cmd, *args;
				int rbytes = this->socket->read(buf, max_pkt_size);

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
					this->socket->send(buildReply(500)); // Command unrecognized
					continue;
				}

				// Check if the command can run on the current state
				if (!((ftpcmd.second.size()) != 0 && (ftpcmd.second[0] == FTP_ST_ANY)) &&
					(std::find(ftpcmd.second.begin(), ftpcmd.second.end(), state) == ftpcmd.second.end())) {
					// Command cannot run in this state
					FTP_DBG_PRINTF("Command cannot run in this state (cmd=%s, state=%d)\n", cmd, state);
					this->socket->send(buildReply(503)); // Bad sequence
					continue;
				}

				// Finally, run the command
				if (!ftpcmd.first(*this, cmd, args)) {
					// Something went wrong with the command, terminate
					break;
				}
			} else if ((*this->data) && FD_ISSET(*this->data, &fds)) {
				// Data being received
				if ((this->state != FTP_ST_STOR) || (!this->filebuf.get())) {
					// This is enough reason to terminate FTP, we shouldn't be here!
					this->socket->send(buildReply(221));
					break;
				}

				int rbytes = this->data->read(buf, max_pkt_size);
				if (rbytes == 0) {
					// Connection gracefully closed, end of file transfer
					FTP_DBG_PRINTF("Received %u bytes\n", this->filebuf_len);
					this->data->close();

					if (this->file->writefile(this->filebuf.get(), this->filebuf_len) != this->filebuf_len) {
						// Fail to apply file
						this->socket->send(buildReply(450, "Unable to fully write file")); // Failed
					} else {
						this->socket->send(buildReply(250)); // Okay
					}

					this->filebuf = NULL;
					this->state = FTP_ST_IDLE;
				} else if (rbytes < 0) {
					// Socket error
					this->filebuf = NULL;
					this->data->close();
					this->socket->send(buildReply(426)); // Connection closed
					this->state = FTP_ST_IDLE;
				} else {
					// Bytes received
					// Make sure there is enough space in the buffer
					if (this->filebuf_len + rbytes > FTP_MAX_PREALLOC) {
						// Not enough space! Abort..
						this->filebuf = NULL;
						this->data->close();
						this->socket->send(buildReply(552)); // Not enough space
						this->state = FTP_ST_IDLE;
					} else {
						memcpy(this->filebuf.get() + filebuf_len, buf, rbytes);
						filebuf_len += rbytes;
					}
				}
			} /*else if ((*this->data) && FD_ISSET(*this->data, &fderrs)) {
				// Data socket error
				FTP_DBG_PRINTF("Data socket might have lost a packet!\n");
			}*/
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

std::string FTPClient::buildReply(uint16_t code, std::string msg) {
	return std::to_string(code) + " " + msg + EOL;
}


bool FTPClient::CommandNotImplemented(FTPClient &client, char *cmd, char *args) {
	FTP_DBG_PRINTF("Command not implemented (cmd=%s, args=%s)\n", cmd, args);
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
	uint8_t a[4], p[2];
	if (sscanf(args, "%hhu,%hhu,%hhu,%hhu,%hhu,%hhu",
			&(a[3]), &(a[2]), &(a[1]), &(a[0]),
			&(p[1]), &(p[0])) != 6) {
		// Incorrect arguments
		client.socket->send(buildReply(501)); // Invalid parameters
	} else {
		std::string address = "";
		address += std::to_string(a[3]) + ".";
		address += std::to_string(a[2]) + ".";
		address += std::to_string(a[1]) + ".";
		address += std::to_string(a[0]);
		unsigned short port = (p[1] << 8) + p[0];

		try {
		client.data = std::shared_ptr<Socket>(new ClientSocket(address, port));
		} catch (SocketAddress::HostNotFound e) {
			FTP_DBG_PRINTF("Host not found %s:%u\n", address, port);
			client.socket->send(buildReply(501)); // Invalid parameters
		}

		client.socket->send(buildReply(200)); // Okay
	}
	return true;
}

bool FTPClient::CommandTYPE(FTPClient &client, char *cmd, char* args) {
	// Possible replies: 200, 500, 501, 504, 421, 530
	// Only support ASCII non-print for the moment
	// TODO: Is Image also supported as ASCII?
	if (args) {
		if (args[0] == 'A') {
			// ASCII non-print
			client.socket->send(buildReply(200)); // Okay
		} else if (args[0] == 'I') {
			// Image
			client.socket->send(buildReply(200)); // Okay
		} else {
			// Others
			client.socket->send(buildReply(504)); // Not implemented for arguments
		}
	} else {
		// Wrong arguments
		client.socket->send(buildReply(501)); // Invalid parameters
	}
	return true;
}

bool FTPClient::CommandMODE(FTPClient &client, char *cmd, char* args) {
	// Possible replies: 200, 500, 501, 504, 421, 530
	// Only Stream mode is supported for the moment, maybe block would be interesting too
	if (args) {
		if (args[0] == 'F') {
			// File mode
			client.socket->send(buildReply(200)); // Okay
		} else {
			// Others
			client.socket->send(buildReply(504)); // Not implemented for arguments
		}
	} else {
		// Wrong arguments
		client.socket->send(buildReply(501)); // Invalid parameters
	}
	return true;
}

bool FTPClient::CommandSTRU(FTPClient &client, char *cmd, char* args) {
	// Possible replies: 200, 500, 501, 504, 421, 530
	// Only File structure is supported for the moment
	if (args) {
		if (args[0] == 'F') {
			// File structure
			client.socket->send(buildReply(200)); // Okay
		} else {
			// Others
			client.socket->send(buildReply(504)); // Not implemented for arguments
		}
	} else {
		// Wrong arguments
		client.socket->send(buildReply(501)); // Invalid parameters
	}
	return true;
}

bool FTPClient::CommandSTOR(FTPClient &client, char *cmd, char* filename) {
	// Possible replies: (125, 150) -> (110, 226, 250, 425, 426, 451, 551, 552), 532, 450, 452, 553, 500, 501, 421, 530
	FTP_DBG_PRINTF("Receiving file %s\n", filename);

	if (!filename) {
		client.socket->send(buildReply(501)); // Invalid parameters
		return true;
	}

	// Check if file exists
	FTPFile *file = FTPServer::getFileFromPath(filename, client.dir);
	if (!file) {
		client.socket->send(buildReply(450, "File does not exist."));
		return true;
	}

	// Check permissions
	if (!file->writefile) {
		client.socket->send(buildReply(450, "File has no write permissions."));
		return true;
	}

	// Pre-allocate read buffer
	client.filebuf = std::unique_ptr<uint8_t>(new uint8_t[FTP_MAX_PREALLOC]);
	if (!client.filebuf.get()) {
		client.socket->send(buildReply(450, "Cannot allocate enough memory."));
		return true;
	}

	client.socket->send(buildReply(150)); // Establishing connection
	if (dynamic_cast<ClientSocket*>(client.data.get())->connect() != 0) {
		client.socket->send(buildReply(425)); // Can't connect
		return true;
	}

	client.filebuf_len = 0;
	client.file = file;

	client.state = FTP_ST_STOR;
	return true;
}

bool FTPClient::CommandRETR(FTPClient &client, char *cmd, char* filename) {
	// Possible replies: (125, 150) -> (226, 250, 425, 426, 451), 450, 500, 501, 502, 421, 530
	FTP_DBG_PRINTF("Sending file %s\n", filename);

	if (!filename) {
		client.socket->send(buildReply(501)); // Invalid parameters
		return true;
	}

	// Check if file exists
	FTPFile *file = FTPServer::getFileFromPath(filename, client.dir);
	if (!file) {
		client.socket->send(buildReply(450, "File does not exist."));
		return true;
	}

	// Check permissions
	if (!file->readfile) {
		client.socket->send(buildReply(450, "File has no read permissions."));
		return true;
	}

	// Pre-allocate read buffer
	std::unique_ptr<uint8_t> buf = std::unique_ptr<uint8_t>(new uint8_t[file->size]);
	if (!buf.get()) {
		client.socket->send(buildReply(450, "Cannot allocate enough memory."));
		return true;
	}

	// Copy the file to memory
	if (file->readfile(buf.get()) != file->size) {
		client.socket->send(buildReply(450, "Cannot read file."));
		return true;
	}

	client.socket->send(buildReply(150)); // Establishing connection
	if (dynamic_cast<ClientSocket*>(client.data.get())->connect() != 0) {
		client.socket->send(buildReply(425)); // Can't connect
		return true;
	}

	// Send the file
	// TODO: For some reason a direct call with file->size blocks and nothing is sent, so break it down
	// TODO: Maybe have this in the select() loop
	const size_t blocksize = 1024;
	size_t pagecnt = file->size / blocksize + 1;
	size_t rem = file->size % blocksize;
	bool problem = false;
	for (size_t i = 0; i < pagecnt; i++) {
		int len = (i == pagecnt-1)? rem : blocksize;
		if (client.data->send(buf.get() + i * blocksize, len) != len) {
			problem = true;
			break;
		}
	}

	//client.data->send(buf.get(), file->size);

	client.data->close();

	if (problem) {
		client.socket->send(buildReply(426)); // Abort
	} else {
		client.socket->send(buildReply(250)); // Okay
	}

	return true;
}

bool FTPClient::CommandNOOP(FTPClient &client, char *cmd, char* args) {
	// Possible replies: 200, 500, 421
	FTP_DBG_PRINTF("Noop\n");
	client.socket->send(buildReply(200)); // Okay
	return true;
}

bool FTPClient::CommandPASS(FTPClient &client, char *cmd, char* pass) {
	// Possible replies: 230, 530, 500, 501, 421, 331, 332
	FTP_DBG_PRINTF("Pass is %s\n", pass);
	client.socket->send(buildReply(230)); // Logged in
	client.state = FTP_ST_IDLE;
	return true;
}

bool FTPClient::CommandPWD(FTPClient &client, char *cmd, char* args) {
	// Possible replies: 257, 500, 501, 502, 421, 550
	client.socket->send(buildReply(257, client.path));
	return true;
}

bool FTPClient::CommandLIST(FTPClient &client, char *cmd, char* path) {
	// Possible replies: (125, 150) -> (226, 250, 425, 426, 451), 450, 500, 501, 502, 421, 530
	FTP_DBG_PRINTF("List %s\n", path);

	FTPFile::FTPDirContents *contents = NULL;

	if (path) {
		contents = FTPServer::getDirContentsFromPath(path, client.dir);
	} else {
		// No path provided
		contents = client.dir;
	}

	if (contents == NULL) {
		// Invalid path
		client.socket->send(buildReply(450)); // Requested file action not taken
		return true;
	}

	client.socket->send(buildReply(150)); // Establishing connection
	dynamic_cast<ClientSocket*>(client.data.get())->connect();

	std::string str = "";

	for (auto&& [filename, stats] : *contents) {
		if (stats.isDirectory) str += "d";
		else str += "-";

		if (stats.readfile) str += "r";
		else str += "-";

		if (stats.writefile) str += "w";
		else str += "-";

		str += "------- 1 ipmc ipmc ";
		str += std::to_string(stats.size);
		str += " Jan 1 0:0 ";
		str += filename;
		str += EOL;
	}

	client.data->send(str);
	client.data->close();
	client.socket->send(buildReply(226)); // Okay

	return true;
}

bool FTPClient::CommandCWD(FTPClient &client, char *cmd, char* path) {
	// Possible replies: 250, 500, 501, 502, 421, 530, 550
	FTP_DBG_PRINTF("CWD %s\n", path);

	if (path == NULL) {
		// No path given
		client.socket->send(buildReply(501)); // Wrong parameters
		return true;

	}

	// Check if directory exists
	FTPFile::FTPDirContents *contents = FTPServer::getDirContentsFromPath(path, client.dir);

	if (contents == NULL) {
		// Directory doesn't exist
		client.socket->send(buildReply(501)); // Wrong parameters
		return true;
	}

	if (path[0] == '/') client.path = path;
	else {
		if (client.path.back() != '/')
			client.path += std::string("/") + path;
		else
			client.path += path;
	}
	client.dir = contents;
	client.socket->send(buildReply(200, "Changed to directory " + client.path)); // Okay

	return true;
}

std::map<std::string, FTPClient::FTPCommand> FTPClient::FTPCommands = {
	{"USER", {FTPClient::CommandUSER, {FTP_ST_LOGIN_USER}}},
	{"PASS", {FTPClient::CommandPASS, {FTP_ST_LOGIN_PASS}}},
	{"ACCT", {FTPClient::CommandNotImplemented, {FTP_ST_IDLE}}},
	{"CWD",  {FTPClient::CommandCWD,  {FTP_ST_IDLE}}},
	{"CDUP", {FTPClient::CommandNotImplemented, {FTP_ST_IDLE}}},
	{"SMNT", {FTPClient::CommandNotImplemented, {FTP_ST_IDLE}}},
	{"QUIT", {FTPClient::CommandQUIT, {FTP_ST_IDLE}}},
	{"REIN", {FTPClient::CommandNotImplemented, {FTP_ST_IDLE}}},
	{"PORT", {FTPClient::CommandPORT, {FTP_ST_IDLE}}},
	{"PASV", {FTPClient::CommandNotImplemented, {FTP_ST_IDLE}}},
	{"TYPE", {FTPClient::CommandTYPE, {FTP_ST_IDLE}}},
	{"STRU", {FTPClient::CommandSTRU, {FTP_ST_IDLE}}},
	{"MODE", {FTPClient::CommandMODE, {FTP_ST_IDLE}}},
	{"RETR", {FTPClient::CommandRETR, {FTP_ST_IDLE}}},
	{"STOR", {FTPClient::CommandSTOR, {FTP_ST_IDLE}}},
	{"STOU", {FTPClient::CommandNotImplemented, {FTP_ST_IDLE}}},
	{"APPE", {FTPClient::CommandNotImplemented, {FTP_ST_IDLE}}},
	{"ALLO", {FTPClient::CommandNotImplemented, {FTP_ST_IDLE}}},
	{" R",   {FTPClient::CommandNotImplemented, {FTP_ST_IDLE}}},
	{"REST", {FTPClient::CommandNotImplemented, {FTP_ST_IDLE}}},
	{"RNFR", {FTPClient::CommandNotImplemented, {FTP_ST_IDLE}}},
	{"RNTO", {FTPClient::CommandNotImplemented, {FTP_ST_IDLE}}},
	{"ABOR", {FTPClient::CommandNotImplemented, {FTP_ST_IDLE}}},
	{"DELE", {FTPClient::CommandNotImplemented, {FTP_ST_IDLE}}},
	{"RMD",  {FTPClient::CommandNotImplemented, {FTP_ST_IDLE}}},
	{"MKD",  {FTPClient::CommandNotImplemented, {FTP_ST_IDLE}}},
	{"PWD",  {FTPClient::CommandPWD,  {FTP_ST_IDLE}}},
	{"LIST", {FTPClient::CommandLIST,  {FTP_ST_IDLE}}},
	{"NLST", {FTPClient::CommandNotImplemented, {FTP_ST_IDLE}}},
	{"SITE", {FTPClient::CommandNotImplemented, {FTP_ST_IDLE}}},
	{"SYST", {FTPClient::CommandNotImplemented, {FTP_ST_IDLE}}},
	{"STAT", {FTPClient::CommandNotImplemented, {FTP_ST_IDLE}}},
	{"HELP", {FTPClient::CommandNotImplemented, {FTP_ST_IDLE}}},
	{"NOOP", {FTPClient::CommandNOOP, {FTP_ST_IDLE}}}, // TODO: Should this be allowed before login? Don't think so
};

