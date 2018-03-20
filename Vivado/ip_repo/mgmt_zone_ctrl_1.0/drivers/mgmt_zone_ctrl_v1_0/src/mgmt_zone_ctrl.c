
/***************************** Include Files *******************************/
#include "mgmt_zone_ctrl.h"
#include "xparameters.h"
#include "xil_io.h"

#define MZ_COUNT_MAX 			(32)
#define MZ_POWER_EN_COUNT 	(32)
#define MZ_HARDFAULT_COUNT  (64)

/*******************************/
/********  Register Map ********/

/* Global Registers*/
#define HARD_FAULT_STATUS_0_REG (0)
#define HARD_FAULT_STATUS_1_REG (4)

#define IRQ_STATUS_REG 		(8)
#define IRQ_EN_REG 			(12)
#define IRQ_ACK_REG 		(16)

/* Individual MZ registers*/
#define MZ_0_ADDR_OFFSET    (32)

#define MZ_0_PWR_STATUS_REG     		(MZ_0_ADDR_OFFSET + 0)
#define MZ_0_HARD_FAULT_MASK_0_REG     	(MZ_0_ADDR_OFFSET + 4)
#define MZ_0_HARD_FAULT_MASK_1_REG      (MZ_0_ADDR_OFFSET + 8)
#define MZ_0_HARD_FAULT_HOLDOFF_REG     (MZ_0_ADDR_OFFSET + 12)
#define MZ_0_SOFT_FAULT_REG      		(MZ_0_ADDR_OFFSET + 16)
#define MZ_0_PWR_ON_INIT_REG     		(MZ_0_ADDR_OFFSET + 20)
#define MZ_0_PWR_OFF_INIT_REG     		(MZ_0_ADDR_OFFSET + 24)
#define MZ_0_PWR_EN_0_CFG_REG           (MZ_0_ADDR_OFFSET + 28)
#define MZ_0_PWR_EN_0_STAT_REG          (MZ_0_PWR_EN_0_CFG_REG + MZ_POWER_EN_COUNT * 4)

#define MZ_2_MZ_ADDR_OFFSET      (512)

#if 0


    constant PWR_EN_AGGR_STATUS             : integer := 20;

    constant PEN_2_PEN_ADDR_OFFSET          : integer := 16;

    constant PWR_EN_0_CFG_0_REG             : integer := 32;
    constant PWR_EN_0_CFG_1_REG             : integer := 36;
    constant PWR_EN_0_INDIV_STATUS_REG      : integer := 40;

    constant MZ_0_ADDR_OFFSET               : integer := (PWR_EN_0_CFG_0_REG + PEN_2_PEN_ADDR_OFFSET*32);

    constant MZ_0_PWR_STATUS_REG            : integer := (MZ_0_ADDR_OFFSET +  0);
    constant MZ_0_HARD_FAULT_MASK_0_REG     : integer := (MZ_0_ADDR_OFFSET +  4);
    constant MZ_0_HARD_FAULT_MASK_1_REG     : integer := (MZ_0_ADDR_OFFSET +  8);
    constant MZ_0_HARD_FAULT_HOLDOFF_REG    : integer := (MZ_0_ADDR_OFFSET + 12);
    constant MZ_0_SOFT_FAULT_REG            : integer := (MZ_0_ADDR_OFFSET + 16);
    constant MZ_0_PWR_ON_INIT_REG           : integer := (MZ_0_ADDR_OFFSET + 20);
    constant MZ_0_PWR_OFF_INIT_REG          : integer := (MZ_0_ADDR_OFFSET + 24);

    constant MZ_2_MZ_ADDR_OFFSET            : integer := 32;
#endif



extern Mgmt_Zone_Ctrl_Config Mgmt_Zone_Ctrl_ConfigTable[];

/************************** Function Definitions ***************************/

#define Mgmt_Zone_Ctrl_WriteReg(BaseAddress, RegOffset, Data) \
	Xil_Out32((BaseAddress) + (RegOffset), (u32)(Data))

#define Mgmt_Zone_Ctrl_ReadReg(BaseAddress, RegOffset) \
	Xil_In32((BaseAddress) + (RegOffset))

/******************************************************************************/
/**
* Lookup the device configuration based on the unique device ID.  The table
* ConfigTable contains the configuration info for each device in the system.
*
* @param	DeviceId is the device identifier to lookup.
*
* @return
*		 - A pointer of data type Mgmt_Zone_Ctrl_Config which points to the
*		device configuration if DeviceID is found.
* 		- NULL if DeviceID is not found.
*
* @note		None.
*
******************************************************************************/
Mgmt_Zone_Ctrl_Config *Mgmt_Zone_Ctrl_LookupConfig(u16 DeviceId)
{
	Mgmt_Zone_Ctrl_Config *CfgPtr = NULL;

	int Index;

	for (Index = 0; Index < XPAR_MGMT_ZONE_CTRL_NUM_INSTANCES; Index++) {
		if (Mgmt_Zone_Ctrl_ConfigTable[Index].DeviceId == DeviceId) {
			CfgPtr = &Mgmt_Zone_Ctrl_ConfigTable[Index];
			break;
		}
	}
	return CfgPtr;
}

