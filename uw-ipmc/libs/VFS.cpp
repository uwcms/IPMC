/*
 * VFS.cpp
 *
 *  Created on: Nov 5, 2018
 *      Author: mpv
 */

#include "VFS.h"

VFS::File::DirectoryContents VFS::files = {};

bool VFS::addFile(const std::string& filename, VFS::File file) {
	VFS::File::DirectoryContents* curdir = &VFS::files;

	std::vector<std::string> vpath = stringSplit(filename, '/');
	size_t nitems = vpath.size();
	size_t i = 0;

	for (;i < (nitems - 1); i++) {
		// Add the necessary directories if they don't exist
		bool change = false;
		try {
			VFS::File* f = &curdir->at(vpath[i]);

			if (f->isDirectory) curdir = &(f->contents);
			else change = true; // Not a directory, but we will replace it
		} catch (std::out_of_range& e) {
			// Doesn't exist, create empty directory
			change = true;
		}

		if (change) {
			(*curdir)[vpath[i]] = VFS::File();
			curdir = &(curdir->at(vpath[i]).contents);
		}
	}

	// Create the file
	(*curdir)[vpath[i]] = file;

	return true;
}

bool VFS::removeFile(const std::string& filename) {
	VFS::File::DirectoryContents *dir = &(VFS::files);

	std::vector<std::string> vpath = stringSplit(filename, '/');
	size_t nitems = vpath.size();
	if (nitems == 0) return NULL;

	for (size_t i = 0; i < nitems; i++) {
		try {
			VFS::File* f = &(dir->at(vpath[i]));

			if (i == (nitems - 1)) {
				// If this is the last item on the list then it must be the file
				if (f->isDirectory) return false;
				else {
					// Remove the file
					dir->erase(vpath[i]);
					return true;
				}
			} else {
				if (f->isDirectory) dir = &(f->contents);
				else return false;
			}
		} catch (std::out_of_range& e) {
			// Doesn't exist
			return false;
		}
	}

	return false;
}

std::string VFS::modifyPath(const std::string& curpath, const std::string& addition, bool isfile) {
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

VFS::File::DirectoryContents* VFS::getContentsFromPath(const std::string &dirpath) {
	VFS::File::DirectoryContents *dir = &(VFS::files);

	std::vector<std::string> vpath = stringSplit(dirpath, '/');
	size_t nitems = vpath.size();
	if (nitems == 0) return dir;

	for (auto it : vpath) {
		try {
			VFS::File& f = dir->at(it);

			if (f.isDirectory) dir = &(f.contents);
			else return NULL; // Not a directory
		} catch (std::out_of_range& e) {
			// Doesn't exist
			return NULL;
		}
	}

	return dir;
}

VFS::File* VFS::getFileFromPath(const std::string &filepath) {
	VFS::File::DirectoryContents *dir = &(VFS::files);

	std::vector<std::string> vpath = stringSplit(filepath, '/');
	size_t nitems = vpath.size();
	if (nitems == 0) return NULL;

	for (size_t i = 0; i < nitems; i++) {
		try {
			VFS::File* f = &(dir->at(vpath[i]));

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


