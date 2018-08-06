/*
 * FTPServer.cpp
 *
 *  Created on: Jul 23, 2018
 *      Author: mpv
 */

#include <IPMC.h>
#include "libs/Utils.h"
#include <FreeRTOS.h>
#include <task.h>
#include <algorithm>

#include "drivers/network/ServerSocket.h"
#include <libs/ThreadingPrimitives.h>

#include "FTPServer.h"


FTPServer::FTPServer(AuthCallbackFunc *authcallback, const uint16_t comport, const uint16_t dataport)
: authcallback(authcallback), comport(comport), dataport(dataport) {
	configASSERT(UWTaskCreate(std::string(FTPSERVER_THREAD_NAME ":") + std::to_string(comport), TASK_PRIORITY_SERVICE, [this]() -> void {
		this->thread_ftpserverd();
	}));

}

FTPServer::~FTPServer() {
	// TODO
}

void FTPServer::thread_ftpserverd() {
	std::unique_ptr<ServerSocket> server (new ServerSocket(comport, FTPSERVER_MAX_BACKLOG));

	int err = server->listen();

	if (err != 0) {
		printf(strerror(err));
		return;
	}

	while (true) {
		std::shared_ptr<Socket> client = server->accept();

		if (!client->isValid()) {
			continue;
		}

		// Start the client, only one instance allowed
		delete new FTPClient(*this, client);
	}
}

std::string FTPServer::modifyPath(const std::string& curpath, const std::string& addition, bool isfile) {
	std::string newpath = "";

	if (addition[0] == '/') {
		// New root path
		newpath = addition;
	} else {
		newpath = curpath;
		if (newpath.back() != '/') newpath += '/';
		newpath += addition;
	}

	if (isfile == false) {
		if (newpath.back() != '/') newpath += '/';
	}

	return newpath;
}

FTPFile::FTPDirContents* FTPServer::getDirContentsFromPath(const std::string &dirpath) {
	FTPFile::FTPDirContents *dir = &(FTPServer::files);

	std::vector<std::string> vpath = stringSplit(dirpath, '/');
	size_t nitems = vpath.size();
	if (nitems == 0) return dir;

	for (auto it : vpath) {
		try {
			FTPFile& f = dir->at(it);

			if (f.isDirectory) dir = &(f.contents);
			else return NULL; // Not a directory
		} catch (std::out_of_range& e) {
			// Doesn't exist
			return NULL;
		}
	}

	return dir;
}

FTPFile* FTPServer::getFileFromPath(const std::string &filepath) {
	FTPFile::FTPDirContents *dir = &(FTPServer::files);

	std::vector<std::string> vpath = stringSplit(filepath, '/');
	size_t nitems = vpath.size();
	if (nitems == 0) return NULL;

	for (size_t i = 0; i < nitems; i++) {
		try {
			FTPFile* f = &(dir->at(vpath[i]));

			if (i == (nitems - 1)) {
				// If this is the last item on the list then it must be the file
				if (f->isDirectory) return NULL;
				else return f;
			} else {
				if (f->isDirectory) dir = &(f->contents);
				else return NULL;
			}
		} catch (std::out_of_range& e) {
			// Doesn't exist
			return NULL;
		}
	}

	return NULL;
}

bool FTPServer::authenticateUser(const std::string &user, const std::string &pass) const {
	if (this->authcallback == NULL) return false; // Don't allow to authenticate if no authenticate callback exists
	return this->authcallback(user, pass);
}


