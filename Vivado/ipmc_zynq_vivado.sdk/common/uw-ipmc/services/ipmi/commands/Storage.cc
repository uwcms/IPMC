#include <services/ipmi/IPMI.h>
#include <services/ipmi/ipmbsvc/IPMBSvc.h>
#include "IPMICmd_Index.h"

// FRU Device Commands

#if 0 // Unimplemented.
static void ipmicmd_Get_FRU_Inventory_Area_Info(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Get_FRU_Inventory_Area_Info);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Read_FRU_Data(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Read_FRU_Data);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Write_FRU_Data(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Write_FRU_Data);
#endif


// SDR Device Commands

#if 0 // Unimplemented.
static void ipmicmd_Get_SDR_Repository_Info(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Get_SDR_Repository_Info);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_SDR_Repository_Allocation_Info(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Get_SDR_Repository_Allocation_Info);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Reserve_SDR_Repository_Storage(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Reserve_SDR_Repository_Storage);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_SDR(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Get_SDR);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Add_SDR(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Add_SDR);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Partial_Add_SDR(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Partial_Add_SDR);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Delete_SDR(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Delete_SDR);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Clear_SDR_Repository(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Clear_SDR_Repository);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_SDR_Repository_Time(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Get_SDR_Repository_Time);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Set_SDR_Repository_Time(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Set_SDR_Repository_Time);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Enter_SDR_Repository_Update_Mode(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Enter_SDR_Repository_Update_Mode);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Exit_SDR_Repository_Update_Mode(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Exit_SDR_Repository_Update_Mode);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Run_Initialization_Agent(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Run_Initialization_Agent);
#endif


// SEL Device Commands

#if 0 // Unimplemented.
static void ipmicmd_Get_SEL_Info(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Get_SEL_Info);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_SEL_Allocation_Info(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Get_SEL_Allocation_Info);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Reserve_SEL(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Reserve_SEL);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_SEL_Entry(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Get_SEL_Entry);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Add_SEL_Entry(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Add_SEL_Entry);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Partial_Add_SEL_Entry(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Partial_Add_SEL_Entry);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Delete_SEL_Entry(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Delete_SEL_Entry);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Clear_SEL(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Clear_SEL);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_SEL_Time(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Get_SEL_Time);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Set_SEL_Time(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Set_SEL_Time);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Get_Auxiliary_Log_Status(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Get_Auxiliary_Log_Status);
#endif

#if 0 // Unimplemented.
static void ipmicmd_Set_Auxiliary_Log_Status(IPMBSvc &ipmb, const IPMI_MSG &message) {

}
IPMICMD_INDEX_REGISTER(Set_Auxiliary_Log_Status);
#endif

