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

#include <services/ipmi/commands/ipmicmd_index.h>
#include <services/ipmi/ipmbsvc/ipmbsvc.h>
#include <services/ipmi/IPMI.h>

// Bridge Management Commands (ICMB)

#if 0 // Unimplemented.
static void ipmicmd_Get_Bridge_State(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Get_Bridge_State);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Set_Bridge_State(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Set_Bridge_State);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_ICMB_Address(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Get_ICMB_Address);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Set_ICMB_Address(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Set_ICMB_Address);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Set_Bridge_Proxy_Address(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Set_Bridge_Proxy_Address);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Bridge_Statistics(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Get_Bridge_Statistics);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_ICMB_Capabilities(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Get_ICMB_Capabilities);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Clear_Bridge_Statistics(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Clear_Bridge_Statistics);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Bridge_Proxy_Address(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Get_Bridge_Proxy_Address);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_ICMB_Connector_Info(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Get_ICMB_Connector_Info);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_ICMB_Connection_ID(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Get_ICMB_Connection_ID);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Send_ICMB_Connection_ID(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Send_ICMB_Connection_ID);
#endif


// Discovery Commands (ICMB)

#if 0 // Unimplemented.
static void ipmicmd_Prepare_For_Discovery(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Prepare_For_Discovery);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Addresses_Bridge(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Get_Addresses_Bridge);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Set_Discovered(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Set_Discovered);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Chassis_Device_ID(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Get_Chassis_Device_ID);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Set_Chassis_Device_ID(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Set_Chassis_Device_ID);
#endif


// Bridging Commands (ICMB)

#if 0 // Unimplemented.
static void ipmicmd_Bridge_Request(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Bridge_Request);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Bridge_Message(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Bridge_Message);
#endif


// Event Commands (ICMB)

#if 0 // Unimplemented.
static void ipmicmd_Get_Event_Count(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Get_Event_Count);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Set_Event_Destination(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Set_Event_Destination);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Set_Event_Reception_State(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Set_Event_Reception_State);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Send_ICMB_Event_Message(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Send_ICMB_Event_Message);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Event_Destination(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Get_Event_Destination);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Event_Reception_State(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Get_Event_Reception_State);
#endif


// Other Bridge Commands

#if 0 // Unimplemented.
static void ipmicmd_Error_Report(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// Unimplemented.
}
IPMICMD_INDEX_REGISTER(Error_Report);
#endif

