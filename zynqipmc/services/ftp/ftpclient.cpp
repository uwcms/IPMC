/*
 * This file is part of the ZYNQ-IPMC Framework.
 *
 * The ZYNQ-IPMC Framework is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The ZYNQ-IPMC Framework is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the ZYNQ-IPMC Framework.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "ftp.h"
#include <algorithm>
#include <drivers/network/network.h>
#include <libs/printf.h>

// TODO: Welcome message?

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

FTPClient::FTPClient(const FTPServer &ftpserver, LogTree &log, std::shared_ptr<Socket> socket) :
ftpserver(ftpserver), log(log), username(""), socket(socket), dataserver(nullptr), data(nullptr),
state(FTP_ST_LOGIN_USER), mode(FTP_MODE_PASSIVE), curpath("/"), curfile(""), buffer({nullptr, 0}) {
	int r = 0;
	const size_t max_pkt_size = TCP_MSS;
	char *buf = new char[max_pkt_size];
	size_t buflen = 0;

	fd_set fds;
	struct timeval timeout;
	int maxfdp = -1;

	this->log.log("New FTP client, sending 220", LogTree::LOG_NOTICE);
	this->socket->send(buildReply(220));

	while (1) {
		// Reset the timeout
		timeout.tv_sec = FTP_TIMEOUT_SEC;
		timeout.tv_usec = 0;

		// FD bits get reset every time, reinitialize
		FD_ZERO(&fds);
		FD_SET(*this->socket, &fds);
		maxfdp = *this->socket;

		if (this->dataserver) {
			FD_SET(*this->dataserver, &fds);
			if (*this->dataserver > maxfdp)
				maxfdp = *this->dataserver;
		}

		if (this->data) {
			FD_SET(*this->data, &fds);
			if (*this->data > maxfdp)
				maxfdp = *this->data;
		}

		r = lwip_select(maxfdp+1, &fds, nullptr, nullptr, &timeout);

		if (r > 0) {
			if (FD_ISSET(*this->socket, &fds)) {
				// Command socket
				char *cmd, *args;
				int rbytes = this->socket->recv(buf + buflen, max_pkt_size - buflen);

				if (rbytes <= 0) {
					this->log.log(stdsprintf("Client disconnected or error (%d), exiting", rbytes), LogTree::LOG_NOTICE);
					break;
				}

				buflen += rbytes;

				// Do a few checks
				if (buflen < 3) continue; // Not enough data to actually do something
				if (buflen == max_pkt_size) break; // Buffer overflow, terminate
				if (!detectEndOfCommand(buf, buflen)) continue; // Detect and terminate end of command
				buflen = 0;

				// Parse the command
				splitCommandString(buf, &cmd, &args);

				// Convert cmd to uppercase, just in case
				for (size_t i = 0; cmd[i] != '\0'; i++) cmd[i] = toupper(cmd[i]);

				// Check if the command is supported
				FTPCommand ftpcmd;
				try {
					ftpcmd = FTPCommands.at(cmd);
				} catch (std::out_of_range const& exc) {
					// Invalid or command not implemented
					this->log.log(stdsprintf("Command unrecognized (%s)", cmd), LogTree::LOG_INFO);
					this->socket->send(buildReply(500)); // Command unrecognized
					continue;
				}

				// Check if the command can run on the current state
				if (!((ftpcmd.second.size()) != 0 && (ftpcmd.second[0] == FTP_ST_ANY)) &&
					(std::find(ftpcmd.second.begin(), ftpcmd.second.end(), state) == ftpcmd.second.end())) {
					// Command cannot run in this state
					this->log.log(stdsprintf("Command cannot run in this state (cmd=%s, state=%d)", cmd, state), LogTree::LOG_INFO);
					this->socket->send(buildReply(503)); // Bad sequence
					continue;
				}

				// Finally, run the command
				if (!ftpcmd.first(*this, cmd, args)) {
					// Something went wrong with the command, terminate
					break;
				}

			} else if ((this->dataserver) && FD_ISSET((int)(*this->dataserver), &fds)) {
				// Someone is trying to connect to the data port
				if (this->data) {
					// There is already someone connected to it or not at the correct state hence refuse connection
					this->dataserver->accept(); // Not assigning it to anything will cause the shared_ptr to be destroyed, C++ magic!
					this->log.log("Incoming connection refused, only one allowed at a time", LogTree::LOG_WARNING);
				}

				// Accept connection
				this->data = this->dataserver->accept();
				this->log.log("Data connection established", LogTree::LOG_NOTICE);

				if (this->state == FTP_ST_RETR) {
					// Send the file
					bool notsent = !this->sendData();

					// Close the data connection
					this->data = nullptr;

					if (notsent) {
						this->socket->send(buildReply(426)); // Abort
					} else {
						this->socket->send(buildReply(250)); // Okay
					}

					this->state = FTP_ST_IDLE;
				}
			} else if ((this->data) && FD_ISSET((int)(*this->data), &fds)) {
				// Data being received
				if ((this->state != FTP_ST_STOR) || (!this->buffer.ptr)) {
					// This is enough reason to terminate FTP, we shouldn't be here!
					this->socket->send(buildReply(221));
					break;
				}

				int rbytes = this->data->recv(buf, max_pkt_size);
				if (rbytes == 0) {
					// Connection gracefully closed, end of file transfer
					this->log.log(stdsprintf("Received %u bytes", this->buffer.len), LogTree::LOG_NOTICE);
					this->data = nullptr;

					VFS::File* file = VFS::getFileFromPath(this->curfile);

					try {
						if (file && file->write &&
							(file->write(this->buffer.ptr.get(), this->buffer.len) != this->buffer.len)) {
							// Fail to apply file
							this->socket->send(buildReply(450, "Unable to fully write file")); // Failed
						} else {
							this->socket->send(buildReply(250)); // Okay
						}
					}
					catch (std::exception &e) {
						this->socket->send(buildReply(450, e.what())); // Failed
					}

					this->buffer.ptr = nullptr;
					this->state = FTP_ST_IDLE;
				} else if (rbytes < 0) {
					// Socket error
					this->buffer.ptr = nullptr;
					this->data->close();
					this->socket->send(buildReply(426)); // Connection closed
					this->state = FTP_ST_IDLE;
				} else {
					// Bytes received
					// Make sure there is enough space in the buffer
					if (this->buffer.len + rbytes > FTP_MAX_PREALLOC) {
						// Not enough space! Abort..
						this->buffer.ptr = nullptr;
						this->data->close();
						this->socket->send(buildReply(552)); // Not enough space
						this->state = FTP_ST_IDLE;
					} else {
						memcpy(this->buffer.ptr.get() + this->buffer.len, buf, rbytes);
						this->buffer.len += rbytes;
					}
				}
			}
		} else if (r == 0) {
			// Timeout, terminate the service
			this->log.log("Timeout, disconnecting client", LogTree::LOG_WARNING);
			this->socket->send(buildReply(221));
			break;
		} else {
			// Error
			this->log.log(stdsprintf("Unkown error (errno=%d)", errno), LogTree::LOG_ERROR);
			break;
		}
	};

	this->log.log("Closing connection", LogTree::LOG_NOTICE);

	delete buf;
}

bool FTPClient::detectEndOfCommand(char* str, size_t length) {
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

void FTPClient::splitCommandString(char* str, char** cmd, char** args) {
	size_t i = 0;

	while (1) {
		if (str[i] == ' ') {
			str[i] = '\0';
			*args = str + i + 1;
			break;
		} else if (str[i] == '\0') {
			*args = nullptr;
			break;
		}

		i++;
	}

	*cmd = str;
}

std::string FTPClient::buildReply(uint16_t code) {
	return std::to_string(code) + " " + FTPCodes[code] + EOL;
}

std::string FTPClient::buildReply(uint16_t code, const std::string &msg) {
	return std::to_string(code) + " " + msg + EOL;
}

/*std::string FTPClient::buildMultilineReply(uint16_t code, const std::vector<std::string> &msg) {
	// TODO: For welcome message at some point
}*/

