
/***************************** Include Files *******************************/
#include "pyld_pwr_ctrl.h"
#include "xparameters.h"
#include "xil_io.h"

extern Pyld_Pwr_Ctrl_Config Pyld_Pwr_Ctrl_ConfigTable[];

/************************** Function Definitions ***************************/

#define Pyld_Pwr_Ctrl_WriteReg(BaseAddress, RegOffset, Data) \
	Xil_Out32((BaseAddress) + (RegOffset), (u32)(Data))

#define Pyld_Pwr_Ctrl_ReadReg(BaseAddress, RegOffset) \
	Xil_In32((BaseAddress) + (RegOffset))

/******************************************************************************/
/**
 * Lookup the device configuration based on the unique device ID.  The table
 * ConfigTable contains the configuration info for each device in the system.
 *
 * @param	DeviceId is the device identifier to lookup.
 *
 * @return
 *		 - A pointer of data type Pyld_Pwr_Ctrl_Config which points to the
 *		device configuration if DeviceID is found.
 * 		- NULL if DeviceID is not found.
 *
 * @note		None.
 *
 ******************************************************************************/
Pyld_Pwr_Ctrl_Config *Pyld_Pwr_Ctrl_LookupConfig(u16 DeviceId) {
	Pyld_Pwr_Ctrl_Config *CfgPtr = NULL;

	int Index;

	for (Index = 0; Index < XPAR_PYLD_PWR_CTRL_NUM_INSTANCES; Index++) {
		if (Pyld_Pwr_Ctrl_ConfigTable[Index].DeviceId == DeviceId) {
			CfgPtr = &Pyld_Pwr_Ctrl_ConfigTable[Index];
			break;
		}
	}

	return CfgPtr;
}

/****************************************************************************/
/**
 * Initialize the Pyld_Pwr_Ctrl instance provided by the caller based on the
 * given DeviceID.
 *
 * Nothing is done except to initialize the InstancePtr.
 *
 * @param	InstancePtr is a pointer to an Pyld_Pwr_Ctrl instance. The memory the
 *		pointer references must be pre-allocated by the caller. Further
 *		calls to manipulate the instance/driver through the Pyld_Pwr_Ctrl API
 *		must be made with this pointer.
 * @param	DeviceId is the unique id of the device controlled by this Pyld_Pwr_Ctrl
 *		instance. Passing in a device id associates the generic Pyld_Pwr_Ctrl
 *		instance to a specific device, as chosen by the caller or
 *		application developer.
 *
 * @return
 *		- XST_SUCCESS if the initialization was successful.
 * 		- XST_DEVICE_NOT_FOUND  if the device configuration data was not
 *		found for a device with the supplied device ID.
 *
 * @note		None.
 *
 *****************************************************************************/
int Pyld_Pwr_Ctrl_Initialize(Pyld_Pwr_Ctrl * InstancePtr, u16 DeviceId) {
	Pyld_Pwr_Ctrl_Config *ConfigPtr;

	/*
	 * Assert arguments
	 */
	Xil_AssertNonvoid(InstancePtr != NULL);

	/*
	 * Lookup configuration data in the device configuration table.
	 * Use this configuration info down below when initializing this
	 * driver.
	 */
	ConfigPtr = Pyld_Pwr_Ctrl_LookupConfig(DeviceId);
	if (ConfigPtr == (Pyld_Pwr_Ctrl_Config *) NULL) {
		InstancePtr->IsReady = 0;
		return (XST_DEVICE_NOT_FOUND);
	}

	return Pyld_Pwr_Ctrl_CfgInitialize(InstancePtr, ConfigPtr,
			ConfigPtr->BaseAddress);
}

