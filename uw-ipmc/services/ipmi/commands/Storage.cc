#include <services/ipmi/IPMI.h>
#include <services/ipmi/ipmbsvc/IPMBSvc.h>
#include <services/ipmi/sdr/SensorDataRepository.h>
#include <libs/ThreadingPrimitives.h>
#include "IPMICmd_Index.h"

#define RETURN_ERROR(ipmb, message, completion_code) \
	do { \
		ipmb.send(message.prepare_reply({completion_code})); \
		return; \
	} while (0)

// FRU Device Commands

#define FRU_AREA_SIZE 1024

static void ipmicmd_Get_FRU_Inventory_Area_Info(IPMBSvc &ipmb, const IPMI_MSG &message) {
	if (message.data_len != 1)
		RETURN_ERROR(ipmb, message, IPMI::Completion::Invalid_Data_Field_In_Request);
	if (message.data[0] != 0) // We only support one FRU Device
		RETURN_ERROR(ipmb, message, IPMI::Completion::Parameter_Out_Of_Range);
	// We'll claim a perfectly reasonable FRU_AREA_SIZE byte FRU Data Area
	ipmb.send(message.prepare_reply({IPMI::Completion::Success, (FRU_AREA_SIZE & 0xFF), (FRU_AREA_SIZE >> 8), 0}));
}
IPMICMD_INDEX_REGISTER(Get_FRU_Inventory_Area_Info);

static void ipmicmd_Read_FRU_Data(IPMBSvc &ipmb, const IPMI_MSG &message) {
	if (message.data_len != 4)
		RETURN_ERROR(ipmb, message, IPMI::Completion::Invalid_Data_Field_In_Request);
	if (message.data[0] != 0) // We only support one FRU Device
		RETURN_ERROR(ipmb, message, IPMI::Completion::Parameter_Out_Of_Range);

	uint16_t offset = (message.data[2] << 8) | message.data[1];
	uint8_t read_count = message.data[3];
	if (1+read_count > IPMI_MSG::max_data_len)
		RETURN_ERROR(ipmb, message, IPMI::Completion::Cannot_Return_Requested_Number_Of_Data_Bytes);
	if (offset >= FRU_AREA_SIZE)
		RETURN_ERROR(ipmb, message, IPMI::Completion::Cannot_Return_Requested_Number_Of_Data_Bytes);
	std::vector<uint8_t> reply{IPMI::Completion::Success, 0};
	safe_init_static_mutex(fru_data_mutex, false);
	MutexGuard<false> lock(fru_data_mutex, true);
	int i = offset;
	for (; i < offset+read_count && i < fru_data.size(); ++i)
		reply.push_back(fru_data[i]); // Return data
	for (; i < offset+read_count && i < FRU_AREA_SIZE; ++i)
		reply.push_back(0); // Fill it out with 0s.
	lock.release();
	reply[1] = reply.size() - 2;
	ipmb.send(message.prepare_reply(reply));
}
IPMICMD_INDEX_REGISTER(Read_FRU_Data);

static void ipmicmd_Write_FRU_Data(IPMBSvc &ipmb, const IPMI_MSG &message) {
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
	lock.release();
	ipmb.send(message.prepare_reply({IPMI::Completion::Success, bytes_written}));
}
IPMICMD_INDEX_REGISTER(Write_FRU_Data);

// SDR Device Commands