bool FTPClient::CommandNotImplemented(FTPClient &client, char *cmd, char *args) {
	client.log.log(stdsprintf("Command not implemented (cmd=%s, args=%s)", cmd, args), LogTree::LOG_INFO);
	client.socket->send(buildReply(502)); // Not implemented
	return true;
}

bool FTPClient::CommandUSER(FTPClient &client, char *cmd, char* user) {
	// Possible replies: 120->220, 220, 421
	client.log.log(stdsprintf("User is %s", user), LogTree::LOG_INFO);
	client.username = user; // User will be used later for authentication
	client.socket->send(buildReply(331)); // Password required
	client.state = FTP_ST_LOGIN_PASS;
	return true;
}

bool FTPClient::CommandQUIT(FTPClient &client, char *cmd, char* args) {
	// Possible replies: 221, 500
	client.log.log("Quitting", LogTree::LOG_INFO);\
	client.socket->send(buildReply(221)); // Closing connection
	client.state = FTP_ST_IDLE;
	return false;
}

bool FTPClient::CommandPORT(FTPClient &client, char *cmd, char* args) {
	// Possible replies: 200, 500, 501, 421, 530
	client.log.log(stdsprintf("Setting active mode and port to %s", args), LogTree::LOG_INFO);
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

		client.dataserver = nullptr;

		try {
			client.data = std::shared_ptr<ClientSocket>(new ClientSocket(address, port));
		} catch (const except::host_not_found& e) {
			client.log.log(stdsprintf("Host not found %s:%hu", address.c_str(), port), LogTree::LOG_ERROR);
			client.socket->send(buildReply(501)); // Invalid parameters
		}

		client.socket->send(buildReply(200)); // Okay
	}

	client.mode = FTP_MODE_ACTIVE;

	return true;
}

