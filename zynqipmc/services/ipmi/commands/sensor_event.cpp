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
#include <payload_manager.h>
#include <services/ipmi/IPMI.h>
#include <services/ipmi/commands/ipmicmd_index.h>
#include <services/ipmi/ipmbsvc/ipmbsvc.h>
#include <services/ipmi/sdr/sensor_data_record_01.h>
#include <services/ipmi/sdr/sensor_data_record_readable_sensor.h>
#include <services/ipmi/sdr/sensor_data_record_sensor.h>
#include <services/ipmi/sdr/sensor_data_repository.h>
#include <services/ipmi/sensor/sensor.h>
#include <services/ipmi/sensor/sensor_set.h>
#include <services/ipmi/sensor/threshold_sensor.h>
#include <services/persistentstorage/persistent_storage.h>

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

static void ipmicmd_Get_Event_Receiver(IPMBSvc &ipmb, const IPMI_MSG &message) {
	ipmb.send(message.prepare_reply({IPMI::Completion::Success, ipmi_event_receiver.addr, ipmi_event_receiver.lun}));
}
IPMICMD_INDEX_REGISTER(Get_Event_Receiver);

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

	time_t update_ts = device_sdr_repo.lastUpdateTimestamp();
	for (int i = 0; i < 4; ++i)
		reply->data[3+i] = (update_ts >> (8*i)) & 0xff;
	ipmb.send(reply);
}
IPMICMD_INDEX_REGISTER(Get_Device_SDR_Info);

