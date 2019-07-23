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

#include "xilinx_image.h"
#include <stdio.h>
#include <string>
#include "md5.h"

#define MD5_CHECKSUM_SIZE 16
#define MAX_IMAGE_NAME_SIZE 256

//! Boot ROM header
typedef struct {
	union {
		struct {
			uint32_t interrupt_table[8];		// 0x000
			uint32_t width_detection;			// 0x020
			uint32_t image_identification;		// 0x024
			uint32_t excryption_status;			// 0x028
			uint32_t fsbl_or_user_defined1;		// 0x02C
			uint32_t source_offset;				// 0x030
			uint32_t image_length;				// 0x034
			uint32_t fsbl_load_address;			// 0x038
			uint32_t start_of_execution;		// 0x03C
			uint32_t total_image_length;		// 0x040
			uint32_t qspi_config_word;			// 0x044
			uint32_t checksum;					// 0x048
			uint32_t fsbl_or_user_defined2[20];	// 0x04C
			uint32_t partion_table;				// 0x09C, special?
			uint32_t register_init[512];		// 0x0A0
			uint32_t image_header[8];			// 0x8A0
			uint32_t partition_header[16];		// 0x8C0
		};
		uint32_t fields[576];
	};
} BootROMHeader;

//! Partition Header
typedef struct {
	union {
		struct {
			uint32_t image_word_len;
			uint32_t data_word_len;
			uint32_t partition_word_len;
			uint32_t load_addr;
			uint32_t exec_addr;
			uint32_t partition_start;
			union {
				uint32_t partition_attr;
				struct {
					uint32_t : 4;
					uint32_t device : 4; // 0-none, 1-PS, 2-PL, 3-INT
					uint32_t : 4;
					uint32_t checksum_type : 3; // 0-noCRC, rest-RFU
					uint32_t rsa : 1;
					uint32_t owner : 2; // 0-FSBL, 1-UBOOT, 2,3-reserved
					uint32_t : 14;
				};
			};
			uint32_t section_count;
			uint32_t partition_check_sum_offset;
			uint32_t image_header_offset;
			uint32_t ac_offset;
			uint32_t _padding2[4];
			uint32_t checksum;
		};
		uint32_t fields[16];
	};
} PartHeader;

//! Image Header
typedef struct {
	uint32_t next_image_offset;
	uint32_t first_partition_offset;
	uint32_t partition_count; // Always zero
	uint32_t image_name_length;
	// image name follows
} ImageHeader;

const char* getBootFileValidationErrorString(BootFileValidationReturn r) {
	switch (r) {
	case BFV_VALID: return "Valid";
	case BFV_INVALID_BOOTROM: return "Invalid BootROM header";
	case BFV_INVALID_SIZE: return "Internal reference goes outside size boundaries";
	case BFV_NOT_ENOUGH_PARTITIONS: return "Not enough partitions";
	case BFV_INVALID_PARTITION: return "Invalid partition header";
	case BFV_UNKNOWN_PARTITION_TYPE: return "Unknown partition type";
	case BFV_MD5_REQUIRED: return "Partition missing MD5";
	case BFV_MD5_CHECK_FAILED: return "md5 check failed in one of the partitions";
	case BFV_UNEXPECTED_ORDER: return "Partitions are out of order";
	}

	return "";
}

std::string getImageNameFromHeader(ImageHeader *header) {
	if (header->image_name_length > MAX_IMAGE_NAME_SIZE) return "?";
	char *imageName = (char*)header + sizeof(ImageHeader);

	std::string name = "";
	for (size_t i = 0; i < 64; i++) {
		for (size_t k = 0; k < 4; k++) {
			char c = imageName[i * 4 + (3 - k)];
			if (c == 0x00 || c == 0xFF) goto ret; // break nested loop
			name.push_back(c);
		}
	}

ret:
	return name;
}

uint32_t calculateChecksum(uint32_t *array, size_t wordcount)
{
	uint32_t checksum = 0;

	// Sum
	for (size_t i = 0; i < wordcount; i++) {
		checksum += array[i];
	}

	// Invert
	checksum ^= 0xffffffff;

	return checksum;
}

#define MAX_NUM_PARTITIONS 10

