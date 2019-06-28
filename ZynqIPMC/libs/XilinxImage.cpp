/*
 * XilinxImage.cpp
 *
 *  Created on: Aug 7, 2018
 *      Author: mpv
 *
 * Based on Xilinx FSBL code.
 */

#include "XilinxImage.h"
#include <stdio.h>
#include <string>
#include "md5.h"

#ifdef XILINXIMAGE_DEBUG
#define XILIMG_DBG_PRINTF(...) printf(__VA_ARGS__)
#else
#define XILIMG_DBG_PRINTF(...)
#endif

#define MD5_CHECKSUM_SIZE 16
#define MAX_IMAGE_NAME_SIZE 256

///! Boot ROM header
typedef struct {
	union {
		struct {
			uint32_t interruptTable[8];			// 0x000
			uint32_t widthDetection;			// 0x020
			uint32_t imageIdentification;		// 0x024
			uint32_t excryptionStatus;			// 0x028
			uint32_t fsblOrUserDefined1;		// 0x02C
			uint32_t sourceOffset;				// 0x030
			uint32_t imageLength;				// 0x034
			uint32_t fsblLoadAddress;			// 0x038
			uint32_t startOfExecution;			// 0x03C
			uint32_t totalImageLength;			// 0x040
			uint32_t qspiConfigWord;			// 0x044
			uint32_t checksum;					// 0x048
			uint32_t fsblOrUserDefined2[20];	// 0x04C
			uint32_t partionTable;				// 0x09C, special?
			uint32_t registerInit[512];			// 0x0A0
			uint32_t imageHeader[8];			// 0x8A0
			uint32_t partitionHeader[16];		// 0x8C0
		};
		uint32_t fields[576];
	};
} BootROMHeader;

///! Partition Header
typedef struct {
	union {
		struct {
			uint32_t imageWordLen;
			uint32_t dataWordLen;
			uint32_t partitionWordLen;
			uint32_t loadAddr;
			uint32_t execAddr;
			uint32_t partitionStart;
			union {
				uint32_t partitionAttr;
				struct {
					uint32_t : 4;
					uint32_t device : 4; // 0-none, 1-PS, 2-PL, 3-INT
					uint32_t : 4;
					uint32_t checksumType : 3; // 0-noCRC, rest-RFU
					uint32_t rsa : 1;
					uint32_t owner : 2; // 0-FSBL, 1-UBOOT, 2,3-reserved
					uint32_t : 14;
				};
			};
			uint32_t sectionCount;
			uint32_t partitionCheckSumOffset;
			uint32_t imageHeaderOffset;
			uint32_t acOffset;
			uint32_t _padding2[4];
			uint32_t checksum;
		};
		uint32_t fields[16];
	};
} PartHeader;

///! Image Header
typedef struct {
	uint32_t nextImageOffset;
	uint32_t firstPartitionOffset;
	uint32_t partitionCount; // Always zero
	uint32_t imageNameLength;
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
	if (header->imageNameLength > MAX_IMAGE_NAME_SIZE) return "?";
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
	if ((bootROMheader->widthDetection      != 0xaa995566) ||
		(bootROMheader->imageIdentification != 0x584c4e58) ||
		(bootROMheader->checksum            != bootROMChecksum))
		return BFV_INVALID_BOOTROM;

	// BootROM seems fine, goes to first partition
	if (size < (bootROMheader->partionTable + sizeof(PartHeader)*MAX_NUM_PARTITIONS))
		return BFV_INVALID_SIZE;

	PartHeader *partitionTable = (PartHeader*)(binfile + bootROMheader->partionTable);
	size_t partitionCount = countPartitionsFromTable(partitionTable);

	if (partitionCount != 3)
		return BFV_NOT_ENOUGH_PARTITIONS; // Only accept bitfiles with 3 images

	for (size_t i = 0; i < partitionCount; i++) {
		PartHeader *partitionHeader = &(partitionTable[i]);

		uint32_t partChecksum = calculateChecksum(partitionHeader->fields, sizeof(PartHeader)/4 - 1);

		if (partitionHeader->checksum != partChecksum)
			return BFV_INVALID_PARTITION;

#ifdef XILINXIMAGE_DEBUG
		// Retrieve image name
		std::string imageName = getImageNameFromHeader((ImageHeader*)(binfile + (partitionHeader->imageHeaderOffset << 2)));
		XILIMG_DBG_PRINTF("Image %d: %s", i+1, imageName.c_str());
#endif

		if (size < ((partitionHeader->partitionStart + partitionHeader->partitionWordLen) << 2))
			return BFV_INVALID_SIZE;

		// Check the type of partition
		if (partitionHeader->device != 1 && partitionHeader->device != 2)
			return BFV_UNKNOWN_PARTITION_TYPE; // Invalid/unsupported type

		// Check if the partitions are in the correct order
		if (partitionHeader->device != expected_order[i])
			return BFV_UNEXPECTED_ORDER;

		if (partitionHeader->checksumType > 0) {
			// Validate md5 checksum
			uint8_t *partitionStart = binfile + (partitionHeader->partitionStart << 2); // Words to bytes
			size_t partitionSize = partitionHeader->partitionWordLen << 2; // Words to bytes
			uint8_t *partitionChecksum = binfile + (partitionHeader->partitionCheckSumOffset << 2); // Words to bytes

			if (size < (partitionHeader->partitionCheckSumOffset << 2) + MD5_CHECKSUM_SIZE)
				return BFV_INVALID_SIZE;

			// Calculate the md5 checksum
			uint8_t md5checksum[MD5_CHECKSUM_SIZE];
			md5(partitionStart, partitionSize, md5checksum, 0);

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
			XILIMG_DBG_PRINTF("WARNING: Partition %d has no checksum!", i);
#endif
		}
	}

	return BFV_VALID; // Valid
}