/****************************************************************************/
/**
* Initialize the Mgmt_Zone_Ctrl instance provided by the caller based on the
* given configuration data.
*
* Nothing is done except to initialize the InstancePtr.
*
* @param	InstancePtr is a pointer to an Mgmt_Zone_Ctrl instance. The memory the
*		pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the driver through the Mgmt_Zone_Ctrl API must be
*		made with this pointer.
* @param	Config is a reference to a structure containing information
*		about a specific GPIO device. This function initializes an
*		InstancePtr object for a specific device specified by the
*		contents of Config. This function can initialize multiple
*		instance objects with the use of multiple calls giving different
*		Config information on each call.
* @param 	EffectiveAddr is the device base address in the virtual memory
*		address space. The caller is responsible for keeping the address
*		mapping from EffectiveAddr to the device physical base address
*		unchanged once this function is invoked. Unexpected errors may
*		occur if the address mapping changes after this function is
*		called. If address translation is not used, use
*		Config->BaseAddress for this parameters, passing the physical
*		address instead.
*
* @return
* 		- XST_SUCCESS if the initialization is successfull.
*
* @note		None.
*
*****************************************************************************/
int Mgmt_Zone_Ctrl_CfgInitialize(Mgmt_Zone_Ctrl * InstancePtr, Mgmt_Zone_Ctrl_Config * Config,
			UINTPTR EffectiveAddr)
{
	/* Assert arguments */
	Xil_AssertNonvoid(InstancePtr != NULL);

	/* Set some default values. */
	InstancePtr->BaseAddress = EffectiveAddr;

	InstancePtr->mz_cnt = Config->MZ_CNT;
	InstancePtr->hf_cnt = Config->HF_CNT;
	InstancePtr->pwren_cnt = Config->PWREN_CNT;

	/*
	 * Indicate the instance is now ready to use, initialized without error
	 */
	InstancePtr->IsReady = XIL_COMPONENT_IS_READY;
	return (XST_SUCCESS);
}

/****************************************************************************/
/**
* Initialize the Mgmt_Zone_Ctrl instance provided by the caller based on the
* given DeviceID.
*
* Nothing is done except to initialize the InstancePtr.
*
* @param	InstancePtr is a pointer to an Mgmt_Zone_Ctrl instance. The memory the
*		pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the instance/driver through the Mgmt_Zone_Ctrl API
*		must be made with this pointer.
* @param	DeviceId is the unique id of the device controlled by this Mgmt_Zone_Ctrl
*		instance. Passing in a device id associates the generic Mgmt_Zone_Ctrl
*		instance to a specific device, as chosen by the caller or
*		application developer.
*
* @return
*		- XST_SUCCESS if the initialization was successfull.
* 		- XST_DEVICE_NOT_FOUND  if the device configuration data was not
*		found for a device with the supplied device ID.
*
* @note		None.
*
*****************************************************************************/
int Mgmt_Zone_Ctrl_Initialize(Mgmt_Zone_Ctrl * InstancePtr, u16 DeviceId)
{
	Mgmt_Zone_Ctrl_Config *ConfigPtr;

	/*
	 * Assert arguments
	 */
	Xil_AssertNonvoid(InstancePtr != NULL);

	/*
	 * Lookup configuration data in the device configuration table.
	 * Use this configuration info down below when initializing this
	 * driver.
	 */
	ConfigPtr = Mgmt_Zone_Ctrl_LookupConfig(DeviceId);
	if (ConfigPtr == (Mgmt_Zone_Ctrl_Config *) NULL) {
		InstancePtr->IsReady = 0;
		return (XST_DEVICE_NOT_FOUND);
	}

	return Mgmt_Zone_Ctrl_CfgInitialize(InstancePtr, ConfigPtr, ConfigPtr->BaseAddress);
}


u64 Mgmt_Zone_Ctrl_Get_Hard_Fault_Status(Mgmt_Zone_Ctrl *InstancePtr)
{
 u32 tmp0, tmp1;

 tmp0 = Mgmt_Zone_Ctrl_ReadReg(InstancePtr->BaseAddress, HARD_FAULT_STATUS_0_REG);
 tmp1 = Mgmt_Zone_Ctrl_ReadReg(InstancePtr->BaseAddress, HARD_FAULT_STATUS_1_REG);

 return (u64)(((u64)tmp1 << 32) | tmp0);
}

