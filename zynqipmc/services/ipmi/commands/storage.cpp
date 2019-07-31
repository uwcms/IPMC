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

#include <core.h>
#include <libs/printf.h>
#include <libs/threading.h>
#include <services/ipmi/commands/ipmicmd_index.h>
#include <services/ipmi/ipmbsvc/ipmbsvc.h>
#include <services/ipmi/ipmi.h>
#include <services/ipmi/sdr/sensor_data_repository.h>
#include <services/persistentstorage/persistent_storage.h>

#define RETURN_ERROR(ipmb, message, completion_code) \
	do { \
		ipmb.send(message.prepareReply({completion_code})); \
		return; \
	} while (0)

// FRU Device Commands

#define FRU_AREA_SIZE 1024

static void ipmicmd_Get_FRU_Inventory_Area_Info(IPMBSvc &ipmb, const IPMIMessage &message) {
	if (message.data_len != 1)
		RETURN_ERROR(ipmb, message, IPMI::Completion::Invalid_Data_Field_In_Request);
	if (message.data[0] != 0) // We only support one FRU Device
		RETURN_ERROR(ipmb, message, IPMI::Completion::Parameter_Out_Of_Range);
	// We'll claim a perfectly reasonable FRU_AREA_SIZE byte FRU Data Area
	ipmb.send(message.prepareReply({IPMI::Completion::Success, (FRU_AREA_SIZE & 0xFF), (FRU_AREA_SIZE >> 8), 0}));
}
IPMICMD_INDEX_REGISTER(Get_FRU_Inventory_Area_Info);

static void ipmicmd_Read_FRU_Data(IPMBSvc &ipmb, const IPMIMessage &message) {
	if (message.data_len != 4)
		RETURN_ERROR(ipmb, message, IPMI::Completion::Invalid_Data_Field_In_Request);
	if (message.data[0] != 0) // We only support one FRU Device
		RETURN_ERROR(ipmb, message, IPMI::Completion::Parameter_Out_Of_Range);

	uint16_t offset = (message.data[2] << 8) | message.data[1];
	uint8_t read_count = message.data[3];
	if (1+read_count > IPMIMessage::max_data_len)
		RETURN_ERROR(ipmb, message, IPMI::Completion::Cannot_Return_Requested_Number_Of_Data_Bytes);
	if (offset >= FRU_AREA_SIZE)
		RETURN_ERROR(ipmb, message, IPMI::Completion::Cannot_Return_Requested_Number_Of_Data_Bytes);
	std::vector<uint8_t> reply{IPMI::Completion::Success, 0};
	safe_init_static_mutex(fru_data_mutex, false);
	MutexGuard<false> lock(fru_data_mutex, true);
	size_t i = offset;
	for (; i < offset+read_count && i < fru_data.size(); ++i)
		reply.push_back(fru_data[i]); // Return data
	for (; i < offset+read_count && i < FRU_AREA_SIZE; ++i)
		reply.push_back(0); // Fill it out with 0s.
	lock.release();
	reply[1] = reply.size() - 2;
	ipmb.send(message.prepareReply(reply));
}
IPMICMD_INDEX_REGISTER(Read_FRU_Data);

static void ipmicmd_Write_FRU_Data(IPMBSvc &ipmb, const IPMIMessage &message) {
	if (message.data_len < 3)
		RETURN_ERROR(ipmb, message, IPMI::Completion::Invalid_Data_Field_In_Request);
	if (message.data[0] != 0) // We only support one FRU Device
		RETURN_ERROR(ipmb, message, IPMI::Completion::Parameter_Out_Of_Range);

	uint16_t offset = (message.data[2] << 8) | message.data[1];
	std::vector<uint8_t> write_bytes(message.data+3, message.data+message.data_len);
	safe_init_static_mutex(fru_data_mutex, false);
	MutexGuard<false> lock(fru_data_mutex, true);
	if (fru_data.size() < offset + write_bytes.size())
		fru_data.resize(offset + write_bytes.size(), 0);
	uint8_t bytes_written = 0;
	for (auto it = write_bytes.begin(), eit = write_bytes.end(); it != eit; ++it) {
		if (offset > FRU_AREA_SIZE)
			break; // We claim to be a perfectly reasonable FRU_AREA_SIZE byte area.  Enforce it.
		fru_data[offset + bytes_written++] = *it;
	}
	VariablePersistentAllocation(*persistent_storage, PersistentStorageAllocations::WISC_FRU_DATA).setData(fru_data);
	lock.release();
	ipmb.send(message.prepareReply({IPMI::Completion::Success, bytes_written}));
}
IPMICMD_INDEX_REGISTER(Write_FRU_Data);

