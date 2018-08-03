/*
 * FTPServer.h
 *
 *  Created on: Jul 23, 2018
 *      Author: mpv
 */

#ifndef SRC_COMMON_UW_IPMC_SERVICES_FTP_FTPSERVER_H_
#define SRC_COMMON_UW_IPMC_SERVICES_FTP_FTPSERVER_H_

#include <string>
#include <drivers/network/Socket.h>
#include <drivers/network/ServerSocket.h>
#include <drivers/network/ClientSocket.h>

struct FTPFile {
	//std::string filename;
	unsigned int size;

	size_t (*readfile)(uint8_t *buf);
	size_t (*writefile)(uint8_t *buf, size_t size);

	typedef std::map<std::string, FTPFile> FTPDirContents;
	bool isDirectory;
	FTPDirContents contents;
};

class FTPServer {
	static const uint16_t FTP_COM_PORT = 21;
	static const uint16_t FTP_DATA_PORT = 20;
	static const unsigned int FTP_MAX_INSTANCES = 1;
	static const unsigned int FTP_TIMEOUT_SEC = 60;

public:
	FTPServer();
	virtual ~FTPServer();

	static void setFiles(FTPFile::FTPDirContents files) {FTPServer::files = files;};
	static FTPFile::FTPDirContents* getDirContentsFromPath(std::string path, FTPFile::FTPDirContents *curDir);
	static FTPFile* getFileFromPath(std::string filepath, FTPFile::FTPDirContents *curDir);

	friend class FTPClient;
private:
	void thread_ftpserverd();

	static FTPFile::FTPDirContents files;
	//static std::vector<struct FTPFile> files;
};

class FTPClient {
	static const size_t FTP_MAX_PREALLOC = 16 * 1024 * 1024; // in bytes

public:
	FTPClient(std::shared_ptr<Socket> socket);
	virtual ~FTPClient() {};

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

	bool sendData();

private:
	std::shared_ptr<Socket> socket;				// Command socket
	std::shared_ptr<ServerSocket> dataserver;	// Data server socket (passive mode)
	std::shared_ptr<Socket> data;				// Data socket (passive and active mode)
	FTPState state;
	enum { FTP_MODE_ACTIVE, FTP_MODE_PASSIVE } mode;

	std::string path;
	FTPFile* file;
	FTPFile::FTPDirContents *dir; // TODO: Can be a memory leak

	struct {
		std::unique_ptr<uint8_t> ptr;
		size_t len;
	} buffer;

	static std::map<uint16_t, std::string> FTPCodes;
	static std::string buildReply(uint16_t code);
	static std::string buildReply(uint16_t code, std::string msg);

	typedef std::pair<FTPCommandFunc*, std::vector<FTPState>> FTPCommand;
	static std::map<std::string, FTPCommand> FTPCommands;
};

#endif /* SRC_COMMON_UW_IPMC_SERVICES_FTP_FTPSERVER_H_ */