/****************************************************************************/
/**
 * Initialize the Pyld_Pwr_Ctrl instance provided by the caller based on the
 * given configuration data.
 *
 * Nothing is done except to initialize the InstancePtr.
 *
 * @param	InstancePtr is a pointer to an Pyld_Pwr_Ctrl instance. The memory the
 *		pointer references must be pre-allocated by the caller. Further
 *		calls to manipulate the driver through the Pyld_Pwr_Ctrl API must be
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
 * 		- XST_SUCCESS if the initialization is successful.
 *
 * @note		None.
 *
 *****************************************************************************/
int Pyld_Pwr_Ctrl_CfgInitialize(Pyld_Pwr_Ctrl * InstancePtr,
		Pyld_Pwr_Ctrl_Config * Config, UINTPTR EffectiveAddr) {
	/* Assert arguments */
	Xil_AssertNonvoid(InstancePtr != NULL);

	/* Set some default values. */
	InstancePtr->BaseAddress = EffectiveAddr;

	InstancePtr->PECount = Config->PECount;
	InstancePtr->PGCount = Config->PGCount;

	/*
	 * Indicate the instance is now ready to use, initialized without error
	 */
	InstancePtr->IsReady = XIL_COMPONENT_IS_READY;
	return (XST_SUCCESS);
}

/****************************************************************************/
/**
* Get Payload Power Controller Core Version
*
* @param	InstancePtr is a pointer to an Pyld_Pwr_Ctrl instance. The memory the
*		pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the driver through the Pyld_Pwr_Ctrl API must be
*		made with this pointer.
*
* @note		None.
*
*****************************************************************************/
u32 Pyld_Pwr_Ctrl_Get_Core_Ver(Pyld_Pwr_Ctrl *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
	return Pyld_Pwr_Ctrl_ReadReg(InstancePtr->BaseAddress, CORE_VER_REG);
}

/****************************************************************************/
/**
* Get number of Power Enable pins managed by this instance of Payload Power Controller
*
* @param	InstancePtr is a pointer to an Pyld_Pwr_Ctrl instance. The memory the
*		pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the driver through the Pyld_Pwr_Ctrl API must be
*		made with this pointer.
*
* @note		None.
*
*****************************************************************************/
u32 Pyld_Pwr_Ctrl_Get_PE_Cnt(Pyld_Pwr_Ctrl *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
	return InstancePtr->PECount;
}

/****************************************************************************/
/**
* Get number of Power Good pins managed by this instance of Payload Power Controller
*
* @param	InstancePtr is a pointer to an Pyld_Pwr_Ctrl instance. The memory the
*		pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the driver through the Pyld_Pwr_Ctrl API must be
*		made with this pointer.
*
* @note		None.
*
*****************************************************************************/
u32 Pyld_Pwr_Ctrl_Get_PG_Cnt(Pyld_Pwr_Ctrl *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
	return InstancePtr->PGCount;
}

/****************************************************************************/
/**
* Get PE pin configuration
*
* @param	InstancePtr is a pointer to an Pyld_Pwr_Ctrl instance. The memory the
*		pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the driver through the Pyld_Pwr_Ctrl API must be
*		made with this pointer.
*
* @param pin
*
* @param PE_cfg
*
* @note		None.
*
*****************************************************************************/
void Pyld_Pwr_Ctrl_Get_Pin_Cfg(Pyld_Pwr_Ctrl *InstancePtr, u32 pin, PE_cfg_t * PE_cfg) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(pin < InstancePtr->PECount);

	u32 master_config, seq_tmr;

	master_config = Pyld_Pwr_Ctrl_ReadReg(InstancePtr->BaseAddress, PE_0_MASTER_CFG_REG + PE_2_PE_ADDR_OFFSET * pin);
	seq_tmr = Pyld_Pwr_Ctrl_ReadReg(InstancePtr->BaseAddress, PE_0_SEQ_TMR_CFG_REG + PE_2_PE_ADDR_OFFSET * pin);

	PE_cfg->seq_tmr = seq_tmr;
	PE_cfg->group = (master_config & 0x7);
	PE_cfg->sw_pd_en = (master_config >> 4) & 0x1;
	PE_cfg->ext_pd_en = (master_config >> 5) & 0x1;

}