static void ipmicmd_Get_SDR_Repository_Info(IPMBSvc &ipmb, const IPMI_MSG &message) {
	std::shared_ptr<IPMI_MSG> reply = message.prepare_reply();
	reply->data[0] = IPMI::Completion::Success;
	reply->data[1] = 0x51; // SDR Version (Spec 2.0: 51h)
	reply->data[2] = 0; // Record Count LSB TODO
	reply->data[3] = 0; // Record Count MSB TODO
	reply->data[4] = 0xff; // Free Space LSB (0xffff = unspecified)
	reply->data[5] = 0xff; // Free Space MSB (0xffff = unspecified)
	reply->data[6] = 0; // Most Recent Addition Timestamp[0]
	reply->data[7] = 0; // Most Recent Addition Timestamp[1]
	reply->data[8] = 0; // Most Recent Addition Timestamp[2]
	reply->data[9] = 0; // Most Recent Addition Timestamp[3]
	reply->data[10] = 0; // Most Recent Deletion Timestamp[0]
	reply->data[11] = 0; // Most Recent Deletion Timestamp[1]
	reply->data[12] = 0; // Most Recent Deletion Timestamp[2]
	reply->data[13] = 0; // Most Recent Deletion Timestamp[3]
	reply->data[14] = 0; // Operation Support
	if (sdr_repo.size() >= 0xFFFE)
		reply->data[14] |= 1<<7; // [7]   Repository has been overflowed by an Add
	reply->data[14] |= 1<<5; // [6:5] Repository supports non-modal update only.
	reply->data[14] |= 1<<3; // [3]   Delete SDR Command Supported
	reply->data[14] |= 1<<2; // [2]   Partial Add Command Supported
	reply->data[14] |= 1<<1; // [1]   Reserve SDR Repository Command Supported
	reply->data[14] |= 0<<0; // [0]   0b = Get SDR Repository Allocation Information Command Not Supported
	reply->data_len = 15;
	ipmb.send(reply);
}
IPMICMD_INDEX_REGISTER(Get_SDR_Repository_Info);

#if 0 // Unimplemented.
static void ipmicmd_Get_SDR_Repository_Allocation_Info(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// TODO: Required? PICMG 3.0 REQ 3.351
}
IPMICMD_INDEX_REGISTER(Get_SDR_Repository_Allocation_Info);
#endif

static void ipmicmd_Reserve_SDR_Repository_Storage(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Required. PICMG 3.0 REQ 3.353
	uint16_t reservation = sdr_repo.reserve();
	ipmb.send(message.prepare_reply({IPMI::Completion::Success, static_cast<uint8_t>(reservation & 0xFF), static_cast<uint8_t>(reservation >> 8)}));
}
IPMICMD_INDEX_REGISTER(Reserve_SDR_Repository_Storage);

static void ipmicmd_Get_SDR(IPMBSvc &ipmb, const IPMI_MSG &message) {
	if (message.data_len != 6)
		RETURN_ERROR(ipmb, message, IPMI::Completion::Request_Data_Length_Invalid);
	uint16_t reservation = (message.data[1] << 8) | message.data[0];
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
	int i = message.data[4]; // offset
	int limit = message.data[4] + message.data[5] /* bytes to read */;
	if (limit > sdrdata.size())
		limit = sdrdata.size();
	for (; i < limit; ++i)
		reply.push_back(sdrdata[i]);
	if (reply.size() > IPMI_MSG::max_data_len)
		RETURN_ERROR(ipmb, message, IPMI::Completion::Cannot_Return_Requested_Number_Of_Data_Bytes);
	ipmb.send(message.prepare_reply(reply));
}
IPMICMD_INDEX_REGISTER(Get_SDR);

#if 0 // Unimplemented.
static void ipmicmd_Add_SDR(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Add_SDR);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Partial_Add_SDR(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Partial_Add_SDR);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Delete_SDR(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Delete_SDR);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Clear_SDR_Repository(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Clear_SDR_Repository);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_SDR_Repository_Time(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Get_SDR_Repository_Time);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Set_SDR_Repository_Time(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Set_SDR_Repository_Time);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Enter_SDR_Repository_Update_Mode(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Enter_SDR_Repository_Update_Mode);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Exit_SDR_Repository_Update_Mode(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Exit_SDR_Repository_Update_Mode);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Run_Initialization_Agent(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Run_Initialization_Agent);
#endif


// SEL Device Commands

#if 0 // Unimplemented.
static void ipmicmd_Get_SEL_Info(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Get_SEL_Info);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_SEL_Allocation_Info(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Get_SEL_Allocation_Info);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Reserve_SEL(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Reserve_SEL);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_SEL_Entry(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Get_SEL_Entry);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Add_SEL_Entry(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Add_SEL_Entry);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Partial_Add_SEL_Entry(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Partial_Add_SEL_Entry);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Delete_SEL_Entry(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Delete_SEL_Entry);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Clear_SEL(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Clear_SEL);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_SEL_Time(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Get_SEL_Time);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Set_SEL_Time(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Set_SEL_Time);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Auxiliary_Log_Status(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Get_Auxiliary_Log_Status);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Set_Auxiliary_Log_Status(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Set_Auxiliary_Log_Status);
#endif

