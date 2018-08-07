/*
 * XilinxImage.cpp
 *
 *  Created on: Aug 7, 2018
 *      Author: mpv
 */

#include "XilinxImage.h"
#include <stdio.h>

#ifdef XILINXIMAGE_DEBUG
#define XILIMG_DBG_PRINTF(...) printf(__VA_ARGS__)
#else
#define XILIMG_DBG_PRINTF(...)
#endif

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
			uint32_t checkSumOffset;
			uint32_t _padding1[1];
			uint32_t acOffset;
			uint32_t _padding2[4];
			uint32_t checksum;
		};
		uint32_t fields[16];
	};
} PartHeader;

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

size_t countPartitionsFromTable(PartHeader *partitionTable){
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

// TODO: Added md5 checks to partiations that have it
// TODO: Check bitfile CRC
// TODO: Check orderer
// TODO: Check partition size vs file size
bool validateBootFile(uint8_t *binfile, size_t size) {
	// An IPMC image must have 3 partition (FSBL, PL, PS) in this order
	size_t plCount = 0, psCount = 0;

	if (size < sizeof(BootROMHeader)) return false;

	BootROMHeader *bootROMheader = (BootROMHeader*)binfile;
	uint32_t bootROMChecksum = calculateChecksum(bootROMheader->fields+8, 10);

	// Do some checks
	if ((bootROMheader->widthDetection      != 0xaa995566) ||
		(bootROMheader->imageIdentification != 0x584c4e58) ||
		(bootROMheader->checksum            != bootROMChecksum))
		return false;

	// BootROM seems fine, goes to first partition
	if (size < (bootROMheader->partionTable + sizeof(PartHeader)*MAX_NUM_PARTITIONS)) return false;
	PartHeader *partitionTable = (PartHeader*)(binfile + bootROMheader->partionTable);
	size_t partitionCount = countPartitionsFromTable(partitionTable);

	if (partitionCount != 3) return false; // Only accept bitfiles with 3 images

	for (size_t i = 0; i < partitionCount; i++) {
		PartHeader *partitionHeader = &(partitionTable[i]);

		uint32_t partChecksum = calculateChecksum(partitionHeader->fields,
							sizeof(PartHeader)/4 - 1);

		if ((partitionHeader->imageWordLen > size) ||
			(partitionHeader->checksum != partChecksum))
			return false;

		if (partitionHeader->checksumType > 0) {
			// TODO: Validate md5 checksum
		} else {
			XILIMG_DBG_PRINTF("WARNING: Partition %d has no checksum!\n", i);
		}

		switch (partitionHeader->device) {
		case 1: // PS
			psCount++;
			break;
		case 2: // PL
			plCount++;
			break;
		default:
			return false; // Invalid/unsupported type
		}
	}

	if ((plCount != 1) || (psCount != 2)) return false;

	return true; // Valid
}
