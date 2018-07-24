

/***************************** Include Files *******************************/
#include "xparameters.h"
#include "xil_io.h"
#include "ad7689_s.h"


/* IP Core register map */
#define RESET_REG 							(0)
#define CH2CH_SAMPLE_CLK_PERIOD_REG 		(4)
#define	SAMPLE_FREQ_MEAS_REG 				(8)
#define	SAMPLE_CH0_CNT_REG 					(12)

#define OVRRD_MASTER_EN_REG 				(16)
#define OVRRD_ENABLES_REG 					(20)

#define ADC_CH0_REG 						(32)

#define OVRRD_VAL_CH0_REG 					(80)


#define MASTER_OVVRD_ENABLE_VAL 			(0xC0DE1357)


extern AD7689_S_Config AD7689_S_ConfigTable[];

/************************** Function Definitions ***************************/

#define AD7689_S_WriteReg(BaseAddress, RegOffset, Data) \
	Xil_Out32((BaseAddress) + (RegOffset), (u32)(Data))

#define AD7689_S_ReadReg(BaseAddress, RegOffset) \
	Xil_In32((BaseAddress) + (RegOffset))

/******************************************************************************/
/**
* Lookup the device configuration based on the unique device ID.  The table
* ConfigTable contains the configuration info for each device in the system.
*
* @param	DeviceId is the device identifier to lookup.
*
* @return
*		 - A pointer of data type AD7689_S_Config which points to the
*		device configuration if DeviceID is found.
* 		- NULL if DeviceID is not found.
*
* @note		None.
*
******************************************************************************/
AD7689_S_Config *AD7689_S_LookupConfig(u16 DeviceId)
{
	AD7689_S_Config *CfgPtr = NULL;

	int Index;

	for (Index = 0; Index < XPAR_AD7689_S_NUM_INSTANCES; Index++) {
		if (AD7689_S_ConfigTable[Index].DeviceId == DeviceId) {
			CfgPtr = &AD7689_S_ConfigTable[Index];
			break;
		}
	}

	return CfgPtr;
}

/****************************************************************************/
/**
* Initialize the AD7689_S instance provided by the caller based on the
* given configuration data.
*
* Nothing is done except to initialize the InstancePtr.
*
* @param	InstancePtr is a pointer to an AD7689_S instance. The memory the
*		pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the driver through the AD7689_S API must be
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
int AD7689_S_CfgInitialize(AD7689_S * InstancePtr, AD7689_S_Config * Config,
			UINTPTR EffectiveAddr)
{
	/* Assert arguments */
	Xil_AssertNonvoid(InstancePtr != NULL);

	/* Set some default values. */
	InstancePtr->BaseAddress = EffectiveAddr;

	InstancePtr->InterfaceCount = Config->InterfaceCount;

	/*
	 * Indicate the instance is now ready to use, initialized without error
	 */
	InstancePtr->IsReady = XIL_COMPONENT_IS_READY;
	return (XST_SUCCESS);
}

/****************************************************************************/
/**
* Initialize the AD7689_S instance provided by the caller based on the
* given DeviceID.
*
* Nothing is done except to initialize the InstancePtr.
*
* @param	InstancePtr is a pointer to an AD7689_S instance. The memory the
*		pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the instance/driver through the AD7689_S API
*		must be made with this pointer.
* @param	DeviceId is the unique id of the device controlled by this AD7689_S
*		instance. Passing in a device id associates the generic AD7689_S
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
int AD7689_S_Initialize(AD7689_S * InstancePtr, u16 DeviceId)
{
	AD7689_S_Config *ConfigPtr;

	/*
	 * Assert arguments
	 */
	Xil_AssertNonvoid(InstancePtr != NULL);

	/*
	 * Lookup configuration data in the device configuration table.
	 * Use this configuration info down below when initializing this
	 * driver.
	 */
	ConfigPtr = AD7689_S_LookupConfig(DeviceId);
	if (ConfigPtr == (AD7689_S_Config *) NULL) {
		InstancePtr->IsReady = 0;
		return (XST_DEVICE_NOT_FOUND);
	}

	return AD7689_S_CfgInitialize(InstancePtr, ConfigPtr, ConfigPtr->BaseAddress);
}


