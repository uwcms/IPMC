/*
 * FTPServer.h
 *
 *  Created on: Jul 23, 2018
 *      Author: mpv
 *
 * Based on RFC 959: https://tools.ietf.org/html/rfc959
 * Implements all minimum features, passive mode and directories.
 */

#ifndef SRC_COMMON_UW_IPMC_SERVICES_FTP_FTPSERVER_H_
#define SRC_COMMON_UW_IPMC_SERVICES_FTP_FTPSERVER_H_

#include <string>
#include <drivers/network/Socket.h>
#include <drivers/network/ServerSocket.h>
#include <drivers/network/ClientSocket.h>
#include <libs/vfs.h>

///! Uncomment to enabled debugging of the FTP server.
#define FTPSERVER_DEBUG
#define FTPSERVER_MAX_BACKLOG 1
#define FTPSERVER_THREAD_NAME "ftpd"

/**
 * Single instance FTP Server
 * Only one client and a single connection will be allowed at any time to avoid problems.
 * The constructor takes a callback function that needs to return true if the user credentials are valid.
 */
class FTPServer final {
	typedef bool AuthCallbackFunc(const std::string &user, const std::string &pass);

public:
	/**
	 * Creates and starts the FTP server.
	 * @param authcallback Authentication function. Can be set to NULL but no user will be
	 * allowed until FTPServer::setAuthCallback is called to set a proper callback. Must return true if
	 * credentials are proper and user is allowed to proceed.
	 * @param comport Communication port. Default is 21.
	 * @param dataport Data port. Default is 20.
	 */
	FTPServer(AuthCallbackFunc *authcallback, const uint16_t comport = 21, const uint16_t dataport = 20);
	~FTPServer() {};

	/**
	 * Set or unset the authentication callback function.
	 * @param func Authentication callback function.
	 */
	inline void setAuthCallback(const AuthCallbackFunc *func) { this->authcallback = func; };

	friend class FTPClient;
private:
	void thread_ftpserverd();

	AuthCallbackFunc *authcallback;				///< Authentication callback function.
	const uint16_t comport;						///< FTP communication port.
	const uint16_t dataport;					///< FTP data port.

protected:
	///! Returns the server communication port.
	inline const uint16_t getComPort() const { return this->comport; };
	///! Returns the server data port.
	inline const uint16_t getDataPort() const { return this->dataport; };
	///! Attempts to authenticate a user, returns false if failed to do so.
	bool authenticateUser(const std::string &user, const std::string &pass) const;
};

/**
 * The FTP client instance, created when the FTP server accepts a valid connection
 */
class FTPClient {
	static const size_t FTP_MAX_PREALLOC = 16 * 1024 * 1024; // in bytes
	static const unsigned int FTP_TIMEOUT_SEC = 60;

public:
	/**
	 * Creates a new FTP client and processes any incomding requests.
	 * @param ftpserver The FTP server that launched the client.
	 * @param socket The socket attributed to the client.
	 */
	FTPClient(const FTPServer &ftpserver, std::shared_ptr<Socket> socket);
	virtual ~FTPClient() {};

	///! All the different states the client can be at.
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

private:
	typedef bool FTPCommandFunc(FTPClient&,char*,char*);

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

	bool sendData();

private:
	const FTPServer &ftpserver;					// The FTP server from which the client was launched from
	std::string username;						// The username
	std::shared_ptr<Socket> socket;				// Command socket
	std::shared_ptr<ServerSocket> dataserver;	// Data server socket (passive mode)
	std::shared_ptr<Socket> data;				// Data socket (passive and active mode)
	FTPState state;
	enum { FTP_MODE_ACTIVE, FTP_MODE_PASSIVE } mode;

	std::string curpath; // Current path
	std::string curfile; // Path to the file being read/written
	// Reason why we don't keep a current file and directory is because these might
	// change later on another part of the application and get invalidated. And alternative
	// would be to use std::shared_ptr later on on the file system.
	//FTPFile* file;
	//FTPFile::FTPDirContents *dir;

	struct {
		std::unique_ptr<uint8_t> ptr;
		size_t len;
	} buffer;

	static std::map<uint16_t, std::string> FTPCodes;
	static std::string buildReply(uint16_t code);
	static std::string buildReply(uint16_t code, const std::string &msg);
	//static std::string buildMultilineReply(uint16_t code, const std::vector<std::string> &msg);

	typedef std::pair<FTPCommandFunc*, std::vector<FTPState>> FTPCommand;
	static std::map<std::string, FTPCommand> FTPCommands;
};

#endif /* SRC_COMMON_UW_IPMC_SERVICES_FTP_FTPSERVER_H_ */