// SDR Device Commands

static void ipmicmd_Get_SDR_Repository_Info(IPMBSvc &ipmb, const IPMIMessage &message) {
	time_t last_update = sdr_repo.lastUpdateTimestamp();
	std::shared_ptr<IPMIMessage> reply = message.prepareReply();
	reply->data[0] = IPMI::Completion::Success;
	reply->data[1] = 0x51; // SDR Version (Spec 2.0: 51h)
	reply->data[2] = sdr_repo.size() & 0xff; // Record Count LSB
	reply->data[3] = sdr_repo.size() >> 8;   // Record Count MSB
	reply->data[4] = 0xff; // Free Space LSB (0xffff = unspecified)
	reply->data[5] = 0xff; // Free Space MSB (0xffff = unspecified)
	reply->data[6] = (last_update >>  0) & 0xff; // Most Recent Addition Timestamp[0]
	reply->data[7] = (last_update >>  8) & 0xff; // Most Recent Addition Timestamp[1]
	reply->data[8] = (last_update >> 16) & 0xff; // Most Recent Addition Timestamp[2]
	reply->data[9] = (last_update >> 24) & 0xff; // Most Recent Addition Timestamp[3]
	reply->data[10] = (last_update >>  0) & 0xff; // Most Recent Deletion Timestamp[0]
	reply->data[11] = (last_update >>  8) & 0xff; // Most Recent Deletion Timestamp[1]
	reply->data[12] = (last_update >> 16) & 0xff; // Most Recent Deletion Timestamp[2]
	reply->data[13] = (last_update >> 24) & 0xff; // Most Recent Deletion Timestamp[3]
	reply->data[14] = 0; // Operation Support
	if (sdr_repo.size() >= 0xFFFE)
		reply->data[14] |= 1<<7; // [7]   Repository has been overflowed by an Add
	reply->data[14] |= 1<<5; // [6:5] 01b = Repository supports non-modal update only.
	reply->data[14] |= 1<<3; // [3]   Delete SDR Command Supported
	reply->data[14] |= 1<<2; // [2]   Partial Add Command Supported
	reply->data[14] |= 1<<1; // [1]   Reserve SDR Repository Command Supported
	reply->data[14] |= 0<<0; // [0]   0b = Get SDR Repository Allocation Information Command Not Supported
	reply->data_len = 15;
	ipmb.send(reply);
}
IPMICMD_INDEX_REGISTER(Get_SDR_Repository_Info);

#if 0 // Unimplemented.
static void ipmicmd_Get_SDR_Repository_Allocation_Info(IPMBSvc &ipmb, const IPMIMessage &message) {
	// Unimplemented
}
IPMICMD_INDEX_REGISTER(Get_SDR_Repository_Allocation_Info);
#endif

static void ipmicmd_Reserve_SDR_Repository_Storage(IPMBSvc &ipmb, const IPMIMessage &message) {
	// Required. PICMG 3.0 REQ 3.353
	uint16_t reservation = sdr_repo.reserve();
	ipmb.send(message.prepareReply({IPMI::Completion::Success, static_cast<uint8_t>(reservation & 0xFF), static_cast<uint8_t>(reservation >> 8)}));
}
IPMICMD_INDEX_REGISTER(Reserve_SDR_Repository_Storage);

