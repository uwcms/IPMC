#include <services/ipmi/IPMI.h>
#include <services/ipmi/ipmbsvc/IPMBSvc.h>
#include <services/ipmi/sensor/SensorSet.h>
#include <IPMC.h>
#include "IPMICmd_Index.h"

// Event Commands

static void ipmicmd_Set_Event_Receiver(IPMBSvc &ipmb, const IPMI_MSG &message) {
	ipmi_event_receiver.ipmb = &ipmb;
	ipmi_event_receiver.addr = message.data[0];
	ipmi_event_receiver.lun  = message.data[1] & 0x03;

	ipmb.send(message.prepare_reply(std::vector<uint8_t>{IPMI::Completion::Success}));

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

#if 0 // Unimplemented.
static void ipmicmd_Get_Device_SDR_Info(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Get_Device_SDR_Info);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Device_SDR(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Get_Device_SDR);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Reserve_Device_SDR_Repository(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Reserve_Device_SDR_Repository);
#endif

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