bool FTPClient::CommandPASV(FTPClient &client, char *cmd, char* args) {
	// Possible replies: 227, 500, 501, 502, 421, 530
	client.log.log("Setting passive mode", LogTree::LOG_INFO);
	const uint16_t dataport = client.ftpserver.getDataPort();

	if (!client.dataserver) {
		client.dataserver = std::shared_ptr<ServerSocket>(new ServerSocket(dataport, 1));
		client.dataserver->reuse(); // Reuse the port, otherwise binding might fail
		if (client.dataserver->listen() < 0) {
			// Couldn't bind to socket, this is a critical issue
			return false;
		}
	}

	// Get local IP from Network
	if (!Network::getInstance()) {
		// Something is terribly wrong, exit
		client.log.log("Network is not initialized", LogTree::LOG_ERROR);
		return false;
	}

	// Build the reply
	std::string reply = "Entering Passive Mode (";
	uint32_t ip = Network::getInstance()->getIP();
	reply += std::to_string(((uint8_t*)(&ip))[3]) + ",";
	reply += std::to_string(((uint8_t*)(&ip))[2]) + ",";
	reply += std::to_string(((uint8_t*)(&ip))[1]) + ",";
	reply += std::to_string(((uint8_t*)(&ip))[0]) + ",";
	reply += std::to_string((uint8_t)(dataport >> 8)) + ",";
	reply += std::to_string((uint8_t)(dataport & 0xFF)) + ").";

	client.socket->send(buildReply(227, reply)); // Passive mode

	client.mode = FTP_MODE_PASSIVE;

	return true;
}

