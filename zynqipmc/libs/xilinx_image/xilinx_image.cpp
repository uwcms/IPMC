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
#include <libs/bootconfig/bootconfig.h>
#include <libs/printf.h>
#include <misc/version.h>
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
			uint32_t header_version;			// 0x02C
			uint32_t source_offset;				// 0x030
			uint32_t image_length;				// 0x034
			uint32_t fsbl_load_address;			// 0x038
			uint32_t start_of_execution;		// 0x03C
			uint32_t total_fsbl_length;			// 0x040
			uint32_t qspi_config_word;			// 0x044
			uint32_t checksum;					// 0x048
			uint32_t fsbl_or_user_defined2[19];	// 0x04C
			uint32_t image_header_tbl_offset;	// 0x098
			uint32_t partition_header_tbl_offset; // 0x09C
			uint32_t register_init[512];		// 0x0A0
		};
		uint32_t fields[552];
	};
} BootROMHeader;

//! Partition Header
typedef struct {
	union {
		struct {
			uint32_t image_word_len;				// 0x00
			uint32_t data_word_len;					// 0x04
			uint32_t partition_word_len;			// 0x08
			uint32_t load_addr;						// 0x0c
			uint32_t exec_addr;						// 0x10
			uint32_t partition_data_start;			// 0x14
			union {
				uint32_t partition_attr;			// 0x18
				struct {
					uint32_t : 4;        // Head Alignment, Tail Alignment: "Not Implemented" -ug1283
					uint32_t device : 4; // 0-none, 1-PS, 2-PL, 3-INT
					uint32_t : 4;        // Destination instance: "Not Implemented" -ug1283
					uint32_t checksum_type : 3; // 0-noCRC, rest-RFU
					uint32_t rsa : 1;    // RSA signature present
					uint32_t owner : 2;  // 0-FSBL, 1-UBOOT, 2,3-reserved
					uint32_t : 14;       // Data attributes: "Not Implemented" -ug1283
				};
			};
			uint32_t section_count;					// 0x1C
			uint32_t partition_check_sum_offset;	// 0x20
			uint32_t image_header_offset;			// 0x24
			uint32_t ac_offset;						// 0x28
			uint32_t _padding2[4];					// 0x2C-0x38 Reserved
			uint32_t checksum;						// 0x3C
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
	case BFV_WRONG_ZYNQ_PART: return "The supplied bitstream is for a different ZYNQ part than this one";
	case BFV_NO_VERSION_INFO: return "Unable to locate BOOT.BIN version information.";
	case BFV_NO_TAG_LOCK_MATCH: return "This BOOT.BIN does not match the image tag lock setting.";
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

static inline std::string extractPrintableString(const char *data, size_t size) {
	size_t i = 0;
	for (i = 0; i < size; ++i) {
		if (data[i] == '\0')
			return std::string(data); // We're done here. This is technically a valid C string.
		if (data[i] == '\r' || data[i] == '\n' || data[i] == '\t')
			continue; // Give these specific "non-printable" characters some slack.
		if (data[i] < ' ' || data[i] > '~')
			return NULL; // non-printable bytes in string.
	}
	// We got to the end without a null terminator.
	// This is technically okay, considering how the version string is constructed and embedded.
	return std::string(data, data+i);
}

BootFileValidationReturn validateBootFile(const uint8_t *binfile, const size_t size, std::string &message, std::shared_ptr<const VersionInfo> &version, BootConfig *carrier_tag_config) {
	message = "Valid Image"; // If we don't assign something better from version detection or an error, this is it.

	if (size < sizeof(BootROMHeader)) {
		message = getBootFileValidationErrorString(BFV_INVALID_SIZE);
		return BFV_INVALID_SIZE;
	}

	BootROMHeader *bootROMheader = (BootROMHeader*)binfile;
	uint32_t bootROMChecksum = calculateChecksum(bootROMheader->fields+8, 10);

	// Do some checks
	if ((bootROMheader->width_detection      != 0xaa995566) ||
		(bootROMheader->image_identification != 0x584c4e58) ||
		(bootROMheader->checksum            != bootROMChecksum)) {
		message = getBootFileValidationErrorString(BFV_INVALID_BOOTROM);
		return BFV_INVALID_BOOTROM;
	}

	// BootROM seems fine, goes to first partition
	if (size < (bootROMheader->partition_header_tbl_offset + sizeof(PartHeader)*MAX_NUM_PARTITIONS)) {
		message = getBootFileValidationErrorString(BFV_INVALID_SIZE);
		return BFV_INVALID_SIZE;
	}

	PartHeader *partitionTable = (PartHeader*)(binfile + bootROMheader->partition_header_tbl_offset);
	size_t partition_count = countPartitionsFromTable(partitionTable);

	if (partition_count < 3) {
		message = getBootFileValidationErrorString(BFV_NOT_ENOUGH_PARTITIONS);
		return BFV_NOT_ENOUGH_PARTITIONS; // Only accept bitfiles with 3 images
	}

	for (size_t i = 0; i < partition_count; i++) {
		PartHeader *partition_header = &(partitionTable[i]);

		uint32_t partChecksum = calculateChecksum(partition_header->fields, sizeof(PartHeader)/4 - 1);

		if (partition_header->checksum != partChecksum) {
			message = getBootFileValidationErrorString(BFV_INVALID_PARTITION);
			return BFV_INVALID_PARTITION;
		}

		// Retrieve image name
		std::string imageName = getImageNameFromHeader((ImageHeader*)(binfile + (partition_header->image_header_offset << 2)));
#ifdef XILINXIMAGE_DEBUG
		printf("Image %d: %s (%lu bytes)", i+1, imageName.c_str(), partition_header->data_word_len<<2);
#endif

		if (size < ((partition_header->partition_data_start + partition_header->data_word_len) << 2)) {
			message = getBootFileValidationErrorString(BFV_INVALID_SIZE);
			return BFV_INVALID_SIZE;
		}

		// Check the type of partition
		if (partition_header->device != 1 && partition_header->device != 2) {
			message = getBootFileValidationErrorString(BFV_UNKNOWN_PARTITION_TYPE);
			return BFV_UNKNOWN_PARTITION_TYPE; // Invalid/unsupported type
		}

		// Check if the (first three) partitions are in the correct order
		// An IPMC image must have 3 partition (FSBL, PL, PS) in this order
		const uint8_t expected_order[3] = {1, 2, 1};
		if (i < 3 && partition_header->device != expected_order[i]) {
			message = getBootFileValidationErrorString(BFV_UNEXPECTED_ORDER);
			return BFV_UNEXPECTED_ORDER;
		}

		const uint8_t *partitionDataStart = binfile + (partition_header->partition_data_start << 2); // Words to bytes
		const size_t partitionDataSize = partition_header->data_word_len << 2; // Words to bytes
		const uint8_t *partitionChecksum = binfile + (partition_header->partition_check_sum_offset << 2); // Words to bytes

		if (partition_header->checksum_type > 0) {
			// Validate md5 checksum
			if (size < (partition_header->partition_check_sum_offset << 2) + MD5_CHECKSUM_SIZE) {
				message = getBootFileValidationErrorString(BFV_INVALID_SIZE);
				return BFV_INVALID_SIZE;
			}

			// Calculate the md5 checksum
			uint8_t md5checksum[MD5_CHECKSUM_SIZE];
			md5(const_cast<uint8_t*>(partitionDataStart), partitionDataSize, md5checksum, 0);

			// Compare md5 checksums
			for (size_t k = 0; k < MD5_CHECKSUM_SIZE; k++) {
				if (partitionChecksum[k] != md5checksum[k]) {
					message = getBootFileValidationErrorString(BFV_MD5_CHECK_FAILED);
					return BFV_MD5_CHECK_FAILED; // md5 checksum failed
				}
			}

		} else {
			if (i > 0) {
#ifdef XILINXIMAGE_MD5_REQUIRED
				// If current partition is NOT the FSBL then we require to have MD5 present
				message = getBootFileValidationErrorString(BFV_MD5_REQUIRED);
				return BFV_MD5_REQUIRED;
#else
#ifdef XILINXIMAGE_DEBUG
			printf("WARNING: Partition %d has no checksum!", i);
#endif
#endif
			}
		}

		if (partition_header->device == 2 /* PL */) {
			// Read the bitstream and verify that it is for the ZYNQ part we are running on.
			if (size < (partition_header->partition_data_start << 2) + partitionDataSize) {
				message = getBootFileValidationErrorString(BFV_INVALID_SIZE);
				return BFV_INVALID_SIZE;
			}
			uint32_t bitpartid = zynqBitstreamPartId(partitionDataStart, partitionDataSize);
			if (!bitpartid) {
				message = "Unable to determine the ZYNQ part from this bitstream.";
				return BFV_WRONG_ZYNQ_PART;
			}
			struct zynqpart bitpart = zynqPartId(bitpartid);
			struct zynqpart runpart = zynqPartId();
			if ((bitpart.id & 0x00ffffff) != (runpart.id & 0x00ffffff)) {
				message = stdsprintf("Bitstream is for ZYNQ part %s (0x_%07lx), hardware is ZYNQ part %s (0x%08lx).", bitpart.part_name.c_str(), bitpart.id & 0x0fffffff, runpart.part_name.c_str(), runpart.id);
				return BFV_WRONG_ZYNQ_PART;
			}
		}

		if (imageName == "version.json") {
			// Extract version info.
			if (size < (partition_header->partition_data_start << 2) + partitionDataSize) {
				message = getBootFileValidationErrorString(BFV_INVALID_SIZE);
				return BFV_INVALID_SIZE;
			}
			std::string version_string = extractPrintableString(reinterpret_cast<const char*>(partitionDataStart), partitionDataSize);
			version = VersionInfo::parse(version_string);
			if (version)
				message = version->summary;
		}
	}

	if (!version) {
		message = "Unable to locate BOOT.BIN version information.";
		return BFV_NO_VERSION_INFO;
	}
	if (carrier_tag_config && !carrier_tag_config->testImageTagLock(version->version.tag)) {
		message = stdsprintf("This BOOT.BIN is from tag \"%s\". Our image tag lock setting is \"%s\".", version->version.tag.c_str(), carrier_tag_config->getImageTagLock().c_str());
		return BFV_NO_TAG_LOCK_MATCH;
	}

	return BFV_VALID; // Valid
}

const struct zynqpart zynqPartId(uint32_t partId) {
	const uint32_t * const sclr_idcode = reinterpret_cast<uint32_t*>(0xF8000530);

	if (!partId)
		partId = *sclr_idcode;

	struct zynqpart part;
	part.idraw = partId;
	part.id    = partId;

	if (part.id_family == 0x1D) {
		/* AR# 47317 -- This reads out wrong on some ZYNQ chips.
		 * We'll spoof it for the purposes of matching things below.
		 */
		part.id_family = 0x1B;
	}

	part.part_name = "UNKNOWN";

	if (part.id_family == 0x1B && part.id_subfamily == 0x9) {
		switch (part.id_device) {
		case 0x03: part.part_name = "7z007"; break;
		case 0x1c: part.part_name = "7z012s"; break;
		case 0x08: part.part_name = "7z014s"; break;
		case 0x02: part.part_name = "7z010"; break;
		case 0x1b: part.part_name = "7z015"; break;
		case 0x07: part.part_name = "7z020"; break;
		case 0x0c: part.part_name = "7z030"; break;
		case 0x12: part.part_name = "7z035"; break;
		case 0x11: part.part_name = "7z045"; break;
		case 0x16: part.part_name = "7z100"; break;
		default:   part.part_name = "UNKNOWN"; break;
		}
	}
	return part;
}

uint32_t zynqBitstreamPartId(const void *data, size_t data_length) {
	/* Whereas bootgen strips the bitfile header, leaving us with a bin file in BOOT.BIN,
	 * And whereas partitions on BOOT.BIN are necessarily word aligned, being word addressed,
	 * And whereas we do not expect our BOOT.BIN to be stored in an unaligned memory segment,
	 * We expect *data to be word-aligned, but will not crash if it is not.
	 */
	if ((reinterpret_cast<uint32_t>(data) % 4) || (data_length % 4))
		return 0; // We don't support unaligned stuff.

	typedef union {
		struct {
			 uint32_t             : 29; // [28: 0] (type dependent)
			 uint32_t packet_type :  3; // [31:29] packet type
		};
		struct { // Type 1 packet
			uint32_t word_count   : 11; // [10: 0] word count
			uint32_t              :  2; // [12:11] reserved
			uint32_t address      :  5; // [17:13] register address
			uint32_t              :  9; // [26:18] reserved
			uint32_t opcode       :  2; // [28:27] opcode
			uint32_t packet_type  :  3; // [31:29] packet type
		} type1;
		struct { // Type 2 packet
			uint32_t word_count   : 27; // [26: 0] word count
			uint32_t opcode       :  2; // [28:27] opcode
			uint32_t packet_type  :  3; // [31:29] packet type
		} type2;
		uint32_t data_word;
	} binstream_word_t;

	const binstream_word_t * const binstream = reinterpret_cast<const binstream_word_t*>(data);
	const size_t binstream_length = data_length >> 2;

	size_t offset = 0;
	// Locate the sync word.
	while (binstream[offset++].data_word != 0xaa995566) {
		if (offset >= binstream_length)
			return 0; // No sync word.
	}

	// Begin FPGA config packet parsing.
	uint8_t current_operation = 0; // 0: NOOP, 1: Read, 2: Write, 3: Reserved.
	uint16_t register_address = 0; // The register address currently being operated on.
	uint32_t operation_data_words = 0; // The total number of data words in the current operation.
	uint32_t data_words_remaining = 0; // The number of data words remaining.  0 = Expect instruction word.

	for (; offset < binstream_length; ++offset) {
		const binstream_word_t &word = binstream[offset];

		if (data_words_remaining == 0) {
			// Process an operation.
			if (binstream[offset].packet_type == 1) {
				current_operation    = word.type1.opcode;
				register_address     = word.type1.address;
				operation_data_words = word.type1.word_count;
				data_words_remaining = operation_data_words;
			}
			else if (word.packet_type == 2) {
				current_operation    = word.type2.opcode;
				operation_data_words = word.type2.word_count;
				data_words_remaining = operation_data_words;
			}
			else {
				return 0; // Unknown config packet type.
			}
		}
		else {
			--data_words_remaining; // Consume this data word.
			if (current_operation == 2 /* write */ && register_address == 0x0c /* IDCODE */ && operation_data_words == 1) {
				// This is the IDCODE word we've been hoping for.  Return it.
				return word.data_word;
			}
		}
	}
	return 0; // We never found the config packet we were looking for.
}