static void ipmicmd_Get_SDR(IPMBSvc &ipmb, const IPMIMessage &message) {
	if (message.data_len != 6)
		RETURN_ERROR(ipmb, message, IPMI::Completion::Request_Data_Length_Invalid);
	//uint16_t reservation = (message.data[1] << 8) | message.data[0]; // not used
	uint16_t record_id = (message.data[3] << 8) | message.data[2];
	if (record_id == 0xFFFF)
		record_id = sdr_repo.size() - 1;
	std::shared_ptr<const SensorDataRecord> record;
	try {
		record = sdr_repo.get(record_id);
	}
	catch (SensorDataRepository::reservation_cancelled_error) {
		RETURN_ERROR(ipmb, message, IPMI::Completion::Reservation_Cancelled);
	}
	if (!record)
		RETURN_ERROR(ipmb, message, IPMI::Completion::Requested_Sensor_Data_Or_Record_Not_Present);
	uint16_t next_record = record_id + 1;
	if (next_record >= sdr_repo.size())
		next_record = 0xFFFF;
	std::vector<uint8_t> reply{IPMI::Completion::Success, static_cast<uint8_t>(next_record & 0xFF), static_cast<uint8_t>(next_record >> 8)};
	std::vector<uint8_t> sdrdata = record->u8export(message.rsSA, 0);
	size_t i = message.data[4]; // offset
	size_t limit = message.data[4] + message.data[5] /* bytes to read */;
	if (limit > sdrdata.size())
		limit = sdrdata.size();
	for (; i < limit; ++i)
		reply.push_back(sdrdata[i]);
	if (reply.size() > IPMIMessage::max_data_len)
		RETURN_ERROR(ipmb, message, IPMI::Completion::Cannot_Return_Requested_Number_Of_Data_Bytes);
	ipmb.send(message.prepareReply(reply));
}
IPMICMD_INDEX_REGISTER(Get_SDR);

static void ipmicmd_Add_SDR(IPMBSvc &ipmb, const IPMIMessage &message) {
	std::vector<uint8_t> sdrdata(message.data, message.data+message.data_len);
	std::shared_ptr<SensorDataRecord> record = SensorDataRecord::interpret(sdrdata);
	if (!record) {
		std::string error = "A partial add for SDR data";
		for (auto it = sdrdata.begin(), eit = sdrdata.end(); it != eit; ++it)
			error += stdsprintf(" %02hhx", *it);
		error += " was rejected because we could not interpret it.";
		ipmb.logroot["commands"].log(error, LogTree::LOG_WARNING);
		RETURN_ERROR(ipmb, message, IPMI::Completion::Invalid_Data_Field_In_Request);
	}
	try {
		record->validate();
	}
	catch (SensorDataRecord::invalid_sdr_error) {
		std::string error = "A partial add for SDR data";
		for (auto it = sdrdata.begin(), eit = sdrdata.end(); it != eit; ++it)
			error += stdsprintf(" %02hhx", *it);
		error += " was rejected because we could not validate it.";
		ipmb.logroot["commands"].log(error, LogTree::LOG_WARNING);
		RETURN_ERROR(ipmb, message, IPMI::Completion::Invalid_Data_Field_In_Request);
	}
	sdr_repo.add(*record, 0);
	ipmb.send(message.prepareReply({IPMI::Completion::Success}));
}
IPMICMD_INDEX_REGISTER(Add_SDR);

