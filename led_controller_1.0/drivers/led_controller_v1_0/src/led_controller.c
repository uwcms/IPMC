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
* Configure a specific LED interface.
*
* @param	InstancePtr is a pointer to an LED_Controller instance. The memory the
*		pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the driver through the LED_Controller API must be
*		made with this pointer.
* @param	InterfaceNumber The target LED interface.
* @param 	PeriodInClockTicks The period of the LED cycle in clock ticks.
* @param    TransitionInClockTicks Time in clock ticks in relation to the period
*       to wait to turn on the LED.
*
* @note		None.
*
*****************************************************************************/
void LED_Controller_Set(LED_Controller *InstancePtr, u32 InterfaceNumber, u8 EnablePWM, u32 PeriodInClockTicks, u32 TransitionInClockTicks) {
	Xil_AssertVoid(InstancePtr != NULL);
	Xil_AssertVoid(InterfaceNumber < InstancePtr->InterfaceCount);
	Xil_AssertVoid(!(PeriodInClockTicks & 0xF0000000));
	Xil_AssertVoid(!(TransitionInClockTicks & 0xF0000000));

	LED_Controller_WriteReg(InstancePtr->BaseAddress, (LED_CONTROLLER_INTERFACE_OFFSET * InterfaceNumber) * 4 + LED_CONTROLLER_PERIOD_REG, (EnablePWM?(1<32):0) | PeriodInClockTicks);
	LED_Controller_WriteReg(InstancePtr->BaseAddress, (LED_CONTROLLER_INTERFACE_OFFSET * InterfaceNumber) * 4 + LED_CONTROLLER_COMP_REG, TransitionInClockTicks);
}

