/*
 * FTPServer.h
 *
 *  Created on: Jul 23, 2018
 *      Author: mpv
 */

#ifndef SRC_COMMON_UW_IPMC_SERVICES_FTP_FTPSERVER_H_
#define SRC_COMMON_UW_IPMC_SERVICES_FTP_FTPSERVER_H_

class FTPServer {
	const uint16_t FTP_PORT = 21;
	const unsigned int FTP_MAX_INSTANCES = 1;

public:
	FTPServer();
	virtual ~FTPServer();

private:
	void thread_ftpserverd();
};

class FTPClient {
public:
	FTPClient(std::shared_ptr<Socket> socket);
	virtual ~FTPClient() {};

	typedef enum {
		FTP_ST_LOGIN_USER,
		FTP_ST_LOGIN_PASS,
		FTP_ST_IDLE,
		FTP_ST_ANY, // Special case: command can run in any state
	} FTPState;

private:
	typedef bool FTPCommandFunc(FTPClient&,char*,char*);

	static FTPCommandFunc CommandNotImplemented;
	static FTPCommandFunc CommandUSER;
	static FTPCommandFunc CommandPASS;
	static FTPCommandFunc CommandQUIT;
	static FTPCommandFunc CommandPORT;

private:
	std::shared_ptr<Socket> socket;
	std::shared_ptr<Socket> data;
	FTPState state;

	static std::map<uint16_t, std::string> FTPCodes;
	static std::string buildReply(uint16_t code);

	typedef std::pair<FTPCommandFunc*, std::vector<FTPState>> FTPCommand;
	static std::map<std::string, FTPCommand> FTPCommands;
};

#endif /* SRC_COMMON_UW_IPMC_SERVICES_FTP_FTPSERVER_H_ */