static void ipmicmd_Partial_Add_SDR(IPMBSvc &ipmb, const IPMIMessage &message) {
	if (message.data_len < 7)
		RETURN_ERROR(ipmb, message, IPMI::Completion::Request_Data_Length_Invalid);
	uint16_t reservation = (message.data[1] << 8) | message.data[0];
	uint16_t record_id = (message.data[3] << 8) | message.data[2];
	uint8_t offset = message.data[3];
	bool commit = message.data[4] & 1;
	std::vector<uint8_t> sdrdata(message.data+7, message.data+message.data_len);

	if (reservation != sdr_repo.getCurrentReservation())
		RETURN_ERROR(ipmb, message, IPMI::Completion::Reservation_Cancelled);

	// Create a static map of partial adds.
	// This may be called from two IPMBs simultaneously.  Be safe.
	static SemaphoreHandle_t partials_mutex = nullptr;
	safe_init_static_mutex(partials_mutex, false);
	MutexGuard<false> lock(partials_mutex, true);
	static uint16_t last_reservation = 0;
	static std::map< uint8_t, std::vector<uint8_t> > partials;

	// If the reservation has changed, all partials are now invalid.
	if (reservation != last_reservation)
		partials.clear();
	last_reservation = reservation;

	// If the record id is 0x0000 and the offset is 0, create it.
	if (record_id == 0 && offset == 0) {
		for (uint8_t rid = 0xFE; rid; --rid) {
			if (!partials.count(rid)) {
				record_id = 0xFF00 | rid;
				partials.insert(std::make_pair(rid, std::vector<uint8_t>()));
				break;
			}
		}
	}

	// Verify that the given target is a known partial.
	if ((record_id & 0xFF00) != 0xFF || !partials.count(record_id & 0xFF))
		RETURN_ERROR(ipmb, message, IPMI::Completion::Requested_Sensor_Data_Or_Record_Not_Present);

	// We know we have a target in partials.
	std::vector<uint8_t> &partial = partials.at(record_id & 0xFF);
	if (partial.size() != offset) // We're writing somewhere other than the immediate end.
		RETURN_ERROR(ipmb, message, IPMI::Completion::Invalid_Data_Field_In_Request);

	// Append the data, since the offset could only be the end.
	partial.insert(partial.end(), sdrdata.begin(), sdrdata.end());

	if (commit) {
		if (partial.size() >= 5) {
			// We have a length so we can actually do the length check.
			if (partial.size() != (5U + partial[4])) {
				std::string error = "A partial add for SDR data";
				for (auto it = partial.begin(), eit = partial.end(); it != eit; ++it)
					error += stdsprintf(" %02hhx", *it);
				error += " does not match the length specified in the record.";
				ipmb.logroot["commands"].log(error, LogTree::LOG_WARNING);
				RETURN_ERROR(ipmb, message, 0x80 /* Specified in message definition as: Rejected due to size mismatch */);
			}
		}
		std::shared_ptr<SensorDataRecord> record = SensorDataRecord::interpret(partial);
		if (!record) {
			std::string error = "A partial add for SDR data";
			for (auto it = partial.begin(), eit = partial.end(); it != eit; ++it)
				error += stdsprintf(" %02hhx", *it);
			error += " was rejected because we could not interpret it.";
			ipmb.logroot["commands"].log(error, LogTree::LOG_WARNING);
			RETURN_ERROR(ipmb, message, IPMI::Completion::Invalid_Data_Field_In_Request);
		}
		try {
			record->validate();
			record_id = sdr_repo.add(*record, reservation);
		}
		catch (SensorDataRecord::invalid_sdr_error) {
			std::string error = "A partial add for SDR data";
			for (auto it = partial.begin(), eit = partial.end(); it != eit; ++it)
				error += stdsprintf(" %02hhx", *it);
			error += " was rejected because we could not validate it.";
			ipmb.logroot["commands"].log(error, LogTree::LOG_WARNING);
			RETURN_ERROR(ipmb, message, IPMI::Completion::Invalid_Data_Field_In_Request);
		}
		catch (SensorDataRepository::reservation_cancelled_error) {
			RETURN_ERROR(ipmb, message, IPMI::Completion::Reservation_Cancelled);
		}
	}
	ipmb.send(message.prepareReply({IPMI::Completion::Success, static_cast<uint8_t>(record_id & 0xff), static_cast<uint8_t>(record_id >> 8)}));
}
IPMICMD_INDEX_REGISTER(Partial_Add_SDR);

