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

/**
 * @file fru_data_init.cpp
 *
 * File contains application specific configurations about the Field
 * Replaceable Unit (FRU) that are forwarded to the ATCA crate shelf
 * when the card is inserted or powered.
 *
 * Change the operation of initFruData based on your specific application.
 */

#include <core.h>
#include <FreeRTOS.h>
#include <payload_manager.h>
#include <vector>

#include <services/ipmi/ipmbsvc/ipmbsvc.h>
#include <services/ipmi/ipmi_formats.h>
#include <services/persistentstorage/persistent_storage.h>
#include <misc/version.h>
#include "ipmc.h"

/**
 * Generate the appropriate headers (up to and excluding Record Format Version)
 * and add the PICMG multirecord to the provided FRU Data vector.
 *
 * @param fruarea The FRU Data area to be appended.
 * @param mrdata The multirecord to be added.
 * @param last_record true if the "end of list" flag should be set on this record, else false.
 * @param record_format The record format version, if not default.
 */
static void addPICMGMultirecord(std::vector<uint8_t> &fruarea, std::vector<uint8_t> mrdata, bool last_record, uint8_t record_format = 2) {
	static const std::vector<uint8_t> mrheader{
		0xC0, // "OEM", specified
		0x00, // [7] 1b=EOL (set later);  [3:0] record_format (set later)
		0, // Length (placeholder)
		0, // Record checksum (placeholder)
		0, // Header checksum (placeholder)
		0x5A, // Mfgr: PICMG, specified
		0x31, // Mfgr: PICMG, specified
		0x00, // Mfgr: PICMG, specified
	};

	mrdata.insert(mrdata.begin(), mrheader.begin(), mrheader.end());
	mrdata[1] = (last_record ? 0x80 : 0) | record_format;
	mrdata[2] = mrdata.size() - 5 /* Apparently this is record DATA length. */;
	mrdata[3] = IPMIMessage::checksum(std::vector<uint8_t>(std::next(mrdata.begin(), 5), mrdata.end()));
	mrdata[4] = IPMIMessage::checksum(std::vector<uint8_t>(mrdata.begin(), std::next(mrdata.begin(), 5)));

	fruarea.insert(fruarea.end(), mrdata.begin(), mrdata.end());
}

