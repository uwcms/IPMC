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

/**
 * Based on RFC 959: https://tools.ietf.org/html/rfc959
 * Implements all minimum features, passive mode and directories.
 */

#ifndef SRC_COMMON_ZYNQIPMC_SERVICES_FTP_FTPSERVER_H_
#define SRC_COMMON_ZYNQIPMC_SERVICES_FTP_FTPSERVER_H_

#include <string>
#include <drivers/network/client_socket.h>
#include <drivers/network/server_socket.h>
#include <libs/vfs/vfs.h>
#include <libs/logtree/logtree.h>

/**
 * Single instance FTP Server.
 * Only one client and a single connection will be allowed at any time to avoid problems.
 * The constructor takes a callback function that needs to return true if the user credentials are valid.
 */
class FTPServer final {
	typedef bool AuthCallbackFunc(const std::string &user, const std::string &pass);

public:
	/**
	 * Creates and starts the FTP server.
	 * @param authcallback Authentication function. Can be set to nullptr but no user will be
	 * allowed until FTPServer::setAuthCallback is called to set a proper callback. Must return true if
	 * credentials are proper and user is allowed to proceed.
	 * @param log Log facility to report status.
	 * @param comport Communication port. Default is 21.
	 * @param dataport Data port. Default is 20.
	 * @param thread_name Name to give the deamon thread. Default is ftpd.
	 * @param backlog Number of possible waiting connections. Default is 1.
	 */
	FTPServer(AuthCallbackFunc *authcallback, LogTree &log, const uint16_t comport = 21,
				const uint16_t dataport = 20, const std::string& thread_name = "ftpd", size_t backlog = 1);
	~FTPServer() {};

	/**
	 * Set or unset the authentication callback function.
	 * @param func Authentication callback function. nullptr to unset.
	 */
	inline void setAuthCallback(const AuthCallbackFunc *func) { this->authcallback = func; };

	//! Returns the server communication port.
	inline const uint16_t getComPort() const { return this->kComPort; };

	//! Returns the server data port.
	inline const uint16_t getDataPort() const { return this->kDataPort; };

	friend class FTPClient;

protected:
	//! Attempts to authenticate a user, returns false if failed to do so.
	bool authenticateUser(const std::string &user, const std::string &pass) const;

private:
	void threadFTPserverd();

	AuthCallbackFunc *authcallback;	///< Authentication callback function.
	LogTree& log;					///< Logging facility.
	const uint16_t kComPort;		///< FTP communication port.
	const uint16_t kDataPort;		///< FTP data port.
	const size_t kBacklog;			///< Backlog limit.
};

/**
 * The FTP client instance, created when the FTP server accepts a valid connection
 */
class FTPClient {
	//! Maximum allowed buffer allocation when receiving files in byes.
	static const size_t FTP_MAX_PREALLOC = 16 * 1024 * 1024;

	//! Inactivity timeout before disconnecting client.
	static const unsigned int FTP_TIMEOUT_SEC = 60;

public:
	/**
	 * Creates a new FTP client and processes any incoming requests.
	 * @param ftpserver FTP server that launched the client.
	 * @param log Log facility to report status.
	 * @param socket Socket attributed to the client.
	 */
	FTPClient(const FTPServer &ftpserver, LogTree &log, std::shared_ptr<Socket> socket);
	virtual ~FTPClient() {};

	//! All the different states the client can be at.
	typedef enum {
		FTP_ST_LOGIN_USER,
		FTP_ST_LOGIN_PASS,
		FTP_ST_IDLE,
		FTP_ST_STOR,
		FTP_ST_RETR,
		FTP_ST_ANY, // Special case: command can run in any state
		// TODO: Maybe delete FTP_ST_ANY
	} FTPState;

private:
	bool detectEndOfCommand(char* str, size_t length);
	void splitCommandString(char* str, char** cmd, char** args);

	typedef bool FTPCommandFunc(FTPClient&,char*,char*);

	//! List of implemented FTP commands.
	///@{
	static FTPCommandFunc CommandNotImplemented;
	static FTPCommandFunc CommandUSER;
	static FTPCommandFunc CommandQUIT;
	static FTPCommandFunc CommandPORT;
	static FTPCommandFunc CommandPASV;
	static FTPCommandFunc CommandTYPE;
	static FTPCommandFunc CommandMODE;
	static FTPCommandFunc CommandSTRU;
	static FTPCommandFunc CommandSTOR;
	static FTPCommandFunc CommandRETR;
	static FTPCommandFunc CommandNOOP;
	static FTPCommandFunc CommandPASS;
	static FTPCommandFunc CommandPWD;
	static FTPCommandFunc CommandLIST;
	static FTPCommandFunc CommandCWD;
	static FTPCommandFunc CommandCDUP;
	///@}

	//! Send the temporarily stored file through the data socket. Internal usage.
	bool sendData();

	const FTPServer &ftpserver;					///< The FTP server from which the client was launched from.
	LogTree& log;								///< Log facility.
	std::string username;						///< The username.
	std::shared_ptr<Socket> socket;				///< Command socket.
	std::shared_ptr<ServerSocket> dataserver;	///< Data server socket (passive mode).
	std::shared_ptr<Socket> data;				///< Data socket (passive and active mode).
	FTPState state;
	enum { FTP_MODE_ACTIVE, FTP_MODE_PASSIVE } mode;

	std::string curpath;	///< Current path
	std::string curfile;	///< Path to the file being read/written

	//! Because of how FTP works it is best to have the file locally stored until the client disconnects.
	struct {
		std::unique_ptr<uint8_t> ptr;
		size_t len;
	} buffer;

	static std::map<uint16_t, std::string> FTPCodes;	///< All supported FTP codes.

	//! Build an FTP compliant reply.
	static std::string buildReply(uint16_t code);
	static std::string buildReply(uint16_t code, const std::string &msg);
	//static std::string buildMultilineReply(uint16_t code, const std::vector<std::string> &msg);

	typedef std::pair<FTPCommandFunc*, std::vector<FTPState>> FTPCommand;
	static std::map<std::string, FTPCommand> FTPCommands;	///< All supported FTP commands.
};

#endif /* SRC_COMMON_ZYNQIPMC_SERVICES_FTP_FTPSERVER_H_ */