/******************************************************************************/
/**
* Reset the AD7689 IP core
*
* @param	InstancePtr is a pointer to an AD7689_S instance. The memory the
*		pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the driver through the AD7689_S API must be
*		made with this pointer.
*
* @return
*		 void
*
* @note		None.
*
******************************************************************************/
void AD7689_S_Reset(AD7689_S * InstancePtr)
{
	AD7689_S_WriteReg(InstancePtr->BaseAddress, RESET_REG, 1);
	AD7689_S_WriteReg(InstancePtr->BaseAddress, RESET_REG, 0);
}

/******************************************************************************/
/**
* FW-based frequency measurement of AD conversions (per channel measurement (ch0))
*
* @param	InstancePtr is a pointer to an AD7689_S instance. The memory the
*		pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the driver through the AD7689_S API must be
*		made with this pointer.
*
* @return
*		 AD conversion frequency, in Hz
*
* @note		Frequency refresh rate in firmware: 1Hz
*
******************************************************************************/
u16 AD7689_S_Measure_Conv_Freq(AD7689_S *InstancePtr)
{
	return 	 AD7689_S_ReadReg(InstancePtr->BaseAddress, SAMPLE_FREQ_MEAS_REG);
}

/******************************************************************************/
/**
* Set (per channel) AD conversion frequency
*
* @param	InstancePtr is a pointer to an AD7689_S instance. The memory the
*		pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the driver through the AD7689_S API must be
*		made with this pointer.
*
*
* @param	freq	Requested AD conversion frequency (in Hz)
*
* @return
*		 void
*
* @note		None
*
******************************************************************************/
void AD7689_S_Set_Conv_Freq(AD7689_S *InstancePtr, u16 freq)
{
	if (freq == 0) freq = 1;
	if (freq > 30000) freq = 30000;

	//(50MHz internal core clock-> 20ns, 9 -> (8+1) channels, 3 -> FW FSM artifact )
	u32 ch2ch_clk_p = 1000000000/freq/20/9 - 3;

	AD7689_S_WriteReg(InstancePtr->BaseAddress, CH2CH_SAMPLE_CLK_PERIOD_REG, ch2ch_clk_p);
}

/******************************************************************************/
/**
* Read back conversion counter (for diagnostic purposes)
*
* @param	InstancePtr is a pointer to an AD7689_S instance. The memory the
*		pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the driver through the AD7689_S API must be
*		made with this pointer.
*
* @return
*		 Per channel (ch0) conversion counter (32-bit FW based, cnt wraps around)
*
* @note		None
*
******************************************************************************/
u32 AD7689_S_Get_Conv_Cnt(AD7689_S *InstancePtr)
{
	return 	 AD7689_S_ReadReg(InstancePtr->BaseAddress, SAMPLE_CH0_CNT_REG);
}

/******************************************************************************/
/**
* Read last 16-bit raw AD conversion result
*
* @param	InstancePtr is a pointer to an AD7689_S instance. The memory the
*		pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the driver through the AD7689_S API must be
*		made with this pointer.
*
* @param 	ch is requested channel number
* 			valid range: [0-8] where 0-7 is ADC input, 8 is ADC internal temperature
*
* @param 	reading is pointer to requested AD conversion channel result
*
* @return   - XST_INVALID_PARAM if ch parameter out of bound
* 			- XST_SUCCESS otherwise
*
* @note		None
*
******************************************************************************/
XStatus AD7689_S_Get_Reading(AD7689_S *InstancePtr, u8 ch, u16 * reading)
{
	if (ch > 8)
		return XST_INVALID_PARAM;

	*reading = (u16)AD7689_S_ReadReg(InstancePtr->BaseAddress, ADC_CH0_REG + ch * 4);

	return XST_SUCCESS;
}

/******************************************************************************/
/**
* Master enable/disable override mode
*
* @param	InstancePtr is a pointer to an AD7689_S instance. The memory the
*		pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the driver through the AD7689_S API must be
*		made with this pointer.
*
* @param 	enable (if non-zero, master enable override mode, zero globally disables it)
*
* @return   void
*
* @note		None
*
******************************************************************************/
void AD7689_S_Set_Master_Ovrrd_Enable(AD7689_S *InstancePtr, u32 enable)
{
	u32 master_ovvrd_enable_reg = 0;

	if (enable  != 0 )
		master_ovvrd_enable_reg = MASTER_OVVRD_ENABLE_VAL;

	AD7689_S_WriteReg(InstancePtr->BaseAddress, OVRRD_MASTER_EN_REG, master_ovvrd_enable_reg);
}

