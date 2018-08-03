/*
 * Utils.cc
 *
 *  Created on: Aug 3, 2018
 *      Author: mpv
 */

#include <libs/Utils.h>

std::string bytesToString(uint64_t bytes) {
	const size_t STR_MAX_LEN = 16; // "0000.00 XiB" + 1 = 12
	char buf[STR_MAX_LEN];

	if ((bytes >> 40) != 0) {
		std::snprintf(buf, STR_MAX_LEN, "%0.2f TiB", (bytes >> 30) / 1024.0);
	} else if ((bytes >> 30) != 0) {
		std::snprintf(buf, STR_MAX_LEN, "%0.2f GiB", (bytes >> 20) / 1024.0);
	} else if ((bytes >> 20) != 0) {
		std::snprintf(buf, STR_MAX_LEN, "%0.2f MiB", (bytes >> 10) / 1024.0);
	} else if ((bytes >> 10) != 0) {
		std::snprintf(buf, STR_MAX_LEN, "%0.2f KiB", bytes / 1024.0);
	} else {
		std::snprintf(buf, STR_MAX_LEN, "%d B", bytes);
	}

	return buf;
}

std::vector<std::string> stringSplit(std::string str, char delimiter) {
	std::vector<std::string> v;

	size_t pos = 0, end;

	do {
		end = str.find(delimiter, pos);
		std::string ss = str.substr(pos, end);
		pos = end + 1; // Skip the delimiter

		if (ss.length() > 0) v.push_back(ss);
	} while (end != std::string::npos);

	return v;
}
