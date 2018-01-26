#include <services/ipmi/IPMI.h>
#include <services/ipmi/ipmbsvc/IPMICommandParser.h>
#include "IPMICmd_Index.h"

// IPM Device “Global” Commands

#if 0 // Unimplemented.
static void ipmicmd_Get_Device_ID(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Get_Device_ID);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Cold_Reset(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Cold_Reset);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Warm_Reset(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Warm_Reset);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Self_Test_Results(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Get_Self_Test_Results);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Manufacturing_Test_On(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Manufacturing_Test_On);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Set_ACPI_Power_State(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Set_ACPI_Power_State);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_ACPI_Power_State(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Get_ACPI_Power_State);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Device_GUID(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Get_Device_GUID);
#endif


// BMC Watchdog Timer Commands

#if 0 // Unimplemented.
static void ipmicmd_Reset_Watchdog_Timer(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Reset_Watchdog_Timer);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Set_Watchdog_Timer(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Set_Watchdog_Timer);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Watchdog_Timer(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Get_Watchdog_Timer);
#endif


// BMC Device and Messaging Commands

#if 0 // Unimplemented.
static void ipmicmd_Set_BMC_Global_Enables(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Set_BMC_Global_Enables);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_BMC_Global_Enables(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Get_BMC_Global_Enables);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Clear_Message_Flags(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Clear_Message_Flags);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Message_Flags(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Get_Message_Flags);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Enable_Message_Channel_Receive(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Enable_Message_Channel_Receive);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Message(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Get_Message);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Send_Message(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Send_Message);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Read_Event_Message_Buffer(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Read_Event_Message_Buffer);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_BT_Interface_Capabilities(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Get_BT_Interface_Capabilities);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_System_GUID(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Get_System_GUID);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Channel_Authentication_Capabilities(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Get_Channel_Authentication_Capabilities);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Session_Challenge(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Get_Session_Challenge);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Activate_Session(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Activate_Session);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Set_Session_Privilege_Level(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Set_Session_Privilege_Level);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Close_Session(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Close_Session);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Session_Info(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Get_Session_Info);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_AuthCode(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Get_AuthCode);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Set_Channel_Access(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Set_Channel_Access);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Channel_Access(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Get_Channel_Access);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Channel_Info(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Get_Channel_Info);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Set_User_Access(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Set_User_Access);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_User_Access(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Get_User_Access);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Set_User_Name(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Set_User_Name);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_User_Name(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Get_User_Name);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Set_User_Password(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Set_User_Password);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Master_WriteRead(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Master_WriteRead);
#endif