/******************************************************************************/
/**
* Get override mode setting
*
* @param	InstancePtr is a pointer to an AD7689_S instance. The memory the
*		pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the driver through the AD7689_S API must be
*		made with this pointer.
*
* @return   1 if master override mode enabled, 0 if disabled
*
* @note		None
*
******************************************************************************/
u32 AD7689_S_Get_Master_Ovrrd_Enable(AD7689_S *InstancePtr)
{
	u32 master_ovvrd_enable_reg;
	master_ovvrd_enable_reg = AD7689_S_ReadReg(InstancePtr->BaseAddress, OVRRD_MASTER_EN_REG);

	if (master_ovvrd_enable_reg  != MASTER_OVVRD_ENABLE_VAL )
		return  0;
	else
		return 1;

}

/******************************************************************************/
/**
* Set per channel enable/disable override mask
*
* @param	InstancePtr is a pointer to an AD7689_S instance. The memory the
*		pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the driver through the AD7689_S API must be
*		made with this pointer.
*
* @param 	ovrrd_enables (per channel enable/disable override mask)
*
* @return   void
*
* @note		None
*
******************************************************************************/
void AD7689_S_Set_Ch_Ovrrd_Enables(AD7689_S *InstancePtr, u32 ovrrd_enables)
{
	AD7689_S_WriteReg(InstancePtr->BaseAddress, OVRRD_ENABLES_REG, ovrrd_enables);
}

/******************************************************************************/
/**
* Get per channel enable/disable override mask
*
* @param	InstancePtr is a pointer to an AD7689_S instance. The memory the
*		pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the driver through the AD7689_S API must be
*		made with this pointer.
*
* @return   ovrrd_enables (per channel enable/disable override mask)
*
* @note		None
*
******************************************************************************/
u32 AD7689_S_Get_Ch_Ovrrd_Enables(AD7689_S *InstancePtr)
{
	return AD7689_S_ReadReg(InstancePtr->BaseAddress, OVRRD_ENABLES_REG);
}

/******************************************************************************/
/**
* Set channel override value
*
* @param	InstancePtr is a pointer to an AD7689_S instance. The memory the
*		pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the driver through the AD7689_S API must be
*		made with this pointer.
*
* @param 	ch is requested channel number
* 			valid range: [0-8] where 0-7 is ADC input, 8 is ADC internal temperature
*
* @param 	ovrrd_val is 16-bit ADC channel override value
*
* @return   - XST_INVALID_PARAM if ch parameter out of bound
* 			- XST_SUCCESS otherwise
*
* @note		None
*
******************************************************************************/
XStatus AD7689_S_Set_Ovrrd_Val(AD7689_S *InstancePtr, u8 ch, u16 ovrrd_val)
{
	if (ch > 8)
		return XST_INVALID_PARAM;

	AD7689_S_WriteReg(InstancePtr->BaseAddress, OVRRD_VAL_CH0_REG + ch * 4, ovrrd_val);

	return XST_SUCCESS;
}

/******************************************************************************/
/**
* Get channel override value
*
* @param	InstancePtr is a pointer to an AD7689_S instance. The memory the
*		pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the driver through the AD7689_S API must be
*		made with this pointer.
*
* @param 	ch is requested channel number
* 			valid range: [0-8] where 0-7 is ADC input, 8 is ADC internal temperature
*
* @param 	ovrrd_val is pointer to 16-bit ADC channel override value
*
* @return   - XST_INVALID_PARAM if ch parameter out of bound
* 			- XST_SUCCESS otherwise
*
* @note		None
*
******************************************************************************/
XStatus AD7689_S_Get_Ovrrd_Val(AD7689_S *InstancePtr, u8 ch, u16 * ovrrd_val)
{
	if (ch > 8)
		return XST_INVALID_PARAM;

	*ovrrd_val = (u16)AD7689_S_ReadReg(InstancePtr->BaseAddress, OVRRD_VAL_CH0_REG + ch * 4);

	return XST_SUCCESS;
}