bool FTPClient::CommandTYPE(FTPClient &client, char *cmd, char* args) {
	// Possible replies: 200, 500, 501, 504, 421, 530
	client.log.log(stdsprintf("Setting TYPE to %s", args), LogTree::LOG_INFO);
	// Only support ASCII non-print and Image/Binary for the moment
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
	client.log.log(stdsprintf("Setting MODE to %s", args), LogTree::LOG_INFO);
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
	client.log.log(stdsprintf("Setting file structure to %s", args), LogTree::LOG_INFO);
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
	client.log.log(stdsprintf("Receiving file %s", filename), LogTree::LOG_NOTICE);

	if (!filename) {
		client.socket->send(buildReply(501)); // Invalid parameters
		return true;
	}

	std::string filepath = VFS::modifyPath(client.curpath, filename, true);

	// Check if file exists
	VFS::File *file = VFS::getFileFromPath(filepath);
	if (!file) {
		client.socket->send(buildReply(450, "File does not exist."));
		return true;
	}

	// Check permissions
	if (!file->write) {
		client.socket->send(buildReply(450, "File has no write permissions."));
		return true;
	}

	// Pre-allocate and set read buffer
	client.buffer.ptr = std::unique_ptr<uint8_t>(new uint8_t[FTP_MAX_PREALLOC]);
	if (!client.buffer.ptr) {
		client.socket->send(buildReply(450, "Cannot allocate enough memory."));
		return true;
	}
	client.buffer.len = 0;
	client.curfile = filepath;

	client.socket->send(buildReply(150)); // Establishing connection

	if (client.mode == FTP_MODE_ACTIVE) {
		// Only connect to the client in active mode
		// If passive mode it will be the client connecting to the server
		if (dynamic_cast<ClientSocket*>(client.data.get())->connect() != 0) {
			client.socket->send(buildReply(425)); // Can't connect
			return true;
		}
	}

	client.state = FTP_ST_STOR;
	return true;
}

bool FTPClient::CommandRETR(FTPClient &client, char *cmd, char* filename) {
	// Possible replies: (125, 150) -> (226, 250, 425, 426, 451), 450, 500, 501, 502, 421, 530
	client.log.log(stdsprintf("Sending file %s", filename), LogTree::LOG_NOTICE);

	if (!filename) {
		client.socket->send(buildReply(501)); // Invalid parameters
		return true;
	}

	std::string filepath = VFS::modifyPath(client.curpath, filename, true);

	// Check if file exists
	VFS::File *file = VFS::getFileFromPath(filepath);
	if (!file) {
		client.socket->send(buildReply(450, "File does not exist."));
		return true;
	}

	// Check permissions
	if (!file->read) {
		client.socket->send(buildReply(450, "File has no read permissions."));
		return true;
	}

	// Pre-allocate read buffer
	client.buffer.ptr = std::unique_ptr<uint8_t>(new uint8_t[file->size]);
	if (!client.buffer.ptr) {
		client.socket->send(buildReply(450, "Cannot allocate enough memory."));
		return true;
	}

	// Copy the file to memory
	try {
		if (file->read(client.buffer.ptr.get(), file->size) != file->size) {
			client.socket->send(buildReply(450, "Cannot read file."));
			return true;
		}
	}
	catch (std::exception &e) {
		client.socket->send(buildReply(450, e.what())); // Failed
		return true;
	}
	client.buffer.len = file->size;
	client.curfile = filepath;

	client.socket->send(buildReply(150)); // Establishing connection

	if (client.mode == FTP_MODE_ACTIVE) {
		if (!(client.data) ||
			(dynamic_cast<ClientSocket*>(client.data.get())->connect() != 0)) {
			client.socket->send(buildReply(425)); // Can't connect
			return true;
		}
	}

	if (client.data) {
		// There is a data connection already established

		// Send the file
		bool notsent = !client.sendData();

		// Close the data connection
		client.data = nullptr;

		if (notsent) {
			client.socket->send(buildReply(426)); // Abort
		} else {
			client.socket->send(buildReply(226)); // Okay
		}
	} else {
		// No data connection yet, be on the lookout
		client.state = FTP_ST_RETR;
	}

	return true;
}

bool FTPClient::CommandNOOP(FTPClient &client, char *cmd, char* args) {
	// Possible replies: 200, 500, 421
	client.socket->send(buildReply(200)); // Okay
	return true;
}

