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

static void ipmicmd_Get_SDR_Repository_Info(IPMBSvc &ipmb, const IPMI_MSG &message) {
	std::shared_ptr<IPMI_MSG> reply = std::make_shared<IPMI_MSG>();
	message.prepare_reply(*reply);
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
	reply->data[14] |= 0<<7; // [7]   Repository has not been overflowed by an Add
	reply->data[14] |= 0<<5; // [6:5] Repository does not specify support for modal or non-modal update.
	reply->data[14] |= 0<<3; // [3]   Delete SDR Command Not Supported
	reply->data[14] |= 0<<2; // [2]   Partial Add Command Not Supported
	reply->data[14] |= 0<<1; // [1]   Reserve SDR Repository Command Not Supported
	reply->data[14] |= 0<<0; // [0]   Get SDR Repository Allocation Information Command Not Supported
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

#if 0 // Unimplemented.
static void ipmicmd_Reserve_SDR_Repository_Storage(IPMBSvc &ipmb, const IPMI_MSG &message) {
	// TODO: Required. PICMG 3.0 REQ 3.353
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

