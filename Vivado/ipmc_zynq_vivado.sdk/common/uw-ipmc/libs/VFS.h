/*
 * VFS.h
 *
 *  Created on: Nov 5, 2018
 *      Author: mpv
 */

#ifndef SRC_COMMON_UW_IPMC_LIBS_VFS_H_
#define SRC_COMMON_UW_IPMC_LIBS_VFS_H_

#include <functional>
#include <map>
#include <libs/Utils.h>

class VFS {
public:
	/**
	 * Defines a single file that can be used by the FTP server
	 */
	struct File {
		size_t size; ///< The size of the file in bytes.

		using FileCallback = std::function<size_t(uint8_t*, size_t)>; ///< Callback type used in virtual files.
		FileCallback read;  ///< The callback used to fill the read buffer.
		FileCallback write; ///< The callback used write to write the file with the buffer.

		using DirectoryContents = std::map<std::string, VFS::File>; ///< Defines the contents of a directory.
		bool isDirectory; ///< true if this particular file is in fact a directory entry.
		DirectoryContents contents; ///< The contents of the directory if isDirectory is true.

		/**
		 * Creates a new file.
		 * @param read The read callback function.
		 * @param write The write callback function.
		 * @param size The number of bytes the file has.
		 */
		File(FileCallback read, FileCallback write, size_t size) :
			size(size), read(read), write(write), isDirectory(false), contents({}) {};

		/**
		 * Creates a directory with some contents.
		 * @param contents
		 */
		File(DirectoryContents contents) : size(0), read(NULL), write(NULL), isDirectory(true), contents(contents) {};

		/**
		 * Creates an empty directory with no contents.
		 */
		File() : size(0), read(NULL), write(NULL), isDirectory(true), contents({}) {};
	};

	/**
	 * Set the root file directory.
	 * @param files The directory root contents.
	 */
	static void setFiles(VFS::File::DirectoryContents files) {VFS::files = files;};

	/**
	 * Create or add a new file reference.
	 * @param filename The full path to the file.
	 * @param file FTPFile structure with information about the file
	 * @return Always true.
	 */
	static bool addFile(const std::string& filename, VFS::File file);

	/**
	 * Remove a certain file reference.
	 * @param filename The full path of the file to remove.
	 * @return true if success, false otherwise.
	 */
	static bool removeFile(const std::string& filename);

	///! Generates a new path based on the current path plus an extension.
	static std::string modifyPath(const std::string& curpath, const std::string& addition, bool isfile = false);

	///! Returns the directory contents (or NULL if invalid) for a given path.
	static VFS::File::DirectoryContents* getContentsFromPath(const std::string &path);

	///! Returns the file (or NULL if invalid) for a given file path.
	static VFS::File* getFileFromPath(const std::string &filepath);

private:
	static VFS::File::DirectoryContents files;	///< Virtual file root directory.
};

#endif /* SRC_COMMON_UW_IPMC_LIBS_VFS_H_ */