size_t countPartitionsFromTable(PartHeader *partitionTable) {
	size_t count = 0;

	for (count = 0; count < MAX_NUM_PARTITIONS; count++) {
		// Detect last entry
		if (partitionTable[count].checksum != 0xffffffff) continue;
		for (size_t i = 0; i < sizeof(PartHeader)/4 ; i++) {
			if (partitionTable[count].fields[i] != 0) continue;
		}
		break;
	}

	return count;
}

BootFileValidationReturn validateBootFile(uint8_t *binfile, size_t size) {
	// An IPMC image must have 3 partition (FSBL, PL, PS) in this order
	const uint8_t expected_order[3] = {1, 2, 1};

	if (size < sizeof(BootROMHeader))
		return BFV_INVALID_SIZE;

	BootROMHeader *bootROMheader = (BootROMHeader*)binfile;
	uint32_t bootROMChecksum = calculateChecksum(bootROMheader->fields+8, 10);

	// Do some checks
	if ((bootROMheader->width_detection      != 0xaa995566) ||
		(bootROMheader->image_identification != 0x584c4e58) ||
		(bootROMheader->checksum            != bootROMChecksum))
		return BFV_INVALID_BOOTROM;

	// BootROM seems fine, goes to first partition
	if (size < (bootROMheader->partion_table + sizeof(PartHeader)*MAX_NUM_PARTITIONS))
		return BFV_INVALID_SIZE;

	PartHeader *partitionTable = (PartHeader*)(binfile + bootROMheader->partion_table);
	size_t partition_count = countPartitionsFromTable(partitionTable);

	if (partition_count != 3)
		return BFV_NOT_ENOUGH_PARTITIONS; // Only accept bitfiles with 3 images

	for (size_t i = 0; i < partition_count; i++) {
		PartHeader *partition_header = &(partitionTable[i]);

		uint32_t partChecksum = calculateChecksum(partition_header->fields, sizeof(PartHeader)/4 - 1);

		if (partition_header->checksum != partChecksum)
			return BFV_INVALID_PARTITION;

#ifdef XILINXIMAGE_DEBUG
		// Retrieve image name
		std::string imageName = getImageNameFromHeader((ImageHeader*)(binfile + (partition_header->image_header_offset << 2)));
		printf("Image %d: %s", i+1, imageName.c_str());
#endif

		if (size < ((partition_header->partition_start + partition_header->partition_word_len) << 2))
			return BFV_INVALID_SIZE;

		// Check the type of partition
		if (partition_header->device != 1 && partition_header->device != 2)
			return BFV_UNKNOWN_PARTITION_TYPE; // Invalid/unsupported type

		// Check if the partitions are in the correct order
		if (partition_header->device != expected_order[i])
			return BFV_UNEXPECTED_ORDER;

		if (partition_header->checksum_type > 0) {
			// Validate md5 checksum
			uint8_t *partition_start = binfile + (partition_header->partition_start << 2); // Words to bytes
			size_t partitionSize = partition_header->partition_word_len << 2; // Words to bytes
			uint8_t *partitionChecksum = binfile + (partition_header->partition_check_sum_offset << 2); // Words to bytes

			if (size < (partition_header->partition_check_sum_offset << 2) + MD5_CHECKSUM_SIZE)
				return BFV_INVALID_SIZE;

			// Calculate the md5 checksum
			uint8_t md5checksum[MD5_CHECKSUM_SIZE];
			md5(partition_start, partitionSize, md5checksum, 0);

			// Compare md5 checksums
			for (size_t k = 0; k < MD5_CHECKSUM_SIZE; k++) {
				if (partitionChecksum[k] != md5checksum[k])
					return BFV_MD5_CHECK_FAILED; // md5 checksum failed
			}

		} else {
#ifdef XILINXIMAGE_MD5_REQUIRED
			if (i > 0) {
				// If current partition is NOT the FSBL then we require to have MD5 present
				return BFV_MD5_REQUIRED;
			}
#else
#ifdef XILINXIMAGE_DEBUG
			printf("WARNING: Partition %d has no checksum!", i);
#endif
#endif
		}
	}

	return BFV_VALID; // Valid
}