/****************************************************************************/
/**
* Set PE pin configuration
*
* @param	InstancePtr is a pointer to an Pyld_Pwr_Ctrl instance. The memory the
*		pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the driver through the Pyld_Pwr_Ctrl API must be
*		made with this pointer.
*
* @param pin
*
* @param PE_cfg
*
* @note		None.
*
*****************************************************************************/
void Pyld_Pwr_Ctrl_Set_Pin_Cfg(Pyld_Pwr_Ctrl *InstancePtr, u32 pin, PE_cfg_t PE_cfg) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(pin < InstancePtr->PECount);

	u32 master_config = 0;
	u32 seq_tmr = 0;

	master_config = master_config | (PE_cfg.group & 0x7);
	master_config = master_config | (PE_cfg.sw_pd_en << 4);
	master_config = master_config | (PE_cfg.ext_pd_en << 5);

	seq_tmr = PE_cfg.seq_tmr;

	Pyld_Pwr_Ctrl_WriteReg(InstancePtr->BaseAddress, PE_0_MASTER_CFG_REG + PE_2_PE_ADDR_OFFSET * pin, master_config);

	Pyld_Pwr_Ctrl_WriteReg(InstancePtr->BaseAddress, PE_0_SEQ_TMR_CFG_REG + PE_2_PE_ADDR_OFFSET * pin, seq_tmr);

}

/****************************************************************************/
/**
* Force immediate Power Down of all PD-enabled pins
*
* @param	InstancePtr is a pointer to an Pyld_Pwr_Ctrl instance. The memory the
*		pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the driver through the Pyld_Pwr_Ctrl API must be
*		made with this pointer.
*
*
* @note		None.
*
*****************************************************************************/
void Pyld_Pwr_Ctrl_PDown_Force(Pyld_Pwr_Ctrl *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
	Pyld_Pwr_Ctrl_WriteReg(InstancePtr->BaseAddress, SW_OFF_REG, SW_OFF_MAGIC_WORD);
}

/****************************************************************************/
/**
* Release immediate Power Down
*
* @param	InstancePtr is a pointer to an Pyld_Pwr_Ctrl instance. The memory the
*		pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the driver through the Pyld_Pwr_Ctrl API must be
*		made with this pointer.
*
*
* @note		None.
*
*****************************************************************************/
void Pyld_Pwr_Ctrl_PDown_Release(Pyld_Pwr_Ctrl *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
	Pyld_Pwr_Ctrl_WriteReg(InstancePtr->BaseAddress, SW_OFF_REG, 0);
}

/****************************************************************************/
/**
* Initiate Power Down Sequence
*
* @param	InstancePtr is a pointer to an Pyld_Pwr_Ctrl instance. The memory the
*		pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the driver through the Pyld_Pwr_Ctrl API must be
*		made with this pointer.
*
* @group
*
* @note		None.
*
*****************************************************************************/
void Pyld_Pwr_Ctrl_Init_PDown_Seq(Pyld_Pwr_Ctrl *InstancePtr, u32 group) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(group < 8);

	Pyld_Pwr_Ctrl_WriteReg(InstancePtr->BaseAddress, PD_INIT_REG, group);
}

/****************************************************************************/
/**
* Initiate Power Up Sequence
*
* @param	InstancePtr is a pointer to an Pyld_Pwr_Ctrl instance. The memory the
*		pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the driver through the Pyld_Pwr_Ctrl API must be
*		made with this pointer.
*
* @group
*
* @note		None.
*
*****************************************************************************/
void Pyld_Pwr_Ctrl_Init_PUp_Seq(Pyld_Pwr_Ctrl *InstancePtr, u32 group) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(group < 8);
	Pyld_Pwr_Ctrl_WriteReg(InstancePtr->BaseAddress, PU_INIT_REG, group);
}
