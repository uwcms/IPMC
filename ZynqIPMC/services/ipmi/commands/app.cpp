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

#include <services/ipmi/IPMI.h>
#include <IPMC.h>
#include <Core.h>
#include <services/ipmi/commands/ipmicmd_index.h>
#include <services/ipmi/ipmbsvc/ipmbsvc.h>
#include <services/ipmi/sdr/sensor_data_repository.h>

// IPM Device “Global” Commands

static void ipmicmd_Get_Device_ID(IPMBSvc &ipmb, const IPMI_MSG &message) {
	std::shared_ptr<IPMI_MSG> reply = message.prepare_reply();
	reply->data[0] = IPMI::Completion::Success;
	reply->data[1] = 0; // Device ID (00 = Unspecified) TODO
	reply->data[2] |= 1<<7; // Device provides Device SDRs
	reply->data[2] |= IPMC_HW_REVISION & 0x0f; // Device revision, binary encoded.
	reply->data[3] = IPMC_FW_REVISION[0] & 0x7f;
	if (/* !SDR_REPOSITORY_LOADED */ false)
		reply->data[3] |= 0x80;
	reply->data[4] = ( (IPMC_FW_REVISION[1]/10) << 4 ) | (IPMC_FW_REVISION[1]%10); // BCD FW Minor Rev
	reply->data[5] = 0x02; // IPMI Version, BCD, Reverse Nibbles.
	reply->data[6] |= 1<<0; // Sensor Device
	reply->data[6] |= 1<<1; // SDR Repository Device
	reply->data[6] |= 0<<2; // SEL Device
	reply->data[6] |= 1<<3; // FRU Inventory Device
	reply->data[6] |= 0<<4; // IPMB Event Receiver
	reply->data[6] |= 1<<5; // IPMB Event Generator
	reply->data[6] |= 0<<6; // Bridge
	reply->data[6] |= 0<<7; // Chassis Device
	reply->data[7]  = 0; // Manufacturer ID (LSB):   Unspecified[0]
	reply->data[8]  = 0; // Manufacturer ID (MidSB): Unspecified[1]
	reply->data[9]  = 0; // Manufacturer ID (MSB):   Unspecified[2]
	reply->data[10] = 0; // Product ID (LSB): Unspecified[0]
	reply->data[11] = 0; // Product ID (MSB): Unspecified[1]
	const long int git = (GIT_SHORT_INT << 4) | (GIT_STATUS[0] ? 1 : 0);
	reply->data[12] = (git >> 24) & 0xff; // Aux Firmware Revision
	reply->data[13] = (git >> 16) & 0xff; // Aux Firmware Revision
	reply->data[14] = (git >>  8) & 0xff; // Aux Firmware Revision
	reply->data[15] = (git >>  0) & 0xff; // Aux Firmware Revision
	reply->data_len = 16;
	ipmb.send(reply);
}
IPMICMD_INDEX_REGISTER(Get_Device_ID);

#if 0 // Unimplemented.
static void ipmicmd_Cold_Reset(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented
}
IPMICMD_INDEX_REGISTER(Cold_Reset);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Warm_Reset(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented
}
IPMICMD_INDEX_REGISTER(Warm_Reset);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Self_Test_Results(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented
}
IPMICMD_INDEX_REGISTER(Get_Self_Test_Results);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Manufacturing_Test_On(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented
}
IPMICMD_INDEX_REGISTER(Manufacturing_Test_On);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Set_ACPI_Power_State(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented
}
IPMICMD_INDEX_REGISTER(Set_ACPI_Power_State);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_ACPI_Power_State(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented
}
IPMICMD_INDEX_REGISTER(Get_ACPI_Power_State);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Device_GUID(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented
}
IPMICMD_INDEX_REGISTER(Get_Device_GUID);
#endif


// BMC Watchdog Timer Commands

#if 0 // Unimplemented.
static void ipmicmd_Reset_Watchdog_Timer(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented
}
IPMICMD_INDEX_REGISTER(Reset_Watchdog_Timer);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Set_Watchdog_Timer(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented
}
IPMICMD_INDEX_REGISTER(Set_Watchdog_Timer);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Watchdog_Timer(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented
}
IPMICMD_INDEX_REGISTER(Get_Watchdog_Timer);
#endif


// BMC Device and Messaging Commands

#if 0 // Unimplemented.
static void ipmicmd_Set_BMC_Global_Enables(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented
}
IPMICMD_INDEX_REGISTER(Set_BMC_Global_Enables);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_BMC_Global_Enables(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented
}
IPMICMD_INDEX_REGISTER(Get_BMC_Global_Enables);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Clear_Message_Flags(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented
}
IPMICMD_INDEX_REGISTER(Clear_Message_Flags);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Message_Flags(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented
}
IPMICMD_INDEX_REGISTER(Get_Message_Flags);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Enable_Message_Channel_Receive(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented
}
IPMICMD_INDEX_REGISTER(Enable_Message_Channel_Receive);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Message(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented
}
IPMICMD_INDEX_REGISTER(Get_Message);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Send_Message(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented
}
IPMICMD_INDEX_REGISTER(Send_Message);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Read_Event_Message_Buffer(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented
}
IPMICMD_INDEX_REGISTER(Read_Event_Message_Buffer);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_BT_Interface_Capabilities(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented
}
IPMICMD_INDEX_REGISTER(Get_BT_Interface_Capabilities);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_System_GUID(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented
}
IPMICMD_INDEX_REGISTER(Get_System_GUID);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Channel_Authentication_Capabilities(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented
}
IPMICMD_INDEX_REGISTER(Get_Channel_Authentication_Capabilities);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Session_Challenge(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented
}
IPMICMD_INDEX_REGISTER(Get_Session_Challenge);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Activate_Session(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented
}
IPMICMD_INDEX_REGISTER(Activate_Session);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Set_Session_Privilege_Level(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented
}
IPMICMD_INDEX_REGISTER(Set_Session_Privilege_Level);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Close_Session(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented
}
IPMICMD_INDEX_REGISTER(Close_Session);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Session_Info(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented
}
IPMICMD_INDEX_REGISTER(Get_Session_Info);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_AuthCode(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented
}
IPMICMD_INDEX_REGISTER(Get_AuthCode);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Set_Channel_Access(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented
}
IPMICMD_INDEX_REGISTER(Set_Channel_Access);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Channel_Access(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented
}
IPMICMD_INDEX_REGISTER(Get_Channel_Access);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Channel_Info(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented
}
IPMICMD_INDEX_REGISTER(Get_Channel_Info);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Set_User_Access(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented
}
IPMICMD_INDEX_REGISTER(Set_User_Access);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_User_Access(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented
}
IPMICMD_INDEX_REGISTER(Get_User_Access);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Set_User_Name(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented
}
IPMICMD_INDEX_REGISTER(Set_User_Name);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_User_Name(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented
}
IPMICMD_INDEX_REGISTER(Get_User_Name);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Set_User_Password(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented
}
IPMICMD_INDEX_REGISTER(Set_User_Password);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Activate_Payload_for_the_IPMI_Trace_Payload(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented
}
IPMICMD_INDEX_REGISTER(Activate_Payload_for_the_IPMI_Trace_Payload);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Master_WriteRead(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented
}
IPMICMD_INDEX_REGISTER(Master_WriteRead);
#endif

