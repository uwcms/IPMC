/*
 * This file is part of the ZYNQ-IPMC Framework.
 *
 * The ZYNQ-IPMC Framework is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The ZYNQ-IPMC Framework is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the ZYNQ-IPMC Framework.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "version.h"

#include <libs/mjson.h>

template <typename T>
static inline bool parse_numeric(const std::string &json, const std::string path, T &v) {
	/* This library uses doubles (because apparently so does javascript?), but
	 * a double can contain a uint32_t losslessly so whatever.
	 */
	double dv;
	if (mjson_get_number(json.c_str(), json.size(), path.c_str(), &dv)) {
		v = dv;
		return true;
	}
	return false;
}

template <int MAXLEN>
static inline bool parse_string(const std::string &json, const std::string path, std::string &v) {
	char s[MAXLEN];
	int slen = mjson_get_string(json.c_str(), json.size(), path.c_str(), s, MAXLEN);
	if (slen > 0 && slen < MAXLEN) {
		v = std::string(s, s+slen);
		return true;
	}
	return false;
}

static inline bool parse_bool(const std::string &json, const std::string path, bool &v) {
	int i;
	if (mjson_get_bool(json.c_str(), json.size(), path.c_str(), &i)) {
		v = i;
		return true;
	}
	return false;
}

std::shared_ptr<VersionInfo> VersionInfo::parse(std::string json) {
	std::shared_ptr<VersionInfo> version = std::make_shared<VersionInfo>();
	version->json = json;

	/*
    "git": {
        "describe": "APd1-v1.0.5-dirty",
        "branch": "main",
        "dirty": true,
        "hash": "c7c97c888c6d85797930f31cd687a6df957905b2",
        "short": "c7c97c88",
        "uint32_t": 3351870600
    },
    */

	bool something_worked = false;

	something_worked |= parse_numeric    (json, "$.git.uint32_t", version->git.commit);
	something_worked |= parse_string<41> (json, "$.git.hash",     version->git.hash);
	something_worked |= parse_string<128>(json, "$.git.describe", version->git.describe);
	something_worked |= parse_string<50> (json, "$.git.branch",   version->git.branch);
	something_worked |= parse_bool       (json, "$.git.dirty",    version->git.dirty);

	/*
	"build": {
        "configuration": "Debug",
        "host": "sonata.hep.wisc.edu",
        "human_date": "Mon Nov 11 12:29:34 EST 2019",
        "machine_date": "2019-11-11T12:29:34-0500",
        "user": "jtikalsky"
    },
	*/

	something_worked |= parse_string<128>(json, "$.build.configuration", version->build.configuration);
	something_worked |= parse_string<128>(json, "$.build.host",          version->build.host);
	something_worked |= parse_string<128>(json, "$.build.user",          version->build.user);
	something_worked |= parse_string<128>(json, "$.build.human_date",    version->build.human_date);
	something_worked |= parse_string<128>(json, "$.build.machine_date",  version->build.machine_date);

	/*
    "version": {
        "dirty": true,
        "extra": "",
        "major": 1,
        "minor": 0,
        "plus_commits": 0,
        "revision": 5,
        "tag": "APd1",
        "version": "1.0.5"
    }
    */

	something_worked |= parse_string<128>(json, "$.version.tag",          version->version.tag);
	something_worked |= parse_string<128>(json, "$.version.version",      version->version.version);
	something_worked |= parse_numeric    (json, "$.version.major",        version->version.major);
	something_worked |= parse_numeric    (json, "$.version.minor",        version->version.minor);
	something_worked |= parse_numeric    (json, "$.version.revision",     version->version.revision);
	something_worked |= parse_string<128>(json, "$.version.extra",        version->version.extra);
	something_worked |= parse_numeric    (json, "$.version.plus_commits", version->version.plus_commits);
	something_worked |= parse_bool       (json, "$.version.dirty",        version->version.dirty);

	// "summary": "Version APd1-v1.0.5-dirty (c7c97c88), built on jtikalsky@sonata.hep.wisc.edu at Mon Nov 11 13:50:43 EST 2019",

	something_worked |= parse_string<256>(json, "$.summary", version->summary);

	if (something_worked)
		return version;
	else
		return NULL;
}

std::shared_ptr<const VersionInfo> VersionInfo::get_running_version() {
	static std::shared_ptr<const VersionInfo> running_version = NULL;
	// Only run parse() on the first call and then simply return a
	// shared ptr to running_version and each subsequent call.
	if (!running_version)
		running_version = parse(VERSION_INFO_STR);
	return running_version;
}