bool FTPClient::CommandPASS(FTPClient &client, char *cmd, char* pass) {
	// Possible replies: 230, 530, 500, 501, 421, 331, 332
	client.log.log("Password received", LogTree::LOG_INFO);

	// Attempt to authenticate user with server authenticate callback
	if (!client.ftpserver.authenticateUser(client.username, pass)) {
		// Authentication failed
		client.socket->send(buildReply(530)); // Not logged in
		return false;
	}

	// User authenticated successfully
	client.socket->send(buildReply(230)); // Logged in
	client.state = FTP_ST_IDLE;
	return true;
}

bool FTPClient::CommandPWD(FTPClient &client, char *cmd, char* args) {
	// Possible replies: 257, 500, 501, 502, 421, 550
	client.log.log("Current directory request, path is " + client.curpath, LogTree::LOG_INFO);
	client.socket->send(buildReply(257, client.curpath));
	return true;
}

bool FTPClient::CommandLIST(FTPClient &client, char *cmd, char* path) {
	// Possible replies: (125, 150) -> (226, 250, 425, 426, 451), 450, 500, 501, 502, 421, 530
	client.log.log(stdsprintf("List directory request for path %s", path), LogTree::LOG_INFO);

	VFS::File::DirectoryContents *contents = nullptr;

	if (path) {
		std::string dirpath = VFS::modifyPath(client.curpath, path);
		contents = VFS::getContentsFromPath(dirpath);
	} else {
		// No path provided
		contents = VFS::getContentsFromPath(client.curpath);
	}

	if (contents == nullptr) {
		// Invalid path
		client.socket->send(buildReply(450)); // Requested file action not taken
		return true;
	}

	// Generate the directory list
	std::string str = "";
	for (auto& cont : *contents) {
		if (cont.second.isDirectory) str += "d";
		else str += "-";

		if (cont.second.read) str += "r";
		else str += "-";

		if (cont.second.write) str += "w";
		else str += "-";

		str += "------- 1 ipmc ipmc ";
		str += stdsprintf("%9d", cont.second.size);
		str += " Jan 1 0:0 ";
		str += cont.first;
		str += EOL;
	}

	bool notsent = true;

	if (client.mode == FTP_MODE_ACTIVE) {
		client.socket->send(buildReply(150)); // Establishing connection
		dynamic_cast<ClientSocket*>(client.data.get())->connect();

		notsent = (client.data->send(str) != (signed)str.length());
		client.data = nullptr;
	} else {
		if (client.data) {
			// There is a data connection established
			client.socket->send(buildReply(150)); // Establishing connection
			notsent = (client.data->send(str) != (signed)str.length());
			client.data = nullptr;
		} else {
			// No connection yet, this will need to get sent later, so buffer it

			// Pre-allocate read buffer
			client.buffer.ptr = std::unique_ptr<uint8_t>(new uint8_t[str.length()]);
			if (!client.buffer.ptr) {
				client.socket->send(buildReply(450, "Cannot allocate enough memory."));
				return true;
			}

			// Copy the string to the write buffer
			memcpy(client.buffer.ptr.get(), str.c_str(), str.length());
			client.buffer.len = str.length();

			client.socket->send(buildReply(150)); // Establishing connection

			client.state = FTP_ST_RETR;

			// Send reply later
			return true;
		}
	}

	if (notsent) {
		client.socket->send(buildReply(426)); // Abort
	} else {
		client.socket->send(buildReply(226)); // Okay
	}

	return true;
}