static void ipmicmd_Get_Device_SDR(IPMBSvc &ipmb, const IPMI_MSG &message) {
	if (message.data_len != 6)
		RETURN_ERROR(ipmb, message, IPMI::Completion::Request_Data_Length_Invalid);
	//uint16_t reservation = (message.data[1] << 8) | message.data[0]; // not used
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
	size_t i = message.data[4]; // offset
	size_t limit = message.data[4] + message.data[5] /* bytes to read */;
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

static void ipmicmd_Set_Sensor_Hysteresis(IPMBSvc &ipmb, const IPMI_MSG &message) {
	if (message.data_len != 4)
		RETURN_ERROR(ipmb, message, IPMI::Completion::Request_Data_Length_Invalid);
	std::shared_ptr<Sensor> sensor = ipmc_sensors.get(message.data[0]);
	if (!sensor)
		RETURN_ERROR(ipmb, message, IPMI::Completion::Requested_Sensor_Data_Or_Record_Not_Present);
	std::shared_ptr<const SensorDataRecordReadableSensor> sdr = std::dynamic_pointer_cast<const SensorDataRecordReadableSensor>(device_sdr_repo.find(sensor->getSdrKey()));
	if (!sdr)
		RETURN_ERROR(ipmb, message, IPMI::Completion::Requested_Sensor_Data_Or_Record_Not_Present);
	sensor->setAllEventsDisabled(!(message.data[1] & 0x80));
	sensor->setSensorScanningDisabled(!(message.data[1] & 0x40));
	if (message.data_len < 3) {
		/* If they only sent a byte for global enable/disable, we're not going
		 * to update our actual specific enables all to zero.  Otherwise it's
		 * specified that any missing bytes are to be read as 0.
		 */
		ipmb.send(message.prepare_reply({IPMI::Completion::Success}));
		return;
	}
	std::shared_ptr<SensorDataRecordReadableSensor> mutable_sdr = std::dynamic_pointer_cast<SensorDataRecordReadableSensor>(sdr->interpret());
	if (!mutable_sdr) // This is effectively an assertion fail, we shouldn't be able to interpret a ReadableSensor into a non-ReadableSensor
		RETURN_ERROR(ipmb, message, IPMI::Completion::Invalid_Data_Field_In_Request);

	mutable_sdr->hysteresis_high(message.data[2]);
	mutable_sdr->hysteresis_low(message.data[3]);
	device_sdr_repo.add(*mutable_sdr, 0);
	// Write changes to EEPROM
	VariablePersistentAllocation sdr_persist(*persistent_storage, PersistentStorageAllocations::WISC_SDR_REPOSITORY);
	sdr_persist.setData(device_sdr_repo.u8export());
	// Update Sensor Processor config
	payload_manager->refreshSensorLinkage();
	// Return success
	ipmb.send(message.prepare_reply({IPMI::Completion::Success}));
}
IPMICMD_INDEX_REGISTER(Set_Sensor_Hysteresis);

static void ipmicmd_Get_Sensor_Hysteresis(IPMBSvc &ipmb, const IPMI_MSG &message) {
	if (message.data_len < 1 || message.data_len > 2)
		RETURN_ERROR(ipmb, message, IPMI::Completion::Request_Data_Length_Invalid);
	std::shared_ptr<ThresholdSensor> sensor = std::dynamic_pointer_cast<ThresholdSensor>(ipmc_sensors.get(message.data[0]));
	if (!sensor)
		RETURN_ERROR(ipmb, message, IPMI::Completion::Requested_Sensor_Data_Or_Record_Not_Present);
	std::shared_ptr<const SensorDataRecord01> sdr = std::dynamic_pointer_cast<const SensorDataRecord01>(device_sdr_repo.find(sensor->getSdrKey()));
	if (!sdr)
		RETURN_ERROR(ipmb, message, IPMI::Completion::Requested_Sensor_Data_Or_Record_Not_Present);

	ipmb.send(message.prepare_reply({IPMI::Completion::Success, sdr->hysteresis_high(), sdr->hysteresis_low()}));
}
IPMICMD_INDEX_REGISTER(Get_Sensor_Hysteresis);

static void ipmicmd_Set_Sensor_Threshold(IPMBSvc &ipmb, const IPMI_MSG &message) {
	if (message.data_len < 2 || message.data_len > 8)
		RETURN_ERROR(ipmb, message, IPMI::Completion::Request_Data_Length_Invalid);
	std::shared_ptr<ThresholdSensor> sensor = std::dynamic_pointer_cast<ThresholdSensor>(ipmc_sensors.get(message.data[0]));
	if (!sensor)
		RETURN_ERROR(ipmb, message, IPMI::Completion::Requested_Sensor_Data_Or_Record_Not_Present);
	std::shared_ptr<const SensorDataRecord01> sdr = std::dynamic_pointer_cast<const SensorDataRecord01>(device_sdr_repo.find(sensor->getSdrKey()));
	std::shared_ptr<SensorDataRecord01> mutable_sdr = NULL;
	if (sdr)
		mutable_sdr = std::dynamic_pointer_cast<SensorDataRecord01>(sdr->interpret());
	uint8_t changed_thresholds = message.data[1];
	if (message.data_len > 2 && changed_thresholds & 1<<0)  {
		if (mutable_sdr)
			mutable_sdr->threshold_lnc_rawvalue(message.data[2]);
		sensor->thresholds.lnc = message.data[2];
	}
	if (message.data_len > 3 && changed_thresholds & 1<<1)  {
		if (mutable_sdr)
			mutable_sdr->threshold_lcr_rawvalue(message.data[3]);
		sensor->thresholds.lcr = message.data[3];
	}
	if (message.data_len > 4 && changed_thresholds & 1<<2)  {
		if (mutable_sdr)
			mutable_sdr->threshold_lnr_rawvalue(message.data[4]);
		sensor->thresholds.lnr = message.data[4];
	}
	if (message.data_len > 5 && changed_thresholds & 1<<3)  {
		if (mutable_sdr)
			mutable_sdr->threshold_unc_rawvalue(message.data[5]);
		sensor->thresholds.unc = message.data[5];
	}
	if (message.data_len > 6 && changed_thresholds & 1<<4)  {
		if (mutable_sdr)
			mutable_sdr->threshold_ucr_rawvalue(message.data[6]);
		sensor->thresholds.ucr = message.data[6];
	}
	if (message.data_len > 7 && changed_thresholds & 1<<5)  {
		if (mutable_sdr)
			mutable_sdr->threshold_unr_rawvalue(message.data[7]);
		sensor->thresholds.unr = message.data[7];
	}
	if (mutable_sdr) {
		device_sdr_repo.add(*mutable_sdr, 0);
		// Write changes to EEPROM
		VariablePersistentAllocation sdr_persist(*persistent_storage, PersistentStorageAllocations::WISC_SDR_REPOSITORY);
		sdr_persist.setData(device_sdr_repo.u8export());
	}
	// Update Sensor Processor config
	payload_manager->refreshSensorLinkage();
	// Return success
	ipmb.send(message.prepare_reply({IPMI::Completion::Success}));
}
IPMICMD_INDEX_REGISTER(Set_Sensor_Threshold);

static void ipmicmd_Get_Sensor_Threshold(IPMBSvc &ipmb, const IPMI_MSG &message) {
	if (message.data_len != 1)
		RETURN_ERROR(ipmb, message, IPMI::Completion::Request_Data_Length_Invalid);
	std::shared_ptr<ThresholdSensor> sensor = std::dynamic_pointer_cast<ThresholdSensor>(ipmc_sensors.get(message.data[0]));
	if (!sensor)
		RETURN_ERROR(ipmb, message, IPMI::Completion::Requested_Sensor_Data_Or_Record_Not_Present);
	sensor->updateThresholdsFromSdr(std::dynamic_pointer_cast<const SensorDataRecord01>(device_sdr_repo.find(sensor->getSdrKey())));

	std::vector<uint8_t> rsp{IPMI::Completion::Success, 0x3F};
	rsp.push_back(sensor->thresholds.lnc);
	rsp.push_back(sensor->thresholds.lcr);
	rsp.push_back(sensor->thresholds.lnr);
	rsp.push_back(sensor->thresholds.unc);
	rsp.push_back(sensor->thresholds.ucr);
	rsp.push_back(sensor->thresholds.unr);
	ipmb.send(message.prepare_reply(rsp));
}
IPMICMD_INDEX_REGISTER(Get_Sensor_Threshold);

static void ipmicmd_Set_Sensor_Event_Enable(IPMBSvc &ipmb, const IPMI_MSG &message) {
	if (message.data_len < 1 || message.data_len > 6)
		RETURN_ERROR(ipmb, message, IPMI::Completion::Request_Data_Length_Invalid);
	std::shared_ptr<Sensor> sensor = ipmc_sensors.get(message.data[0]);
	if (!sensor)
		RETURN_ERROR(ipmb, message, IPMI::Completion::Requested_Sensor_Data_Or_Record_Not_Present);
	sensor->setAllEventsDisabled(!(message.data[1] & 0x80));
	sensor->setSensorScanningDisabled(!(message.data[1] & 0x40));
	if (message.data_len < 3) {
		/* If they only sent a byte for global enable/disable, we're not going
		 * to update our actual specific enables all to zero.  Otherwise it's
		 * specified that any missing bytes are to be read as 0.
		 */
		ipmb.send(message.prepare_reply({IPMI::Completion::Success}));
		return;
	}
	// Any missing bytes are to be read as zero.  We'll just fill that in for simplicity.
	std::vector<uint8_t> msgdata(message.data, message.data + message.data_len);
	while (msgdata.size() < 6)
		msgdata.push_back(0);

	uint16_t assertions = (message.data[3]<<8) | message.data[2];
	uint16_t deassertions = (message.data[5]<<8) | message.data[4];

	if ((message.data[1] & 0x30) == 0x10) {
		// Enable Selected
		assertions   |= sensor->getAssertionEventsEnabled();
		deassertions |= sensor->getDeassertionEventsEnabled();
	}
	else if ((message.data[1] & 0x30) == 0x20) {
		// Disable Selected
		assertions   = sensor->getAssertionEventsEnabled() & ~assertions;
		deassertions = sensor->getDeassertionEventsEnabled() & ~deassertions;
	}
	else {
		// Do not change individual enables 0x00, OR, Reserved 0x30.
		ipmb.send(message.prepare_reply({IPMI::Completion::Success}));
	}
	sensor->setAssertionEventsEnabled(assertions);
	sensor->setDeassertionEventsEnabled(deassertions);
	// Update Sensor Processor config
	payload_manager->refreshSensorLinkage();
	// Return success
	ipmb.send(message.prepare_reply({IPMI::Completion::Success}));
}
IPMICMD_INDEX_REGISTER(Set_Sensor_Event_Enable);

static void ipmicmd_Get_Sensor_Event_Enable(IPMBSvc &ipmb, const IPMI_MSG &message) {
	if (message.data_len != 1)
		RETURN_ERROR(ipmb, message, IPMI::Completion::Request_Data_Length_Invalid);
	std::shared_ptr<Sensor> sensor = ipmc_sensors.get(message.data[0]);
	if (!sensor)
		RETURN_ERROR(ipmb, message, IPMI::Completion::Requested_Sensor_Data_Or_Record_Not_Present);
	std::vector<uint8_t> rsp{IPMI::Completion::Success, 0};
	if (!sensor->getAllEventsDisabled())
		rsp[1] |= 0x80;
	if (!sensor->getSensorScanningDisabled())
		rsp[1] |= 0x40;
	uint16_t assert_events = sensor->getAssertionEventsEnabled();
	uint16_t deassert_events = sensor->getDeassertionEventsEnabled();
	rsp.push_back(assert_events & 0xff);
	rsp.push_back(assert_events >> 8);
	rsp.push_back(deassert_events & 0xff);
	rsp.push_back(deassert_events >> 8);
	ipmb.send(message.prepare_reply(rsp));
}
IPMICMD_INDEX_REGISTER(Get_Sensor_Event_Enable);

static void ipmicmd_Rearm_Sensor_Events(IPMBSvc &ipmb, const IPMI_MSG &message) {
	ipmb.send(message.prepare_reply({IPMI::Completion::Invalid_Command}));
}
IPMICMD_INDEX_REGISTER(Rearm_Sensor_Events);

static void ipmicmd_Get_Sensor_Event_Status(IPMBSvc &ipmb, const IPMI_MSG &message) {
	if (message.data_len != 1)
		RETURN_ERROR(ipmb, message, IPMI::Completion::Request_Data_Length_Invalid);
	std::shared_ptr<Sensor> sensor = ipmc_sensors.get(message.data[0]);
	if (!sensor)
		RETURN_ERROR(ipmb, message, IPMI::Completion::Requested_Sensor_Data_Or_Record_Not_Present);
	bool reading_good = true;
	uint16_t event_status = sensor->getSensorEventStatus(&reading_good);
	std::vector<uint8_t> payload{IPMI::Completion::Success, 0x00};
	if (!sensor->getAllEventsDisabled())
		payload[1] |= 0x80;
	if (!sensor->getSensorScanningDisabled())
		payload[1] |= 0x40;
	if (!reading_good)
		payload[1] |= 0x20;
	payload.push_back(event_status & 0xff);
	payload.push_back(event_status >> 8);
	ipmb.send(message.prepare_reply(payload));
}
IPMICMD_INDEX_REGISTER(Get_Sensor_Event_Status);

static void ipmicmd_Get_Sensor_Reading(IPMBSvc &ipmb, const IPMI_MSG &message) {
	if (message.data_len != 1)
		RETURN_ERROR(ipmb, message, IPMI::Completion::Request_Data_Length_Invalid);
	std::shared_ptr<Sensor> sensor = ipmc_sensors.get(message.data[0]);
	if (!sensor)
		RETURN_ERROR(ipmb, message, IPMI::Completion::Requested_Sensor_Data_Or_Record_Not_Present);
	ipmb.send(message.prepare_reply(sensor->getSensorReading()));
}
IPMICMD_INDEX_REGISTER(Get_Sensor_Reading);

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

