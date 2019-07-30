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
#include <services/ipmi/MStateMachine.h>
#include <services/ipmi/commands/ipmicmd_index.h>
#include <services/ipmi/ipmbsvc/ipmbsvc.h>
#include <services/ipmi/sdr/sensor_data_repository.h>

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

static void ipmicmd_Get_FRU_LED_Properties(IPMBSvc &ipmb, const IPMI_MSG &message) {
	ASSERT_PICMG_IDENTIFIER(ipmb, message);
	if (message.data_len != 2 || message.data[1] != 0)
		// Not enough parameters, or asking for FRU != 0.  We dont have one of those.
		RETURN_ERROR(ipmb, message, IPMI::Completion::Parameter_Out_Of_Range);
	ipmb.send(message.prepare_reply({
		IPMI::Completion::Success,
		0, // PICMG Identifier (spec requires 0)
		0x0f, // BRGA LEDs are under FRU control.
		0, // No Application Specific LEDs are under FRU control.
	}));
}
IPMICMD_INDEX_REGISTER(Get_FRU_LED_Properties);

static void ipmicmd_Get_LED_Color_Capabilities(IPMBSvc &ipmb, const IPMI_MSG &message) {
	ASSERT_PICMG_IDENTIFIER(ipmb, message);
	if (message.data_len != 3 || message.data[1] != 0)
		// Not enough parameters, or asking for FRU != 0.  We dont have one of those.
		RETURN_ERROR(ipmb, message, IPMI::Completion::Parameter_Out_Of_Range);
	if (message.data[2] > 3)
		// We don't have more than 4 controllable LEDs.
		RETURN_ERROR(ipmb, message, IPMI::Completion::Invalid_Data_Field_In_Request /* Specified */);
	uint8_t led = message.data[2];

	uint8_t color = led + 1; // Nice how these things work out, isn't it?
	ipmb.send(message.prepare_reply({
		IPMI::Completion::Success,
		0, // PICMG Identifier (spec requires 0)
		(uint8_t)(1<<color), // Color flags as specified.
		color, // Local control color state, as specified
		color, // Default override color state, as specified
		0, // No hardware restrictions, all powered from mgmt power.
	}));
}
IPMICMD_INDEX_REGISTER(Get_LED_Color_Capabilities);

static void ipmicmd_Set_FRU_LED_State(IPMBSvc &ipmb, const IPMI_MSG &message) {
	ASSERT_PICMG_IDENTIFIER(ipmb, message);
	if (message.data_len != 6 || message.data[1] != 0)
		// Not enough parameters, or asking for FRU != 0.  We dont have one of those.
		RETURN_ERROR(ipmb, message, IPMI::Completion::Parameter_Out_Of_Range);
	if (message.data[2] > 3)
		// We don't have more than 4 controllable LEDs.
		RETURN_ERROR(ipmb, message, IPMI::Completion::Invalid_Data_Field_In_Request /* Specified */);
	IPMILED &led = *ipmi_leds.at(message.data[2]);
	uint8_t function = message.data[3];
	uint8_t on_duration = message.data[4];
	//uint8_t color = message.data[5]; // We ignore this.
	struct IPMILED::Action action;
	action.min_duration = 0;

	if (function == 0) {
		action.effect = IPMILED::OFF;
	}
	else if (0x01 <= function && function <= 0xFA) {
		action.effect = IPMILED::BLINK;
		action.time_on_ms = on_duration * pdMS_TO_TICKS(10);
		action.period_ms = action.time_on_ms + function * pdMS_TO_TICKS(10);
		led.override(action);
	}
	else if (function == 0xFB) {
		// LAMP TEST
		led.lampTest(on_duration * pdMS_TO_TICKS(100));
	}
	else if (function == 0xFC) {
		action.effect = IPMILED::INACTIVE; // Turn off override mode
		led.override(action);
	}
	else if (function == 0xFF) {
		action.effect = IPMILED::OFF;
		led.override(action);
	}
	else {
		RETURN_ERROR(ipmb, message, IPMI::Completion::Invalid_Data_Field_In_Request /* Specified */);
	}
	ipmb.send(message.prepare_reply({ IPMI::Completion::Success, 0 }));
}
IPMICMD_INDEX_REGISTER(Set_FRU_LED_State);

