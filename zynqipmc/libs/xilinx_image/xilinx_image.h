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
 * @return true if the image passed all checks, false otherwise.
 */
BootFileValidationReturn validateBootFile(uint8_t *binfile, size_t size);

#endif /* SRC_COMMON_ZYNQIPMC_LIBS_XILINXIMAGE_H_ */
