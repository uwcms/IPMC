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

///! Uncomment to enabled debugging of the FTP server.
#define FTPSERVER_DEBUG
#define FTPSERVER_MAX_BACKLOG 1
#define FTPSERVER_THREAD_NAME "ftpd"

/**
 * Defines a single file that can be used by the FTP server
 */
struct FTPFile {
	size_t size; ///< The size of the file in bytes.

	//typedef size_t (*FileCallback)(uint8_t *buf, size_t size);
	typedef std::function<size_t(uint8_t*, size_t)> FileCallback; ///< Callback type used in virtual files.
	FileCallback read;  ///< The callback used to fill the read buffer.
	FileCallback write; ///< The callback used write to write the file with the buffer.

	typedef std::map<std::string, FTPFile> DirectoryContents; ///< Defines the contents of a directory.
	bool isDirectory; ///< true if this particular file is in fact a directory entry.
	DirectoryContents contents; ///< The contents of the directory if isDirectory is true.

	/**
	 * Creates a new file.
	 * @param read The read callback function.
	 * @param write The write callback function.
	 * @param size The number of bytes the file has.
	 */
	FTPFile(FileCallback read, FileCallback write, size_t size) :
		size(size), read(read), write(write), isDirectory(false), contents({}) {};

	/**
	 * Creates a directory with some contents.
	 * @param contents
	 */
	FTPFile(DirectoryContents contents) : size(0), read(NULL), write(NULL), isDirectory(true), contents(contents) {};

	/**
	 * Creates an empty directory with no contents.
	 */
	FTPFile() : size(0), read(NULL), write(NULL), isDirectory(true), contents({}) {};
};

/**
 * Single instance FTP Server
 * Only one client and a single connection will be allowed at any time to avoid problems.
 * The constructor takes a callback function that needs to return true if the user credentials are valid.
 */
class FTPServer {
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
	virtual ~FTPServer();

	/**
	 * Set or unset the authentication callback function.
	 * @param func Authentication callback function.
	 */
	inline void setAuthCallback(const AuthCallbackFunc *func) { this->authcallback = func; };

	/**
	 * Set the root file directory.
	 * @param files The directory root contents.
	 */
	static void setFiles(FTPFile::DirectoryContents files) {FTPServer::files = files;};

	/**
	 * Create or add a new file reference.
	 * @param filename The full path to the file.
	 * @param file FTPFile structure with information about the file
	 * @return Always true.
	 */
	static bool addFile(const std::string& filename, FTPFile file);

	/**
	 * Remove a certain file reference.
	 * @param filename The full path of the file to remove.
	 * @return true if success, false otherwise.
	 */
	static bool removeFile(const std::string& filename);

	///! Generates a new path based on the current path plus an extension.
	static std::string modifyPath(const std::string& curpath, const std::string& addition, bool isfile = false);

	///! Returns the directory contents (or NULL if invalid) for a given path.
	static FTPFile::DirectoryContents* getContentsFromPath(const std::string &path);

	///! Returns the file (or NULL if invalid) for a given file path.
	static FTPFile* getFileFromPath(const std::string &filepath);

	friend class FTPClient;
private:
	void thread_ftpserverd();

	AuthCallbackFunc *authcallback;				///< Authentication callback function.
	const uint16_t comport;						///< FTP communication port.
	const uint16_t dataport;					///< FTP data port.
	static FTPFile::DirectoryContents files;	///< Virtual file root directory.

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
