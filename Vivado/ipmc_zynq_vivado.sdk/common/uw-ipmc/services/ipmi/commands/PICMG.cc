#include <services/ipmi/IPMI.h>
#include <services/ipmi/ipmbsvc/IPMBSvc.h>
#include <services/ipmi/sdr/SensorDataRepository.h>
#include <services/ipmi/MStateMachine.h>
#include <PayloadManager.h>
#include "IPMICmd_Index.h"

// AdvancedTCA

#define RETURN_ERROR(ipmb, message, completion_code) \
	do { \
		ipmb.send(message.prepare_reply({completion_code})); \
		return; \
	} while (0)

#define ASSERT_PICMG_IDENTIFIER(ipmb, message) \
	if (message.data_len < 1 || message.data[0] != 0) \
		RETURN_ERROR(ipmb, message, IPMI::Completion::Invalid_Data_Field_In_Request)

static void ipmicmd_Get_PICMG_Properties(IPMBSvc &ipmb, const IPMI_MSG &message) {
	ASSERT_PICMG_IDENTIFIER(ipmb, message);
	ipmb.send(message.prepare_reply({
		IPMI::Completion::Success,
		0, // PICMG Identifier (spec requires 0)
		0x32, // PICMG Extension Version
		0, // Max FRU Device ID // TODO
		0, // FRU Device ID for IPM Controller (spec requires 0)
	}));
}
IPMICMD_INDEX_REGISTER(Get_PICMG_Properties);

