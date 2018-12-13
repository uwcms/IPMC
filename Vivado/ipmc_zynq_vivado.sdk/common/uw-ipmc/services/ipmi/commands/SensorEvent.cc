#include <services/ipmi/IPMI.h>
#include <services/ipmi/ipmbsvc/IPMBSvc.h>
#include <services/ipmi/sensor/SensorSet.h>
#include <services/ipmi/sdr/SensorDataRepository.h>
#include <services/ipmi/sdr/SensorDataRecordSensor.h>
#include <IPMC.h>
#include "IPMICmd_Index.h"

#define RETURN_ERROR(ipmb, message, completion_code) \
	do { \
		ipmb.send(message.prepare_reply({completion_code})); \
		return; \
	} while (0)

// Event Commands

static void ipmicmd_Set_Event_Receiver(IPMBSvc &ipmb, const IPMI_MSG &message) {
	ipmi_event_receiver.ipmb = &ipmb;
	ipmi_event_receiver.addr = message.data[0];
	ipmi_event_receiver.lun  = message.data[1] & 0x03;

	ipmb.send(message.prepare_reply({IPMI::Completion::Success}));

	SensorSet::container_type all_sensors(ipmc_sensors);
	for (auto it = all_sensors.begin(), eit = all_sensors.end(); it != eit; ++it)
		it->second->rearm();
}
IPMICMD_INDEX_REGISTER(Set_Event_Receiver);

#if 0 // Unimplemented.
static void ipmicmd_Get_Event_Receiver(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Get_Event_Receiver);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Platform_Event(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Platform_Event);
#endif


// PEF and Alerting Commands

#if 0 // Unimplemented.
static void ipmicmd_Get_PEF_Capabilities(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Get_PEF_Capabilities);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Arm_PEF_Postpone_Timer(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Arm_PEF_Postpone_Timer);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Set_PEF_Configuration_Parameters(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Set_PEF_Configuration_Parameters);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_PEF_Configuration_Parameters(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Get_PEF_Configuration_Parameters);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Set_Last_Processed_Event_ID(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Set_Last_Processed_Event_ID);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Last_Processed_Event_ID(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Get_Last_Processed_Event_ID);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Alert_Immediate(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Alert_Immediate);
#endif

#if 0 // Unimplemented.
static void ipmicmd_PET_Acknowledge(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(PET_Acknowledge);
#endif


// Sensor Device Commands

static void ipmicmd_Get_Device_SDR_Info(IPMBSvc &ipmb, const IPMI_MSG &message) {
	uint8_t parameter = 0;
	if (message.data_len == 1)
		parameter = message.data[1];
	else if (message.data_len > 1)
		RETURN_ERROR(ipmb, message, IPMI::Completion::Request_Data_Length_Invalid);

	// Count the sensors on each LUN.
	uint16_t lun_sensor_count[4] = {0, 0, 0, 0};
	std::vector< std::shared_ptr<const SensorDataRecord> > dsdrr(device_sdr_repo);
	for (auto it = dsdrr.begin(), eit = dsdrr.end(); it != eit; ++it) {
		std::shared_ptr<const SensorDataRecordSensor> sdrs = std::dynamic_pointer_cast<const SensorDataRecordSensor>(*it);
		if (!sdrs)
			continue;
		if (sdrs->sensor_owner_lun() < 4)
			++lun_sensor_count[sdrs->sensor_owner_lun()];
	}

	std::shared_ptr<IPMI_MSG> reply = message.prepare_reply({IPMI::Completion::Success, 0, 0, 0, 0, 0, 0});
	if (parameter & 1) {
		// Return the number of records in the Device SDR Repository
		reply->data[1] = device_sdr_repo.size();
	}
	else {
		// Return the number of sensors (specifically) on the current LUN (see request message)
		reply->data[1] = lun_sensor_count[message.rsLUN] > 255 ? 255 : lun_sensor_count[message.rsLUN];
	}

	/* Dynamic Sensor Population.  I mean, I don't want to guarantee our
	 * Device SDR Repo *won't* change, even though it /probably/ won't.
	 */
	reply->data[2] |= 0x80;

	for (int i = 0; i < 4; ++i)
		if (lun_sensor_count[i])
			reply->data[2] |= (1 << i); // Sensors exist on this LUN.

	time_t update_ts = device_sdr_repo.last_update_timestamp();
	for (int i = 0; i < 4; ++i)
		reply->data[3+i] = (update_ts >> (8*i)) & 0xff;
	ipmb.send(reply);
}
IPMICMD_INDEX_REGISTER(Get_Device_SDR_Info);

static void ipmicmd_Get_Device_SDR(IPMBSvc &ipmb, const IPMI_MSG &message) {
	if (message.data_len != 6)
		RETURN_ERROR(ipmb, message, IPMI::Completion::Request_Data_Length_Invalid);
	uint16_t reservation = (message.data[1] << 8) | message.data[0];
	uint16_t record_id = (message.data[3] << 8) | message.data[2];
	if (record_id == 0xFFFF)
		record_id = device_sdr_repo.size() - 1;
	std::shared_ptr<const SensorDataRecord> record;
	try {
		record = device_sdr_repo.get(record_id);
	}
	catch (SensorDataRepository::reservation_cancelled_error) {
		RETURN_ERROR(ipmb, message, IPMI::Completion::Reservation_Cancelled);
	}
	if (!record)
		RETURN_ERROR(ipmb, message, IPMI::Completion::Requested_Sensor_Data_Or_Record_Not_Present);
	uint16_t next_record = record_id + 1;
	if (next_record >= device_sdr_repo.size())
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
IPMICMD_INDEX_REGISTER(Get_Device_SDR);

static void ipmicmd_Reserve_Device_SDR_Repository(IPMBSvc &ipmb, const IPMI_MSG &message) {
	uint16_t reservation = device_sdr_repo.reserve();
	ipmb.send(message.prepare_reply({IPMI::Completion::Success, static_cast<uint8_t>(reservation & 0xFF), static_cast<uint8_t>(reservation >> 8)}));
}
IPMICMD_INDEX_REGISTER(Reserve_Device_SDR_Repository);

#if 0 // Unimplemented.
static void ipmicmd_Get_Sensor_Reading_Factors(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Get_Sensor_Reading_Factors);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Set_Sensor_Hysteresis(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Set_Sensor_Hysteresis);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Sensor_Hysteresis(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Get_Sensor_Hysteresis);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Set_Sensor_Threshold(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Set_Sensor_Threshold);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Sensor_Threshold(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Get_Sensor_Threshold);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Set_Sensor_Event_Enable(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Set_Sensor_Event_Enable);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Sensor_Event_Enable(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Get_Sensor_Event_Enable);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Rearm_Sensor_Events(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Rearm_Sensor_Events);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Sensor_Event_Status(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Get_Sensor_Event_Status);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Sensor_Reading(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Get_Sensor_Reading);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Set_Sensor_Type(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Set_Sensor_Type);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Sensor_Type(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Get_Sensor_Type);
#endif

