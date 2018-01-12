

/***************************** Include Files *******************************/
#include "led_controller.h"
#include "xparameters.h"
#include "xil_io.h"

extern LED_Controller_Config LED_Controller_ConfigTable[];

/************************** Function Definitions ***************************/

#define LED_Controller_WriteReg(BaseAddress, RegOffset, Data) \
	Xil_Out32((BaseAddress) + (RegOffset), (u32)(Data))

#define LED_Controller_ReadReg(BaseAddress, RegOffset) \
	Xil_In32((BaseAddress) + (RegOffset))

/******************************************************************************/
/**
* Lookup the device configuration based on the unique device ID.  The table
* ConfigTable contains the configuration info for each device in the system.
*
* @param	DeviceId is the device identifier to lookup.
*
* @return
*		 - A pointer of data type LED_Controller_Config which points to the
*		device configuration if DeviceID is found.
* 		- NULL if DeviceID is not found.
*
* @note		None.
*
******************************************************************************/
LED_Controller_Config *LED_Controller_LookupConfig(u16 DeviceId)
{
	LED_Controller_Config *CfgPtr = NULL;

	int Index;

	for (Index = 0; Index < XPAR_LED_CONTROLLER_NUM_INSTANCES; Index++) {
		if (LED_Controller_ConfigTable[Index].DeviceId == DeviceId) {
			CfgPtr = &LED_Controller_ConfigTable[Index];
			break;
		}
	}

	return CfgPtr;
}


/****************************************************************************/
/**
* Initialize the LED_Controller instance provided by the caller based on the
* given DeviceID.
*
* Nothing is done except to initialize the InstancePtr.
*
* @param	InstancePtr is a pointer to an LED_Controller instance. The memory the
*		pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the instance/driver through the LED_Controller API
*		must be made with this pointer.
* @param	DeviceId is the unique id of the device controlled by this LED_Controller
*		instance. Passing in a device id associates the generic LED_Controller
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
int LED_Controller_Initialize(LED_Controller * InstancePtr, u16 DeviceId)
{
	LED_Controller_Config *ConfigPtr;

	/*
	 * Assert arguments
	 */
	Xil_AssertNonvoid(InstancePtr != NULL);

	/*
	 * Lookup configuration data in the device configuration table.
	 * Use this configuration info down below when initializing this
	 * driver.
	 */
	ConfigPtr = LED_Controller_LookupConfig(DeviceId);
	if (ConfigPtr == (LED_Controller_Config *) NULL) {
		InstancePtr->IsReady = 0;
		return (XST_DEVICE_NOT_FOUND);
	}

	return LED_Controller_CfgInitialize(InstancePtr, ConfigPtr,
				   ConfigPtr->BaseAddress);
}

/****************************************************************************/
/**
* Initialize the LED_Controller instance provided by the caller based on the
* given configuration data.
*
* Nothing is done except to initialize the InstancePtr.
*
* @param	InstancePtr is a pointer to an LED_Controller instance. The memory the
*		pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the driver through the LED_Controller API must be
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
int LED_Controller_CfgInitialize(LED_Controller * InstancePtr, LED_Controller_Config * Config,
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
* Sets a LED to On-Off mode and changes its state.
*
* @param	InstancePtr is a pointer to an LED_Controller instance. The memory the
*		pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the driver through the LED_Controller API must be
*		made with this pointer.
* @param	InterfaceNumber is the target LED interface in the controller. An
*       assertion will take place if the target interface is above the number
*       of available interfaces.
* @param 	turnOn is the On-Off request, use 0 to turn off or any number above 0
*       to turn the LED on.
*
* @note		None.
*
*****************************************************************************/
void LED_Controller_SetOnOff(LED_Controller *InstancePtr, u32 InterfaceNumber, u8 turnOn)
{
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InterfaceNumber < InstancePtr->InterfaceCount);
    
    LED_Controller_WriteReg(InstancePtr->BaseAddress, (LED_CONTROLLER_INTERFACE_OFFSET * InterfaceNumber) * 4 + LED_CONTROLLER_MODE_REG, 0x0); // On-Off mode
    LED_Controller_WriteReg(InstancePtr->BaseAddress, (LED_CONTROLLER_INTERFACE_OFFSET * InterfaceNumber) * 4 + LED_CONTROLLER_FACTOR_REG, (turnOn?0x1:0x0));
}

/****************************************************************************/
/**
* Sets a LED to pulse mode and changes its PWM frequency.
*
* @param	InstancePtr is a pointer to an LED_Controller instance. The memory the
*		pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the driver through the LED_Controller API must be
*		made with this pointer.
* @param	InterfaceNumber is the target LED interface in the controller. An
*       assertion will take place if the target interface is above the number
*       of available interfaces.
* @param 	pwmFactor is the PWM factor based on the AXI clock frequency.
*       Use LED_PULSE_SLOW, LED_PULSE_NORMAL, LED_PULSE_FAST for best results or a
*       set the range between 0 and 255.
*
* @note		None.
*
*****************************************************************************/
void LED_Controller_Pulse(LED_Controller *InstancePtr, u32 InterfaceNumber, u8 pwmFactor)
{
	Xil_AssertVoid(InstancePtr != NULL);
	Xil_AssertVoid(InterfaceNumber < InstancePtr->InterfaceCount);

	LED_Controller_WriteReg(InstancePtr->BaseAddress, (LED_CONTROLLER_INTERFACE_OFFSET * InterfaceNumber) * 4 + LED_CONTROLLER_MODE_REG, 0x1); // Pulse mode
	LED_Controller_WriteReg(InstancePtr->BaseAddress, (LED_CONTROLLER_INTERFACE_OFFSET * InterfaceNumber) * 4 + LED_CONTROLLER_FACTOR_REG, pwmFactor);
}

/****************************************************************************/
/**
* Sets a LED to dim mode and changes its PWM frequency.
*
* @param	InstancePtr is a pointer to an LED_Controller instance. The memory the
*		pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the driver through the LED_Controller API must be
*		made with this pointer.
* @param	InterfaceNumber is the target LED interface in the controller. An
*       assertion will take place if the target interface is above the number
*       of available interfaces.
* @param 	dimFactor is the PWM factor based on the AXI clock frequency.
*       Use LED_DIM_25, LED_DIM_50, LED_DIM_75 for best results or a
*       set the range between 0 and 255.
*
* @note		None.
*
*****************************************************************************/
void LED_Controller_Dim(LED_Controller *InstancePtr, u32 InterfaceNumber, u8 dimFactor)
{
	Xil_AssertVoid(InstancePtr != NULL);
	Xil_AssertVoid(InterfaceNumber < InstancePtr->InterfaceCount);

	LED_Controller_WriteReg(InstancePtr->BaseAddress, (LED_CONTROLLER_INTERFACE_OFFSET * InterfaceNumber) * 4 + LED_CONTROLLER_MODE_REG, 0x2); // Pulse mode
	LED_Controller_WriteReg(InstancePtr->BaseAddress, (LED_CONTROLLER_INTERFACE_OFFSET * InterfaceNumber) * 4 + LED_CONTROLLER_FACTOR_REG, dimFactor);
}