#if 0 // Unimplemented.
static void ipmicmd_Get_Address_Info(IPMBSvc &ipmb, const IPMI_MSG &message) {
	ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(Get_Address_Info);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Shelf_Address_Info(IPMBSvc &ipmb, const IPMI_MSG &message) {
	ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(Get_Shelf_Address_Info);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Set_Shelf_Address_Info(IPMBSvc &ipmb, const IPMI_MSG &message) {
	ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(Set_Shelf_Address_Info);
#endif

#if 0 // Unimplemented.
static void ipmicmd_FRU_Control(IPMBSvc &ipmb, const IPMI_MSG &message) {
	ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(FRU_Control);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_FRU_LED_Properties(IPMBSvc &ipmb, const IPMI_MSG &message) {
	ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(Get_FRU_LED_Properties);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_LED_Color_Capabilities(IPMBSvc &ipmb, const IPMI_MSG &message) {
	ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(Get_LED_Color_Capabilities);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Set_FRU_LED_State(IPMBSvc &ipmb, const IPMI_MSG &message) {
	ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(Set_FRU_LED_State);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_FRU_LED_State(IPMBSvc &ipmb, const IPMI_MSG &message) {
	ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(Get_FRU_LED_State);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Set_IPMB_State(IPMBSvc &ipmb, const IPMI_MSG &message) {
	ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(Set_IPMB_State);
#endif

static void ipmicmd_Set_FRU_Activation_Policy(IPMBSvc &ipmb, const IPMI_MSG &message) {
	ASSERT_PICMG_IDENTIFIER(ipmb, message);
	if (message.data_len != 4 || message.data[1] != 0)
		// Not enough parameters, or asking for FRU != 0.  We dont have one of those.
		RETURN_ERROR(ipmb, message, IPMI::Completion::Parameter_Out_Of_Range);
	if (!mstatemachine)
		// Not yet initialized (we got an IPMI message before ipmc_service_init() completed)
		RETURN_ERROR(ipmb, message, IPMI::Completion::Node_Busy);

	bool act_locked = mstatemachine->activation_locked();
	bool deact_locked = mstatemachine->deactivation_locked();
	if (message.data[2] & 0x01)
		act_locked = message.data[3] & 0x01;
	if (message.data[2] & 0x02)
		deact_locked = message.data[3] & 0x02;
	mstatemachine->set_activation_lock(act_locked, deact_locked);
	ipmb.send(message.prepare_reply({IPMI::Completion::Success, 0}));
}
IPMICMD_INDEX_REGISTER(Set_FRU_Activation_Policy);

static void ipmicmd_Get_FRU_Activation_Policy(IPMBSvc &ipmb, const IPMI_MSG &message) {
	ASSERT_PICMG_IDENTIFIER(ipmb, message);
	if (message.data_len != 2 || message.data[1] != 0)
		// Not enough parameters, or asking for FRU != 0.  We dont have one of those.
		RETURN_ERROR(ipmb, message, IPMI::Completion::Parameter_Out_Of_Range);
	if (!mstatemachine)
		// Not yet initialized (we got an IPMI message before ipmc_service_init() completed)
		RETURN_ERROR(ipmb, message, IPMI::Completion::Node_Busy);
	std::shared_ptr<IPMI_MSG> reply = message.prepare_reply({IPMI::Completion::Success, 0, 0});
	if (mstatemachine->deactivation_locked())
		reply->data[2] |= 0x02;
	if (mstatemachine->activation_locked())
		reply->data[2] |= 0x01;
	ipmb.send(reply);
}
IPMICMD_INDEX_REGISTER(Get_FRU_Activation_Policy);

static void ipmicmd_Set_FRU_Activation(IPMBSvc &ipmb, const IPMI_MSG &message) {
	ASSERT_PICMG_IDENTIFIER(ipmb, message);
	if (message.data_len != 3 || message.data[1] != 0)
		// Not enough parameters, or asking for FRU != 0.  We dont have one of those.
		RETURN_ERROR(ipmb, message, IPMI::Completion::Parameter_Out_Of_Range);
	if (!mstatemachine)
		// Not yet initialized (we got an IPMI message before ipmc_service_init() completed)
		RETURN_ERROR(ipmb, message, IPMI::Completion::Node_Busy);
	if (message.data[2] == 0) {
		// Deactivate FRU
		mstatemachine->deactivate_fru();
	}
	else if (message.data[2] == 1) {
		// Activate FRU
		mstatemachine->activate_fru();
	}
	else {
		// What?
		RETURN_ERROR(ipmb, message, IPMI::Completion::Parameter_Out_Of_Range);
	}
	ipmb.send(message.prepare_reply({IPMI::Completion::Success, 0}));
}
IPMICMD_INDEX_REGISTER(Set_FRU_Activation);

static void ipmicmd_Get_Device_Locator_Record_ID(IPMBSvc &ipmb, const IPMI_MSG &message) {
	ASSERT_PICMG_IDENTIFIER(ipmb, message);
	const std::vector< std::shared_ptr<const SensorDataRecord> > device_sdr_repo_vector(device_sdr_repo);
	for (auto it = device_sdr_repo_vector.begin(), eit = device_sdr_repo_vector.end(); it != eit; ++it) {
		if ((*it)->record_type() != 0x12)
			continue;
		uint16_t key = (*it)->record_id();
		ipmb.send(message.prepare_reply({IPMI::Completion::Success, 0, static_cast<uint8_t>(key & 0xff), static_cast<uint8_t>(key >> 8)}));
		return;
	}
	ipmb.send(message.prepare_reply({IPMI::Completion::Requested_Sensor_Data_Or_Record_Not_Present}));
}
IPMICMD_INDEX_REGISTER(Get_Device_Locator_Record_ID);

#if 0 // Unimplemented.
static void ipmicmd_Set_Port_State(IPMBSvc &ipmb, const IPMI_MSG &message) {
	ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(Set_Port_State);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Port_State(IPMBSvc &ipmb, const IPMI_MSG &message) {
	ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(Get_Port_State);
#endif

static void ipmicmd_Compute_Power_Properties(IPMBSvc &ipmb, const IPMI_MSG &message) {
	ASSERT_PICMG_IDENTIFIER(ipmb, message);
	if (message.data_len != 2 || message.data[1] != 0)
		// Not enough parameters, or asking for FRU != 0.  We dont have one of those.
		RETURN_ERROR(ipmb, message, IPMI::Completion::Parameter_Out_Of_Range);
	if (!payload_manager)
		// Not yet initialized (we got an IPMI message before ipmc_service_init() completed)
		RETURN_ERROR(ipmb, message, IPMI::Completion::Node_Busy);
	PayloadManager::PowerProperties properties = payload_manager->get_power_properties(message.data[1], true);
	ipmb.send(message.prepare_reply({IPMI::Completion::Success, 0, properties.spanned_slots, properties.controller_location}));
}
IPMICMD_INDEX_REGISTER(Compute_Power_Properties);

static void ipmicmd_Set_Power_Level(IPMBSvc &ipmb, const IPMI_MSG &message) {
	ASSERT_PICMG_IDENTIFIER(ipmb, message);
	if (message.data_len != 4 || message.data[1] != 0)
		// Not enough parameters, or asking for FRU != 0.  We don't have one of those.
		RETURN_ERROR(ipmb, message, IPMI::Completion::Parameter_Out_Of_Range);
	if (!payload_manager)
		// Not yet initialized (we got an IPMI message before ipmc_service_init() completed)
		RETURN_ERROR(ipmb, message, IPMI::Completion::Node_Busy);

	PayloadManager::PowerProperties properties = payload_manager->get_power_properties(message.data[1], false);
	uint8_t new_power_level = properties.current_power_level;
	if (message.data[2] != 0xFF)
		new_power_level = message.data[2];
	else if (message.data[3] == 1)
		new_power_level = properties.desired_power_level;

	if (properties.current_power_level != new_power_level) {
		try {
			payload_manager->set_power_level(message.data[1], new_power_level);
		}
		catch (std::domain_error &e) {
			RETURN_ERROR(ipmb, message, IPMI::Completion::Parameter_Out_Of_Range);
		}
	}

	ipmb.send(message.prepare_reply({IPMI::Completion::Success, 0}));
}
IPMICMD_INDEX_REGISTER(Set_Power_Level);

static void ipmicmd_Get_Power_Level(IPMBSvc &ipmb, const IPMI_MSG &message) {
	ASSERT_PICMG_IDENTIFIER(ipmb, message);
	if (message.data_len != 3 || message.data[1] != 0)
		// Not enough parameters, or asking for FRU != 0.  We dont have one of those.
		RETURN_ERROR(ipmb, message, IPMI::Completion::Parameter_Out_Of_Range);
	if (!payload_manager)
		// Not yet initialized (we got an IPMI message before ipmc_service_init() completed)
		RETURN_ERROR(ipmb, message, IPMI::Completion::Node_Busy);

	PayloadManager::PowerProperties properties = payload_manager->get_power_properties(message.data[1], false);

	std::shared_ptr<IPMI_MSG> reply = message.prepare_reply({IPMI::Completion::Success, 0, 0, 0, 0});
	if (properties.dynamic_reconfiguration)
		reply->data[2] |= 0x80;
	reply->data[3] = properties.delay_to_stable_power;
	reply->data[4] = properties.power_multiplier;

	if (message.data[2] > 3)
		RETURN_ERROR(ipmb, message, IPMI::Completion::Parameter_Out_Of_Range);

	if (message.data[2] == 0) // Steady state levels
		reply->data[2] |= properties.current_power_level;
	else if (message.data[2] & 1) // Desired levels
		reply->data[2] |= properties.desired_power_level;

	if ((message.data[2] & 2) == 0) {
		// Any steady
		for (std::vector<uint8_t>::size_type i = 0; i < properties.power_levels.size(); ++i)
			reply->data[5+i] = properties.power_levels[i];
		reply->data_len = 5+properties.power_levels.size();
	}

	if ((message.data[2] & 2) == 2) {
		// Any early
		for (std::vector<uint8_t>::size_type i = 0; i < properties.early_power_levels.size(); ++i)
			reply->data[5+i] = properties.early_power_levels[i];
		reply->data_len = 5+properties.early_power_levels.size();
	}
	ipmb.send(reply);
}
IPMICMD_INDEX_REGISTER(Get_Power_Level);

#if 0 // Unimplemented. TODO
static void ipmicmd_Renegotiate_Power(IPMBSvc &ipmb, const IPMI_MSG &message) {
	ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(Renegotiate_Power);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Fan_Speed_Properties(IPMBSvc &ipmb, const IPMI_MSG &message) {
	ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(Get_Fan_Speed_Properties);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Set_Fan_Level(IPMBSvc &ipmb, const IPMI_MSG &message) {
	ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(Set_Fan_Level);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Fan_Level(IPMBSvc &ipmb, const IPMI_MSG &message) {
	ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(Get_Fan_Level);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Bused_Resource(IPMBSvc &ipmb, const IPMI_MSG &message) {
	ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(Bused_Resource);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_IPMB_Link_Info(IPMBSvc &ipmb, const IPMI_MSG &message) {
	ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(Get_IPMB_Link_Info);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Shelf_Manager_IPMB_Address(IPMBSvc &ipmb, const IPMI_MSG &message) {
	ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(Get_Shelf_Manager_IPMB_Address);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Set_Fan_Policy(IPMBSvc &ipmb, const IPMI_MSG &message) {
	ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(Set_Fan_Policy);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Fan_Policy(IPMBSvc &ipmb, const IPMI_MSG &message) {
	ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(Get_Fan_Policy);
#endif

#if 0 // Unimplemented.
static void ipmicmd_FRU_Control_Capabilities(IPMBSvc &ipmb, const IPMI_MSG &message) {
	ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(FRU_Control_Capabilities);
#endif

#if 0 // Unimplemented.
static void ipmicmd_FRU_Inventory_Device_Lock_Control(IPMBSvc &ipmb, const IPMI_MSG &message) {
	ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(FRU_Inventory_Device_Lock_Control);
#endif

#if 0 // Unimplemented.
static void ipmicmd_FRU_Inventory_Device_Write(IPMBSvc &ipmb, const IPMI_MSG &message) {
	ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(FRU_Inventory_Device_Write);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Shelf_Manager_IP_Addresses(IPMBSvc &ipmb, const IPMI_MSG &message) {
	ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(Get_Shelf_Manager_IP_Addresses);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Shelf_Power_Allocation(IPMBSvc &ipmb, const IPMI_MSG &message) {
	ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(Get_Shelf_Power_Allocation);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Telco_Alarm_Capability(IPMBSvc &ipmb, const IPMI_MSG &message) {
	ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(Get_Telco_Alarm_Capability);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Set_Telco_Alarm_State(IPMBSvc &ipmb, const IPMI_MSG &message) {
	ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(Set_Telco_Alarm_State);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Telco_Alarm_State(IPMBSvc &ipmb, const IPMI_MSG &message) {
	ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(Get_Telco_Alarm_State);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Target_Upgrade_Capabilities(IPMBSvc &ipmb, const IPMI_MSG &message) {
    ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(Get_Target_Upgrade_Capabilities);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Component_Properties(IPMBSvc &ipmb, const IPMI_MSG &message) {
    ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(Get_Component_Properties);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Abort_Firmware_Upgrade(IPMBSvc &ipmb, const IPMI_MSG &message) {
    ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(Abort_Firmware_Upgrade);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Initiate_Upgrade_Action(IPMBSvc &ipmb, const IPMI_MSG &message) {
    ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(Initiate_Upgrade_Action);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Upload_Firmware_Block(IPMBSvc &ipmb, const IPMI_MSG &message) {
    ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(Upload_Firmware_Block);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Finish_Firmware_Upload(IPMBSvc &ipmb, const IPMI_MSG &message) {
    ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(Finish_Firmware_Upload);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Upgrade_Status(IPMBSvc &ipmb, const IPMI_MSG &message) {
    ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(Get_Upgrade_Status);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Activate_Firmware(IPMBSvc &ipmb, const IPMI_MSG &message) {
    ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(Activate_Firmware);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Query_Self_Test_Results(IPMBSvc &ipmb, const IPMI_MSG &message) {
    ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(Query_Self_Test_Results);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Query_Rollback_Status(IPMBSvc &ipmb, const IPMI_MSG &message) {
    ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(Query_Rollback_Status);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Initiate_Manual_Rollback(IPMBSvc &ipmb, const IPMI_MSG &message) {
    ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(Initiate_Manual_Rollback);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Telco_Alarm_Location(IPMBSvc &ipmb, const IPMI_MSG &message) {
	ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(Get_Telco_Alarm_Location);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Set_FRU_Extracted(IPMBSvc &ipmb, const IPMI_MSG &message) {
	ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(Set_FRU_Extracted);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_HPM_x_Capabilities(IPMBSvc &ipmb, const IPMI_MSG &message) {
    ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(Get_HPM_x_Capabilities);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Dynamic_Credentials(IPMBSvc &ipmb, const IPMI_MSG &message) {
    ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(Get_Dynamic_Credentials);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Session_Handle_for_Explicit_LAN_Bridging(IPMBSvc &ipmb, const IPMI_MSG &message) {
    ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(Get_Session_Handle_for_Explicit_LAN_Bridging);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_ATCA_Extended_Management_Resources(IPMBSvc &ipmb, const IPMI_MSG &message) {
    ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(Get_ATCA_Extended_Management_Resources);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_AMC_Extended_Management_Resources(IPMBSvc &ipmb, const IPMI_MSG &message) {
    ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(Get_AMC_Extended_Management_Resources);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Set_ATCA_Extended_Management_State(IPMBSvc &ipmb, const IPMI_MSG &message) {
    ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(Set_ATCA_Extended_Management_State);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_ATCA_Extended_Management_State(IPMBSvc &ipmb, const IPMI_MSG &message) {
    ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(Get_ATCA_Extended_Management_State);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Set_AMC_Power_State(IPMBSvc &ipmb, const IPMI_MSG &message) {
    ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(Set_AMC_Power_State);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_AMC_Power_State(IPMBSvc &ipmb, const IPMI_MSG &message) {
    ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(Get_AMC_Power_State);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Assign_SOL_Payload_Instance(IPMBSvc &ipmb, const IPMI_MSG &message) {
    ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(Assign_SOL_Payload_Instance);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_IP_Address_Source(IPMBSvc &ipmb, const IPMI_MSG &message) {
    ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(Get_IP_Address_Source);
#endif
