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

std::vector<std::string> stringSplit(const std::string &str, char delimiter) {
	std::vector<std::string> v;

	size_t pos = 0, end;

	do {
		end = str.find(delimiter, pos);
		std::string ss = str.substr(pos, end - pos);
		pos = end + 1; // Skip the delimiter

		if (ss.length() > 0) v.push_back(ss);
	} while (end != std::string::npos);

	return v;
}

std::string formatedHexString(const void *ptr, size_t bytes, size_t str_offset) {
#define WORD_BYTES 4
#define WORDS_PER_LINE 4
#define BYTES_PER_LINE (WORD_BYTES * WORDS_PER_LINE)
	std::string r;

	// Pre-allocate the string - it will make it faster
	const size_t reserve_bytes_per_line = 8 + 2 + BYTES_PER_LINE * 2 + WORDS_PER_LINE + BYTES_PER_LINE + 1;
	const size_t total_lines = bytes / BYTES_PER_LINE + ((bytes % BYTES_PER_LINE)? 1 : 0);
	const size_t required_size = reserve_bytes_per_line * total_lines + 1;

	r.reserve(required_size);

	char addr[9], ascii[BYTES_PER_LINE + 1] = "", hex[3];

	for (size_t i = 0; i < total_lines; i++) {
		size_t line_bytes = bytes - (i * BYTES_PER_LINE);
		line_bytes = (line_bytes > BYTES_PER_LINE)? BYTES_PER_LINE : line_bytes;
		size_t empty_bytes = BYTES_PER_LINE - line_bytes;

		snprintf(addr, 9, "%08X", i * BYTES_PER_LINE + str_offset);
		r += std::string(addr) + ": ";

		for (size_t j = 0; j < line_bytes; j++) {
			uint8_t v = ((uint8_t*)ptr)[i * BYTES_PER_LINE + j];

			if (v >= ' ' && v <= '~') ascii[j] = v; // Known character
			else ascii[j] = '.'; // Unknown character

			snprintf(hex, 3, "%02X", v);
			r += hex;

			if ((j % WORD_BYTES) == (WORD_BYTES - 1)) r += " ";
		}

		for (size_t j = line_bytes; j < BYTES_PER_LINE; j++) {
			ascii[j] = ' ';

			r += "  ";

			if ((j % WORD_BYTES) == (WORD_BYTES - 1)) r += " ";
		}

		r += std::string(ascii) + "\n";
	}

	return r;
}

bool toUint32(std::string &s, uint32_t &v) {
	// Check if hex
	if (s.length() > 2 && (s.substr(0, 2) == "0x")) {
		errno = 0;
		v = strtoul(s.substr(2).c_str(), NULL, 16);
		if (errno != 0) return false;

		return true;
	} else if (s.length() > 1 && (s.substr(0, 1) == "b")) {
		std::string b = s.substr(1);
		size_t len = b.length();

		if (len > 32) return false; // too long

		for (size_t i = 0; i < len; i++) {
			if (!(b[i] == '0' || b[i] == '1'))
				return false; // invalid characters
		}

		v = 0;
		for (size_t i = len; i != 0; i--) {
			if (b[i-1] == '1') v |= (1 << (i-1));
		}

		return true;
	}

	return false;
}