void Mgmt_Zone_Ctrl_Set_MZ_Cfg(Mgmt_Zone_Ctrl *InstancePtr, u32 MZ, MZ_config cfg)
{
	 Mgmt_Zone_Ctrl_WriteReg(InstancePtr->BaseAddress, MZ * MZ_2_MZ_ADDR_OFFSET + MZ_0_HARD_FAULT_MASK_0_REG, (u32)(cfg.hardfault_mask & 0xFFFFFFFF));
	 Mgmt_Zone_Ctrl_WriteReg(InstancePtr->BaseAddress, MZ * MZ_2_MZ_ADDR_OFFSET + MZ_0_HARD_FAULT_MASK_1_REG, (u32)((cfg.hardfault_mask >> 32)));

	 Mgmt_Zone_Ctrl_WriteReg(InstancePtr->BaseAddress, MZ * MZ_2_MZ_ADDR_OFFSET + MZ_0_HARD_FAULT_HOLDOFF_REG, cfg.fault_holdoff);

	 for (int idx = 0; idx < 32; idx++)
		 Mgmt_Zone_Ctrl_WriteReg(InstancePtr->BaseAddress, MZ * MZ_2_MZ_ADDR_OFFSET + MZ_0_PWR_EN_0_CFG_REG + idx*4, cfg.pwren_cfg[idx]);
}

void Mgmt_Zone_Ctrl_Get_MZ_Cfg(Mgmt_Zone_Ctrl *InstancePtr, u32 MZ, MZ_config * cfg)
{
	cfg->fault_holdoff = Mgmt_Zone_Ctrl_ReadReg(InstancePtr->BaseAddress, MZ * MZ_2_MZ_ADDR_OFFSET + MZ_0_HARD_FAULT_HOLDOFF_REG);

	//TODO: hardfault readback
	for (int idx = 0; idx < 32; idx++)
		cfg->pwren_cfg[idx] = Mgmt_Zone_Ctrl_ReadReg(InstancePtr->BaseAddress, MZ * MZ_2_MZ_ADDR_OFFSET + MZ_0_PWR_EN_0_CFG_REG + idx*4);
}

 MZ_pwr Mgmt_Zone_Ctrl_Get_MZ_Status(Mgmt_Zone_Ctrl *InstancePtr, u32 MZ)
{
		// TODO

#if 0
	status->MZ_pwr_status = Mgmt_Zone_Ctrl_ReadReg(InstancePtr->BaseAddress, MZ * MZ_2_MZ_ADDR_OFFSET + MZ_0_PWR_STATUS_REG);

	for (int idx = 0; idx < 32; idx++)
		status->pwren_stat[idx] = Mgmt_Zone_Ctrl_ReadReg(InstancePtr->BaseAddress, MZ * MZ_2_MZ_ADDR_OFFSET + MZ_0_PWR_EN_0_STAT_REG + idx*4);
#endif
	return MZ_PWR_OFF;

}

void Mgmt_Zone_Ctrl_Pwr_ON_Seq(Mgmt_Zone_Ctrl *InstancePtr, u32 MZ)
{
	 Mgmt_Zone_Ctrl_WriteReg(InstancePtr->BaseAddress, MZ * MZ_2_MZ_ADDR_OFFSET + MZ_0_PWR_ON_INIT_REG, 1 << MZ);
}

void Mgmt_Zone_Ctrl_Pwr_OFF_Seq(Mgmt_Zone_Ctrl *InstancePtr, u32 MZ)
{
	 Mgmt_Zone_Ctrl_WriteReg(InstancePtr->BaseAddress, MZ * MZ_2_MZ_ADDR_OFFSET + MZ_0_PWR_OFF_INIT_REG, 1 << MZ);
}

void Mgmt_Zone_Ctrl_Dispatch_Soft_Fault(Mgmt_Zone_Ctrl *InstancePtr, u32 MZ)
{
	 Mgmt_Zone_Ctrl_WriteReg(InstancePtr->BaseAddress, MZ * MZ_2_MZ_ADDR_OFFSET + MZ_0_SOFT_FAULT_REG, 1 << MZ);
}

void Mgmt_Zone_Ctrl_Set_IRQ_Enables(Mgmt_Zone_Ctrl *InstancePtr, u32 irq_enables)
{
	 Mgmt_Zone_Ctrl_WriteReg(InstancePtr->BaseAddress, IRQ_EN_REG, irq_enables);
}

u32 Mgmt_Zone_Ctrl_Get_IRQ_Enables(Mgmt_Zone_Ctrl *InstancePtr)
{
	return 	 Mgmt_Zone_Ctrl_ReadReg(InstancePtr->BaseAddress, IRQ_EN_REG);
}

void Mgmt_Zone_Ctrl_Ack_IRQ(Mgmt_Zone_Ctrl *InstancePtr, u32 irq_acks)
{
	 Mgmt_Zone_Ctrl_WriteReg(InstancePtr->BaseAddress, IRQ_ACK_REG, irq_acks);
}

u32 Mgmt_Zone_Ctrl_Get_IRQ_Status(Mgmt_Zone_Ctrl *InstancePtr)
{
	return 	 Mgmt_Zone_Ctrl_ReadReg(InstancePtr->BaseAddress, IRQ_STATUS_REG);
}
