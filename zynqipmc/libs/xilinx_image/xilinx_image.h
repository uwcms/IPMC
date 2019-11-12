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

#ifndef SRC_COMMON_ZYNQIPMC_LIBS_XILINXIMAGE_H_
#define SRC_COMMON_ZYNQIPMC_LIBS_XILINXIMAGE_H_

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <misc/version.h>

class BootConfig;

//! Uncomment to enabled debugging
#define XILINXIMAGE_DEBUG

//! Uncomment to fail verification if any partition (with exception of the FSBL) doesn't have MD5
#define XILINXIMAGE_MD5_REQUIRED

///! All possible error codes returned by validateBootFile
enum BootFileValidationReturn {
	BFV_VALID = 0,
	BFV_INVALID_BOOTROM,
	BFV_INVALID_SIZE,
	BFV_NOT_ENOUGH_PARTITIONS,
	BFV_INVALID_PARTITION,
	BFV_UNKNOWN_PARTITION_TYPE,
	BFV_MD5_REQUIRED,
	BFV_MD5_CHECK_FAILED,
	BFV_UNEXPECTED_ORDER,
	BFV_WRONG_ZYNQ_PART,
	BFV_NO_VERSION_INFO,
	BFV_NO_TAG_LOCK_MATCH,
};

/**
 * Converted the error code returned from validateBootFile to a readable string.
 * @param r The returned error code from validateBootFile.
 * @return String with the explanation of the error code.
 */
const char* getBootFileValidationErrorString(BootFileValidationReturn r);

/**
 * Validates a given BIN file in accordance with UG585, UG821 and UG470.
 * @param binfile A buffer in memory that holds the BIN raw data.
 * @param size The total number of bytes of the BIN file.
 * @param[out] message A human-readable informative message related to the output.
 * @param carrier_tag_config An optional BootConfig for carrier tag validation.
 * @return true if the image passed all checks, false otherwise.
 */
BootFileValidationReturn validateBootFile(const uint8_t *binfile, const size_t size, std::string &message, std::shared_ptr<const VersionInfo> &version, BootConfig *carrier_tag_config=NULL);

/**
 * Identify the ZYNQ part that the system is running on.
 * @return A string containing the likes of "xc7z020" or "UNKNOWN".
 */
struct zynqpart {
	union {
		uint32_t     idraw; // The raw ID read from the SCLR.PSS_IDCODE register.
		struct {
			uint32_t                    :  1; // [ 0: 0] Reserved
			uint32_t idraw_manufacturer : 11; // [11: 1] Manufacturer ID
			uint32_t idraw_device       :  5; // [16:12] Device code
			uint32_t idraw_subfamily    :  4; // [20:17] Subfamily
			uint32_t idraw_family       :  7; // [27:21] Family
			uint32_t idraw_revision     :  4; // [31:28] Revision
		};
	};
	union {
		uint32_t     id;    // Corrected for issues like AR# 47317.
		struct {
			uint32_t                 :  1; // [ 0: 0] Reserved
			uint32_t id_manufacturer : 11; // [11: 1] Manufacturer ID
			uint32_t id_device       :  5; // [16:12] Device code
			uint32_t id_subfamily    :  4; // [20:17] Subfamily
			uint32_t id_family       :  7; // [27:21] Family
			uint32_t id_revision     :  4; // [31:28] Revision
		};
	};
	std::string part_name;
};
/**
 * Parse the supplied (or currently running) ZYNQ part.
 * @param partId An optional part IDCODE to parse.  If 0, use the current part's ID.
 * @return A zynqpart struct dissecting the supplied or current IDCODE.
 */
const struct zynqpart zynqPartId(uint32_t partId = 0);

/**
 * Return the ZYNQ part id that the supplied bitstream is targeted at.
 * @param data Bitstream data
 * @param data_length Bitstream data length
 * @return The ZYNQ part id, or 0 on error.
 */
uint32_t zynqBitstreamPartId(const void *data, size_t data_length);

#endif /* SRC_COMMON_ZYNQIPMC_LIBS_XILINXIMAGE_H_ */