void initFruData(bool reinit) {
	std::vector<uint8_t> tlstring;

	std::shared_ptr<const VersionInfo> version = VersionInfo::get_running_version();

	std::vector<uint8_t> board_info;
	board_info.push_back(0x01); // Format Version
	board_info.push_back(0x00); // Length Placeholder
	board_info.push_back(25);   // Language Code (English)
	board_info.push_back(0x00); // Mfg Date/Time (Unspecified)
	board_info.push_back(0x00); // Mfg Date/Time (Unspecified)
	board_info.push_back(0x00); // Mfg Date/Time (Unspecified)
	tlstring = encodeIpmiTypeLengthField("University of Wisconsin");
	board_info.insert(board_info.end(), tlstring.begin(), tlstring.end()); // Board Mfgr.
	tlstring = encodeIpmiTypeLengthField("ZYNQ IPMC");
	board_info.insert(board_info.end(), tlstring.begin(), tlstring.end()); // Board Product Name
	tlstring = encodeIpmiTypeLengthField(std::to_string(IPMC_SERIAL));
	board_info.insert(board_info.end(), tlstring.begin(), tlstring.end()); // Board Serial
	tlstring = encodeIpmiTypeLengthField(std::string("IPMC Rev") + std::to_string(IPMC_HW_REVISION));
	board_info.insert(board_info.end(), tlstring.begin(), tlstring.end()); // Board Part Number
	tlstring = encodeIpmiTypeLengthField(version ? version->version.tag : "UNKNOWN");
	board_info.insert(board_info.end(), tlstring.begin(), tlstring.end()); // FRU File ID (in our case generating software)
	board_info.push_back(0xC1); // End of T/L Records.
	board_info.push_back(0); // Ensure at least one pad, to be used for checksum.
	while (board_info.size() % 8)
		board_info.push_back(0); // Pad.
	board_info[1] = board_info.size()/8; // Update length
	board_info.pop_back(); // Remove one pad for checksum.
	board_info.push_back(IPMIMessage::checksum(board_info));


	std::vector<uint8_t> product_info;
	product_info.push_back(0x01); // Format Version
	product_info.push_back(0x00); // Length Placeholder
	product_info.push_back(25);   // Language Code (English)
	tlstring = encodeIpmiTypeLengthField("University of Wisconsin");
	product_info.insert(product_info.end(), tlstring.begin(), tlstring.end()); // Mfgr Name
	tlstring = encodeIpmiTypeLengthField("ZYNQ IPMC");
	product_info.insert(product_info.end(), tlstring.begin(), tlstring.end()); // Product Name
	tlstring = encodeIpmiTypeLengthField(std::string("IPMC Rev") + std::to_string(IPMC_HW_REVISION));
	product_info.insert(product_info.end(), tlstring.begin(), tlstring.end()); // Product Part/Model Number
	tlstring = encodeIpmiTypeLengthField(std::to_string(IPMC_HW_REVISION));
	product_info.insert(product_info.end(), tlstring.begin(), tlstring.end()); // Product Version
		tlstring = encodeIpmiTypeLengthField(std::to_string(IPMC_SERIAL));
	product_info.insert(product_info.end(), tlstring.begin(), tlstring.end()); // Product Serial
	product_info.push_back(0xC0); // Asset Tag (NULL)
	tlstring = encodeIpmiTypeLengthField(version ? version->version.tag : "UNKNOWN");
	product_info.insert(product_info.end(), tlstring.begin(), tlstring.end()); // FRU File ID (in our case generating software)
	product_info.push_back(0xC1); // End of T/L Records.
	product_info.push_back(0); // Ensure at least one pad, to be used for checksum.
	while (product_info.size() % 8)
		product_info.push_back(0); // Pad.
	product_info[1] = product_info.size()/8; // Update length
	product_info.pop_back(); // Remove one pad for checksum.
	product_info.push_back(IPMIMessage::checksum(product_info));


	fru_data.resize(8);
	fru_data[0] = 0x01; // Common Header Format Version
	fru_data[1] = 0x00; // Internal Use Area Offset (multiple of 8 bytes)
	fru_data[2] = 0x00; // Chassis Info Area Offset (multiple of 8 bytes)
	fru_data[3] = 0x01; // Board Area Offset (multiple of 8 bytes)
	fru_data[4] = 0x01 + board_info.size()/8; // Product Info Area Offset (multiple of 8 bytes)
	fru_data[5] = 0x01 + board_info.size()/8 + product_info.size()/8; // Multi-Record Area Offset (multiple of 8 bytes)
	fru_data[6] = 0x00; // PAD, write as 00h
	fru_data[7] = 0x00;
	fru_data[7] = IPMIMessage::checksum(fru_data); // Checksum

	fru_data.insert(fru_data.end(), board_info.begin(), board_info.end());
	fru_data.insert(fru_data.end(), product_info.begin(), product_info.end());

	/* Board Point-to-Point Connectivity Record
	 *
	 * This block of code will generate this automatically based on the E-Keying
	 * link descriptors provided by the payload manager.  It does not need to be
	 * customized by a board integrator.  E-Keying link descriptions should be
	 * defined in the Payload Manager instead.
	 */
	{
		std::vector<uint8_t> bp2pcr{0x14, 0, 0};
		for (uint8_t i = 0xF0; i < 0xFF; ++i) {
			try {
				std::vector<uint8_t> guid = LinkDescriptor::lookup_oem_LinkType_guid(i);
				bp2pcr[2]++; // Increment number of GUIDs in record.
				bp2pcr.insert(bp2pcr.end(), guid.begin(), guid.end()); // Add GUID to record.
			} catch (std::out_of_range) {
				break;
			}
		}

		configASSERT(payload_manager); // We should be called only after this is initialized.

		std::vector<LinkDescriptor> links = payload_manager->getLinks();
		for (auto it = links.begin(), eit = links.end(); it != eit; ++it) {
			if (bp2pcr.size() > 255 /* record data limit */ - 3 /* MultiRec OEM ID header */ - 4 /* next link descriptor size */ - 1 /* safety margin */) {
				// We filled up this record we need to start a new one.
				addPICMGMultirecord(fru_data, bp2pcr, false);
				// Start new record, zero GUIDs in further records, they all fit in one (barely).
				bp2pcr = std::vector<uint8_t>{0x14, 0, 0};
			}

			std::vector<uint8_t> ld_vec(*it);
			bp2pcr.insert(bp2pcr.end(), ld_vec.begin(), ld_vec.end());
		}

		// We have at least one link or at least one GUID, or just need to say we have none.
		addPICMGMultirecord(fru_data, bp2pcr, false /* We are not the last record in the FRU Data. */);
	}

	/* Carrier Activation and Current Management record
	 * ...not that we have any AMC modules.
	 *
	 * This is supposed to specify the maximum power we can provide to our AMCs,
	 * and be used for validating our AMC modules' power requirements.
	 */
	addPICMGMultirecord(fru_data, std::vector<uint8_t>{0x17, 0, 0x3f /* ~75W for all AMCs (and self..?) LSB */, 0 /* MSB */, 5, 0}, true);

	runTask("persist_fru", TASK_PRIORITY_SERVICE, [reinit]() -> void {
		safe_init_static_mutex(fru_data_mutex, false);
		MutexGuard<false> lock(fru_data_mutex, true);
		VariablePersistentAllocation fru_persist(*persistent_storage, PersistentStorageAllocations::WISC_FRU_DATA);

		// If not reinitializing, and there's an area to read, replace ours, else write.
		std::vector<uint8_t> persist_data = fru_persist.getData();
		if (persist_data.size() && !reinit) {
			fru_data = persist_data;
		}

		// Store the newly initialized Device SDRs
		fru_persist.setData(fru_data);
		std::string out;
#if 0
		for (auto it = fru_data.begin(), eit = fru_data.end(); it != eit; ++it) {
			out += stdsprintf("%02hhx", *it);
		}
		printf("FRU Data \"%s\"\n", out.c_str());
#endif
	});
}