bool FTPClient::CommandCWD(FTPClient &client, char *cmd, char* path) {
	// Possible replies: 250, 500, 501, 502, 421, 530, 550
	client.log.log(stdsprintf("Directory change request for path %s", path), LogTree::LOG_INFO);

	if (path == nullptr) {
		// No path given
		client.socket->send(buildReply(501)); // Wrong parameters
		return true;
	}

	VFS::File::DirectoryContents *contents = nullptr;
	std::string dirpath = "";

	// Check if the goal is to go one directory up or down
	if (std::string("..").compare(path) == 0) {
		// Split the current path
		std::vector<std::string> vpath = stringSplit(client.curpath, '/');

		// Generate the new path by ignoring the last entry
		if (vpath.size() > 1) {
			for (unsigned int i = 0; i < vpath.size()-1; i++) {
				dirpath += "/" + vpath[i];
			}
		} else {
			dirpath = "/";
		}
	} else {
		// Check if directory exists
		dirpath = VFS::modifyPath(client.curpath, path);
	}

	// Check if the directory exists
	contents = VFS::getContentsFromPath(dirpath);

	if (contents == nullptr) {
		// Directory doesn't exist
		client.socket->send(buildReply(550));
		return true;
	}

	client.curpath = dirpath;
	client.socket->send(buildReply(250, "Changed to directory " + client.curpath)); // Okay

	return true;
}

bool FTPClient::CommandCDUP(FTPClient &client, char *cmd, char* args) {
	// Possible replies: 200, 500, 501, 502, 421, 530, 550
	client.log.log("Directory up request", LogTree::LOG_INFO);

	// Split the current path
	std::vector<std::string> vpath = stringSplit(client.curpath, '/');

	// Generate the new path by ignoring the last entry
	std::string dirpath = "";
	if (vpath.size() > 1) {
		for (unsigned int i = 0; i < vpath.size()-1; i++) {
			dirpath += "/" + vpath[i];
		}
	} else {
		dirpath = "/";
	}

	// Check if directory exists
	VFS::File::DirectoryContents *contents = VFS::getContentsFromPath(dirpath);

	if (contents == nullptr) {
		// Directory doesn't exist
		client.socket->send(buildReply(550));
		return true;
	}

	client.curpath = dirpath;
	client.socket->send(buildReply(200, "Changed to directory " + client.curpath)); // Okay

	return true;
}

bool FTPClient::sendData() {
	// Send the file
	// TODO: For some reason a direct call with file->size blocks and nothing is sent, so break it down
	// TODO: Maybe have this in the select() loop
	//client.data->send(buf.get(), file->size);
	const size_t blocksize = TCP_MSS;
	size_t pagecnt = this->buffer.len / blocksize + 1;
	size_t rem = this->buffer.len % blocksize;
	bool problem = false;
	for (size_t i = 0; i < pagecnt; i++) {
		int len = (i == pagecnt-1)? rem : blocksize;
		if (this->data->send(this->buffer.ptr.get() + i * blocksize, len) != len) {
			problem = true;
			break;
		}
	}

	// Release the buffer from memory
	this->buffer.ptr = nullptr;

	return !problem; // Returns true if success
}

std::map<std::string, FTPClient::FTPCommand> FTPClient::FTPCommands = {
	{"USER", {FTPClient::CommandUSER, {FTP_ST_LOGIN_USER}}},
	{"PASS", {FTPClient::CommandPASS, {FTP_ST_LOGIN_PASS}}},
	{"ACCT", {FTPClient::CommandNotImplemented, {FTP_ST_IDLE}}},
	{"CWD",  {FTPClient::CommandCWD,  {FTP_ST_IDLE}}},
	{"CDUP", {FTPClient::CommandCDUP, {FTP_ST_IDLE}}},
	{"SMNT", {FTPClient::CommandNotImplemented, {FTP_ST_IDLE}}},
	{"QUIT", {FTPClient::CommandQUIT, {FTP_ST_IDLE}}},
	{"REIN", {FTPClient::CommandNotImplemented, {FTP_ST_IDLE}}},
	{"PORT", {FTPClient::CommandPORT, {FTP_ST_IDLE}}},
	{"PASV", {FTPClient::CommandPASV, {FTP_ST_IDLE}}},
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
	{"NOOP", {FTPClient::CommandNOOP, {FTP_ST_IDLE}}}, // Should this be allowed before login? Don't think so
};