static void ipmicmd_Delete_SDR(IPMBSvc &ipmb, const IPMIMessage &message) {
	if (message.data_len != 4)
		RETURN_ERROR(ipmb, message, IPMI::Completion::Request_Data_Length_Invalid);
	uint16_t reservation = (message.data[1] << 8) | message.data[0];
	uint16_t record_id = (message.data[3] << 8) | message.data[2];
	if (reservation != sdr_repo.getCurrentReservation())
		RETURN_ERROR(ipmb, message, IPMI::Completion::Reservation_Cancelled);
	try {
		if (!sdr_repo.remove(record_id, reservation))
			RETURN_ERROR(ipmb, message, IPMI::Completion::Requested_Sensor_Data_Or_Record_Not_Present);
	}
	catch (SensorDataRepository::reservation_cancelled_error) {
		RETURN_ERROR(ipmb, message, IPMI::Completion::Reservation_Cancelled);
	}
	ipmb.send(message.prepareReply({IPMI::Completion::Success, message.data[2], message.data[3]}));
}
IPMICMD_INDEX_REGISTER(Delete_SDR);

static void ipmicmd_Clear_SDR_Repository(IPMBSvc &ipmb, const IPMIMessage &message) {
	if (message.data_len != 6)
		RETURN_ERROR(ipmb, message, IPMI::Completion::Request_Data_Length_Invalid);
	uint16_t reservation = (message.data[1] << 8) | message.data[0];
	if (reservation != sdr_repo.getCurrentReservation())
		RETURN_ERROR(ipmb, message, IPMI::Completion::Reservation_Cancelled);
	if (message.data[2] != 'C' || message.data[3] != 'L' || message.data[4] != 'R')
		RETURN_ERROR(ipmb, message, IPMI::Completion::Invalid_Data_Field_In_Request);

	try {
		sdr_repo.clear(reservation);
	}
	catch (SensorDataRepository::reservation_cancelled_error) {
		RETURN_ERROR(ipmb, message, IPMI::Completion::Reservation_Cancelled);
	}
	ipmb.send(message.prepareReply({IPMI::Completion::Success}));
}
IPMICMD_INDEX_REGISTER(Clear_SDR_Repository);

#if 0 // Unimplemented.
static void ipmicmd_Get_SDR_Repository_Time(IPMBSvc &ipmb, const IPMIMessage &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Get_SDR_Repository_Time);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Set_SDR_Repository_Time(IPMBSvc &ipmb, const IPMIMessage &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Set_SDR_Repository_Time);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Enter_SDR_Repository_Update_Mode(IPMBSvc &ipmb, const IPMIMessage &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Enter_SDR_Repository_Update_Mode);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Exit_SDR_Repository_Update_Mode(IPMBSvc &ipmb, const IPMIMessage &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Exit_SDR_Repository_Update_Mode);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Run_Initialization_Agent(IPMBSvc &ipmb, const IPMIMessage &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Run_Initialization_Agent);
#endif


// SEL Device Commands

#if 0 // Unimplemented.
static void ipmicmd_Get_SEL_Info(IPMBSvc &ipmb, const IPMIMessage &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Get_SEL_Info);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_SEL_Allocation_Info(IPMBSvc &ipmb, const IPMIMessage &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Get_SEL_Allocation_Info);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Reserve_SEL(IPMBSvc &ipmb, const IPMIMessage &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Reserve_SEL);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_SEL_Entry(IPMBSvc &ipmb, const IPMIMessage &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Get_SEL_Entry);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Add_SEL_Entry(IPMBSvc &ipmb, const IPMIMessage &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Add_SEL_Entry);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Partial_Add_SEL_Entry(IPMBSvc &ipmb, const IPMIMessage &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Partial_Add_SEL_Entry);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Delete_SEL_Entry(IPMBSvc &ipmb, const IPMIMessage &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Delete_SEL_Entry);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Clear_SEL(IPMBSvc &ipmb, const IPMIMessage &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Clear_SEL);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_SEL_Time(IPMBSvc &ipmb, const IPMIMessage &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Get_SEL_Time);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Set_SEL_Time(IPMBSvc &ipmb, const IPMIMessage &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Set_SEL_Time);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Auxiliary_Log_Status(IPMBSvc &ipmb, const IPMIMessage &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Get_Auxiliary_Log_Status);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Set_Auxiliary_Log_Status(IPMBSvc &ipmb, const IPMIMessage &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Set_Auxiliary_Log_Status);
#endif

