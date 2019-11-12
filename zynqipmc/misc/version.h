/*
 * version.h
 *
 *  Created on: Nov 8, 2019
 *      Author: jtikalsky
 */

#ifndef SRC_COMMON_ZYNQIPMC_MISC_VERSION_H_
#define SRC_COMMON_ZYNQIPMC_MISC_VERSION_H_

#include <memory>

class VersionInfo {
public:
	struct {
		uint32_t commit;      ///< The short git hash, as a uint32_t.
		std::string hash;     ///< The long git hash, as a string.
		std::string describe; ///< The output of "git describe [...flags]"
		bool dirty;           ///< true if the repository was dirty at build time.
	} git;
	struct {
		std::string user;          ///< The user who built the code.
		std::string host;          ///< The host the code was built on.
		std::string human_date;    ///< The build timestamp in human readable format.
		std::string machine_date;  ///< The build timestamp in a more machine friendly format.
		std::string configuration; ///< The build configuration used.
	} build;
	struct {
		std::string tag;       ///< The git tag this version is descended from. (Label/Prefix only)
		std::string version;   ///< The version number, as a string. e.g. "1.0.0", "1.0.0b", "1.0.0.1"
		unsigned major;        ///< The major version number. e.g. 1.2.3 => 1
		unsigned minor;        ///< The minor version number. e.g. 1.2.3 => 2
		unsigned revision;     ///< The revision number.      e.g. 1.2.3 => 3
		std::string extra;     ///< Any extra version info.   e.g. 1.2.3a => "a", 1.2.3.4 => ".4"
		unsigned plus_commits; ///< The number of commits ahead of the version tag described.
		bool dirty;            ///< True if the repository was dirty at build time.
	} version;
	std::string summary; ///< A one-line human readable build/version summary.
	std::string json; ///< The raw JSON version data parsed into this structure.

	static std::shared_ptr<VersionInfo> parse(std::string json);
	static std::shared_ptr<const VersionInfo> get_running_version();
};

#endif /* SRC_COMMON_ZYNQIPMC_MISC_VERSION_H_ */