static void ipmicmd_Get_FRU_LED_State(IPMBSvc &ipmb, const IPMI_MSG &message) {
	ASSERT_PICMG_IDENTIFIER(ipmb, message);
	if (message.data_len != 3 || message.data[1] != 0)
		// Not enough parameters, or asking for FRU != 0.  We dont have one of those.
		RETURN_ERROR(ipmb, message, IPMI::Completion::Parameter_Out_Of_Range);
	if (message.data[2] > 3)
		// We don't have more than 4 controllable LEDs.
		RETURN_ERROR(ipmb, message, IPMI::Completion::Invalid_Data_Field_In_Request /* Specified */);
	IPMILED &led = *ipmi_leds.at(message.data[2]);
	struct IPMILED::Action action = led.getCurrentPhysicalAction();
	uint8_t color = message.data[2] + 1; // Nice how these things work out, isn't it?

	std::shared_ptr<IPMI_MSG> reply = message.prepare_reply();
	reply->data[0] = IPMI::Completion::Success;
	reply->data[1] = 0; // PICMG Identifier (Specified)
	reply->data[2] = 1<<0; // We do have a local control state.
	if (action.control_level == IPMILED::OVERRIDE)
		reply->data[2] |= 1<<1; // The override state is active.
	if (action.control_level == IPMILED::LAMPTEST)
		reply->data[2] = 1<<2; // The Lamp Test state is active.

	action = led.getCurrentLocalAction();
	uint32_t ticksOn = action.time_on_ms;
	uint32_t ticksOff = action.period_ms - action.time_on_ms;

	if (action.effect == IPMILED::OFF) {
		reply->data[3] = 0;
		reply->data[4] = 0;
	}
	else if (action.effect == IPMILED::BLINK) {
		uint32_t unitsOff = ticksOff / pdMS_TO_TICKS(10);
		reply->data[3] = unitsOff;
		if (unitsOff < 1)
			reply->data[3] = 1;
		if (unitsOff > 0xFA)
			reply->data[3] = 0xFA;

		uint8_t unitsOn = ticksOn / pdMS_TO_TICKS(10);
		reply->data[4] = unitsOn;
		if (unitsOn < 1)
			reply->data[4] = 1;
		if (unitsOn > 255)
			reply->data[4] = 255;
	}
	else {
		reply->data[3] = 0xFF; // We'll consider pulsing to just be ON.  IPMI has no provision for it.
		reply->data[4] = 0;
	}
	reply->data[5] = color;

	reply->data_len = 6;
	if (!(reply->data[2] & 0x06)) {
		ipmb.send(reply); // Done here, no higher levels to report.
		return;
	}

	// Fine so we have an override level too.

	action = led.getCurrentOverrideAction();
	ticksOn = action.time_on_ms;
	ticksOff = action.period_ms - action.time_on_ms;

	if (action.effect == IPMILED::OFF) {
		reply->data[7] = 0;
		reply->data[8] = 0;
	}
	else if (action.effect == IPMILED::BLINK) {
		uint32_t unitsOff = ticksOff / pdMS_TO_TICKS(10);
		reply->data[7] = unitsOff;
		if (unitsOff < 1)
			reply->data[7] = 1;
		if (unitsOff > 0xFA)
			reply->data[7] = 0xFA;

		uint8_t unitsOn = ticksOn / pdMS_TO_TICKS(10);
		reply->data[8] = unitsOn;
		if (unitsOn < 1)
			reply->data[8] = 1;
		if (unitsOn > 255)
			reply->data[8] = 255;
	}
	else {
		reply->data[7] = 0xFF; // We'll consider pulsing to just be ON.  IPMI has no provision for it.
		reply->data[8] = 0;
	}
	reply->data[9] = color;

	reply->data_len = 10;
	if (!(reply->data[2] & 0x04)) {
		ipmb.send(reply); // Done here, no higher levels to report.
		return;
	}

	// Fine, so we have a lamp test too.
	uint32_t lamp_test_remaining_decisec = led.getCurrentLampTestDuration().getTimeout() / pdMS_TO_TICKS(100);
	reply->data[10] = lamp_test_remaining_decisec;
	if (lamp_test_remaining_decisec > 127)
		reply->data[10] = 127;
	reply->data_len = 11;

	ipmb.send(reply);
}
IPMICMD_INDEX_REGISTER(Get_FRU_LED_State);

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

static void ipmicmd_Set_Port_State(IPMBSvc &ipmb, const IPMI_MSG &message) {
	ASSERT_PICMG_IDENTIFIER(ipmb, message);
	if (message.data_len != 6)
		RETURN_ERROR(ipmb, message, IPMI::Completion::Request_Data_Length_Invalid);
	std::vector<uint8_t> linkid(message.data+1, message.data+5);
	bool enable;
	switch (message.data[5]) {
	case 0x00: enable = false; break;
	case 0x01: enable = true; break;
	default: RETURN_ERROR(ipmb, message, IPMI::Completion::Invalid_Data_Field_In_Request);
	}
	if (!payload_manager)
		// Not yet initialized (we got an IPMI message before ipmc_service_init() completed)
		RETURN_ERROR(ipmb, message, IPMI::Completion::Node_Busy);
	payload_manager->updateLinkEnable(LinkDescriptor(linkid, enable));
	ipmb.send(message.prepare_reply({IPMI::Completion::Success, 0}));
}
IPMICMD_INDEX_REGISTER(Set_Port_State);

static void ipmicmd_Get_Port_State(IPMBSvc &ipmb, const IPMI_MSG &message) {
	ASSERT_PICMG_IDENTIFIER(ipmb, message);
}
IPMICMD_INDEX_REGISTER(Get_Port_State);

static void ipmicmd_Compute_Power_Properties(IPMBSvc &ipmb, const IPMI_MSG &message) {
	ASSERT_PICMG_IDENTIFIER(ipmb, message);
	if (message.data_len != 2 || message.data[1] != 0)
		// Not enough parameters, or asking for FRU != 0.  We dont have one of those.
		RETURN_ERROR(ipmb, message, IPMI::Completion::Parameter_Out_Of_Range);
	if (!payload_manager)
		// Not yet initialized (we got an IPMI message before ipmc_service_init() completed)
		RETURN_ERROR(ipmb, message, IPMI::Completion::Node_Busy);
	PowerProperties properties = payload_manager->getPowerProperties(message.data[1], true);
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

	PowerProperties properties = payload_manager->getPowerProperties(message.data[1], false);
	uint8_t new_power_level = properties.current_power_level;
	if (message.data[2] != 0xFF)
		new_power_level = message.data[2];
	else if (message.data[3] == 1)
		new_power_level = properties.desired_power_level;

	if (properties.current_power_level != new_power_level) {
		try {
			payload_manager->setPowerLevel(message.data[1], new_power_level);
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

	PowerProperties properties = payload_manager->getPowerProperties(message.data[1], false);

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
