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

#ifndef IPMI_SENSOR_PROC_H
#define IPMI_SENSOR_PROC_H

#ifdef __cplusplus
extern "C" {
#endif

/****************** Include Files ********************/
#include "xil_types.h"
#include "xstatus.h"

/**************************** Type Definitions *****************************/

/**
 * This typedef contains build time configuration information for the device.
 */
typedef struct {
	u16 DeviceId; /* Unique ID of device */
	UINTPTR BaseAddress; /* Device base address */
	u32 SENSOR_CH_CNT; /* Number of sensors supported in FW*/
	u32 SENSOR_DATA_WIDTH; /* Sensor bit width */
} IPMI_SENSOR_PROC_Config;

/**
 * The IPMI_Sensor_Proc driver instance data. The user is required to allocate
 * a variable of this type for every IPMI_Sensor_Proc device in the system.
 * A pointer to a variable of this type is then passed to the driver API functions.
 **/
typedef struct {
	UINTPTR BaseAddress; /* Device base address */
	u32 IsReady; /* Device is initialized and ready */
	u32 sensor_ch_cnt;  /* Number of sensors supported in FW*/
	u32 sensor_data_width;  /* Sensor bit width */
} IPMI_Sensor_Proc;

/**
 * Threshold Configuration
 **/
typedef struct {
    u16 LNC; /* Raw lower non-critical threshold */
    u16 LCR; /* Raw lower critical threshold. */
    u16 LNR; /* Raw lower non-recoverable threshold. */
    u16 UNC; /* Raw upper non-critical threshold */
    u16 UCR; /* Raw upper critical threshold. */
    u16 UNR; /* Raw upper non-recoverable threshold. */
} Thr_Cfg;

/**
 * Hysteresis Configuration
 **/
typedef struct {
    u16 hyst_pos; /* Raw Positive-going Threshold Hysteresis Value */
    u16 hyst_neg; /* Raw Negative-going Threshold Hysteresis Value */
} Hyst_Cfg;

/****************************************************************************/
/**
* Initialize the IPMI_Sensor_Proc instance provided by the caller based on
* the given DeviceID.
*
* Nothing is done except to initialize the InstancePtr.
*
* @param	InstancePtr is a pointer to an IPMI_Sensor_Proc instance.
* 		The memory the pointer references must be pre-allocated by the
* 		caller. Further calls to manipulate the instance/driver through
* 		the IPMI_Sensor_Proc API must be made with this pointer.
* @param	DeviceId is the unique id of the device controlled by this
* 		IPMI_Sensor_Proc instance. Passing in a device id associates
* 		the generic IPMI_Sensor_Proc instance to a specific device,
* 		as chosen by the caller or application developer.
*
* @return
*		- XST_SUCCESS if the initialization was successful.
* 		- XST_DEVICE_NOT_FOUND  if the device configuration data was not
*		found for a device with the supplied device ID.
*
* @note		None.
*
*****************************************************************************/
XStatus IPMI_Sensor_Proc_Initialize(IPMI_Sensor_Proc *InstancePtr, u16 DeviceId);


/****************************************************************************/
/**
* Reset IPMI Sensor Processor core
*
* @param	InstancePtr is a pointer to an IPMI_Sensor_Proc instance.
* 		The memory the pointer references must be pre-allocated by the
* 		caller. Further calls to manipulate the instance/driver through
* 		the IPMI_Sensor_Proc API must be made with this pointer.
*
* @note		None.
*
*****************************************************************************/
void IPMI_Sensor_Proc_Reset(IPMI_Sensor_Proc *InstancePtr);


/****************************************************************************/
/**
* Configure Sensor Hysteresis values
*
* The function roughly corresponds to section "35.6 Set Sensor Hysteresis Command"
* from IPMI Interface Spec v2.0 document. It is up to the higher level SW stack to
* translate raw sensor values to 8-bit IPMI values
*
* @param	InstancePtr is a pointer to an IPMI_Sensor_Proc instance. The memory
* 		the pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the instance/driver through the Mgmt_Zone_Ctrl API
*		must be made with this pointer.
*
* @param 	Sensor channel number
*
* @param 	Pointer to Hysteresis Configuration
*
*
* @return   - XST_INVALID_PARAM if ch parameter out of bound
* 			- XST_SUCCESS otherwise
*
* @note		None.
*
*****************************************************************************/
XStatus IPMI_Sensor_Proc_Set_Hyst(const IPMI_Sensor_Proc *InstancePtr, const u32 ch, const Hyst_Cfg * cfg);

/****************************************************************************/
/**
* Read back Sensor Hysteresis values
*
* The function roughly corresponds to section "35.7 Get Sensor Hysteresis Command"
* from IPMI Interface Spec v2.0 document. It is up to the higher level SW stack to
* translate raw sensor values to 8-bit IPMI values
*
* @param	InstancePtr is a pointer to an IPMI_Sensor_Proc instance. The memory
* 		the pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the instance/driver through the Mgmt_Zone_Ctrl API
*		must be made with this pointer.
*
* @param 	Sensor channel number
*
* @param 	Pointer to Hysteresis Configuration
*
* @return   - XST_INVALID_PARAM if ch parameter out of bound
* 			- XST_SUCCESS otherwise
*
* @note		None.
*
*****************************************************************************/
XStatus IPMI_Sensor_Proc_Get_Hyst(const IPMI_Sensor_Proc *InstancePtr, const u32 ch, Hyst_Cfg * cfg);

/****************************************************************************/
/**
* Configure Sensor Threshold values
*
* The function roughly corresponds to section "35.8 Set Sensor Threshold Command"
* from IPMI Interface Spec v2.0 document. It is up to the higher level SW stack to
* translate raw sensor values to 8-bit IPMI values
*
* @param	InstancePtr is a pointer to an IPMI_Sensor_Proc instance. The memory
* 		the pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the instance/driver through the Mgmt_Zone_Ctrl API
*		must be made with this pointer.
*
* @param 	Sensor channel number
*
* @param 	Pointer to Threshold Configuration
*
*
* @return   - XST_INVALID_PARAM if ch parameter out of bound
* 			- XST_SUCCESS otherwise
*
* @note		None.
*
*****************************************************************************/
XStatus IPMI_Sensor_Proc_Set_Thr(const IPMI_Sensor_Proc *InstancePtr, const u32 ch, const Thr_Cfg * cfg);

/****************************************************************************/
/**
* Read back Sensor Threshold values
*
* The function roughly corresponds to section "35.9 Get Sensor Threshold Command"
* from IPMI Interface Spec v2.0 document. It is up to the higher level SW stack to
* translate raw sensor values to 8-bit IPMI values
*
* @param	InstancePtr is a pointer to an IPMI_Sensor_Proc instance. The memory
* 		the pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the instance/driver through the Mgmt_Zone_Ctrl API
*		must be made with this pointer.
*
* @param 	Sensor channel number
*
* @param 	Pointer to Threshold Configuration
*
*
* @return   - XST_INVALID_PARAM if ch parameter out of bound
* 			- XST_SUCCESS otherwise
*
* @note		None.
*
*****************************************************************************/
XStatus IPMI_Sensor_Proc_Get_Thr(const IPMI_Sensor_Proc *InstancePtr, const u32 ch, Thr_Cfg * cfg);

/****************************************************************************/
/**
* Configure Sensor Event Enable
*
* The function roughly corresponds to section "35.10 Set Sensor Event Enable Command"
* from IPMI Interface Spec v2.0 document.
* *
* @param	InstancePtr is a pointer to an IPMI_Sensor_Proc instance. The memory
* 		the pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the instance/driver through the Mgmt_Zone_Ctrl API
*		must be made with this pointer.
*
* @param 	Sensor channel number
*
* @param 	Sensor Event Assert Enable bitfield definition
* 		bit 11: select assertion event for upper non-recoverable going high
* 		bit 10: select assertion event for upper non-recoverable going low
* 		bit  9: select assertion event for upper critical going high
* 		bit  8: select assertion event for upper critical going low
* 		bit  7: select assertion event for upper non-critical going high
* 		bit  6: select assertion event for upper non-critical going low
* 		bit  5: select assertion event for lower non-recoverable going high
* 		bit  4: select assertion event for lower non-recoverable going low
* 		bit  3: select assertion event for lower critical going high
* 		bit  2: select assertion event for lower critical going low
* 		bit  1: select assertion event for lower non-critical going high
* 		bit  0: select assertion event for lower non-critical going low
*
* @param 	Sensor Event Deassert Enable bitfield definition
* 		bit 11: select deassertion event for upper non-recoverable going high
* 		bit 10: select deassertion event for upper non-recoverable going low
* 		bit  9: select deassertion event for upper critical going high
* 		bit  8: select deassertion event for upper critical going low
* 		bit  7: select deassertion event for upper non-critical going high
* 		bit  6: select deassertion event for upper non-critical going low
* 		bit  5: select deassertion event for lower non-recoverable going high
* 		bit  4: select deassertion event for lower non-recoverable going low
* 		bit  3: select deassertion event for lower critical going high
* 		bit  2: select deassertion event for lower critical going low
* 		bit  1: select deassertion event for lower non-critical going high
* 		bit  0: select deassertion event for lower non-critical going low
*
* @return   - XST_INVALID_PARAM if ch parameter out of bound
* 			- XST_SUCCESS otherwise
*
* @note		None.
*
*****************************************************************************/
XStatus IPMI_Sensor_Proc_Set_Event_Enable(const IPMI_Sensor_Proc *InstancePtr,
		const u32 ch, const u16 assert_en, const u16 deassert_en );

/****************************************************************************/
/**
* Read back Sensor Event Enable
*
* The function roughly corresponds to section "35.11 Get Sensor Event Enable Command"
* from IPMI Interface Spec v2.0 document.
* *
* @param	InstancePtr is a pointer to an IPMI_Sensor_Proc instance. The memory
* 		the pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the instance/driver through the Mgmt_Zone_Ctrl API
*		must be made with this pointer.
*
* @param 	Sensor channel number
*
* @param 	Sensor Event Assert Enable bitfield definition
* 		bit 11: select assertion event for upper non-recoverable going high
* 		bit 10: select assertion event for upper non-recoverable going low
* 		bit  9: select assertion event for upper critical going high
* 		bit  8: select assertion event for upper critical going low
* 		bit  7: select assertion event for upper non-critical going high
* 		bit  6: select assertion event for upper non-critical going low
* 		bit  5: select assertion event for lower non-recoverable going high
* 		bit  4: select assertion event for lower non-recoverable going low
* 		bit  3: select assertion event for lower critical going high
* 		bit  2: select assertion event for lower critical going low
* 		bit  1: select assertion event for lower non-critical going high
* 		bit  0: select assertion event for lower non-critical going low
*
* @param 	Sensor Event Deassert Enable bitfield definition
* 		bit 11: select deassertion event for upper non-recoverable going high
* 		bit 10: select deassertion event for upper non-recoverable going low
* 		bit  9: select deassertion event for upper critical going high
* 		bit  8: select deassertion event for upper critical going low
* 		bit  7: select deassertion event for upper non-critical going high
* 		bit  6: select deassertion event for upper non-critical going low
* 		bit  5: select deassertion event for lower non-recoverable going high
* 		bit  4: select deassertion event for lower non-recoverable going low
* 		bit  3: select deassertion event for lower critical going high
* 		bit  2: select deassertion event for lower critical going low
* 		bit  1: select deassertion event for lower non-critical going high
* 		bit  0: select deassertion event for lower non-critical going low
*
* @return   - XST_INVALID_PARAM if ch parameter out of bound
* 			- XST_SUCCESS otherwise
*
* @note		None.
*
*****************************************************************************/
XStatus IPMI_Sensor_Proc_Get_Event_Enable(const IPMI_Sensor_Proc *InstancePtr,
		const u32 ch, u16 * assert_en, u16 * deassert_en );


/****************************************************************************/
/**
* Re-arm Sensor Event Enable
*
* The function roughly corresponds to section "35.12 Re-arm Sensor Events Command"
* from IPMI Interface Spec v2.0 document.
* *
* @param	InstancePtr is a pointer to an IPMI_Sensor_Proc instance. The memory
* 		the pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the instance/driver through the Mgmt_Zone_Ctrl API
*		must be made with this pointer.
*
* @param 	Sensor channel number
*
* @param 	Sensor Event Assert Enable bitfield definition
* 		bit 11: re-arm assertion event for upper non-recoverable going high
* 		bit 10: re-arm assertion event for upper non-recoverable going low
* 		bit  9: re-arm assertion event for upper critical going high
* 		bit  8: re-arm assertion event for upper critical going low
* 		bit  7: re-arm assertion event for upper non-critical going high
* 		bit  6: re-arm assertion event for upper non-critical going low
* 		bit  5: re-arm assertion event for lower non-recoverable going high
a* 		bit  4: re-arm assertion event for lower non-recoverable going low
* 		bit  3: re-arm assertion event for lower critical going high
* 		bit  2: re-arm assertion event for lower critical going low
* 		bit  1: re-arm assertion event for lower non-critical going high
* 		bit  0: re-arm assertion event for lower non-critical going low
*
* @param 	Sensor Event Deassert Enable bitfield definition
* 		bit 11: re-arm deassertion event for upper non-recoverable going high
* 		bit 10: re-arm deassertion event for upper non-recoverable going low
* 		bit  9: re-arm deassertion event for upper critical going high
* 		bit  8: re-arm deassertion event for upper critical going low
* 		bit  7: re-arm deassertion event for upper non-critical going high
* 		bit  6: re-arm deassertion event for upper non-critical going low
* 		bit  5: re-arm deassertion event for lower non-recoverable going high
* 		bit  4: re-arm deassertion event for lower non-recoverable going low
* 		bit  3: re-arm deassertion event for lower critical going high
* 		bit  2: re-arm deassertion event for lower critical going low
* 		bit  1: re-arm deassertion event for lower non-critical going high
* 		bit  0: re-arm deassertion event for lower non-critical going low
*
*
* @return   - XST_INVALID_PARAM if ch parameter out of bound
* 			- XST_SUCCESS otherwise
*
* @note		Threshold based sensor, with manual re-arm.
*
*****************************************************************************/
XStatus IPMI_Sensor_Proc_Rearm_Event_Enable(const IPMI_Sensor_Proc *InstancePtr,
		const u32 ch, const u16 assert_rearm, const u16 deassert_rearm );

/****************************************************************************/
/**
* Read back Latched Sensor Event Status
*
* The function roughly corresponds to section "35.13 Get Sensor Event Status Command"
* from IPMI Interface Spec v2.0 document.
* *
* @param	InstancePtr is a pointer to an IPMI_Sensor_Proc instance. The memory
* 		the pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the instance/driver through the Mgmt_Zone_Ctrl API
*		must be made with this pointer.
*
* @param 	Sensor channel number
*
* @param 	Sensor Event Assert Status bitfield definition
* 		bit 11: 1b = assertion event condition for upper non-recoverable going high occurred
* 		bit 10: 1b = assertion event condition for upper non-recoverable going low occurred
* 		bit  9: 1b = assertion event condition for upper critical going high occurred
* 		bit  8: 1b = assertion event condition for upper critical going low occurred
* 		bit  7: 1b = assertion event condition for upper non-critical going high occurred
* 		bit  6: 1b = assertion event condition for upper non-critical going low occurred
* 		bit  5: 1b = assertion event condition for lower non-recoverable going high occurred
* 		bit  4: 1b = assertion event condition for lower non-recoverable going low occurred
* 		bit  3: 1b = assertion event condition for lower critical going high occurred
* 		bit  2: 1b = assertion event condition for lower critical going low occurred
* 		bit  1: 1b = assertion event condition for lower non-critical going high occurred
* 		bit  0: 1b = assertion event condition for lower non-critical going low occurred
*
* @param 	Sensor Event Deassert Status bitfield definition
* 		bit 11: 1b = deassertion event condition for upper non-recoverable going high occurred
* 		bit 10: 1b = deassertion event condition for upper non-recoverable going low occurred
* 		bit  9: 1b = deassertion event condition for upper critical going high occurred
* 		bit  8: 1b = deassertion event condition for upper critical going low occurred
* 		bit  7: 1b = deassertion event condition for upper non-critical going high occurred
* 		bit  6: 1b = deassertion event condition for upper non-critical going low occurred
* 		bit  5: 1b = deassertion event condition for lower non-recoverable going high occurred
* 		bit  4: 1b = deassertion event condition for lower non-recoverable going low occurred
* 		bit  3: 1b = deassertion event condition for lower critical going high occurred
* 		bit  2: 1b = deassertion event condition for lower critical going low occurred
* 		bit  1: 1b = deassertion event condition for lower non-critical going high occurred
* 		bit  0: 1b = deassertion event condition for lower non-critical going low occurred
*
* @return   - XST_INVALID_PARAM if ch parameter out of bound
* 			- XST_SUCCESS otherwise
*
* @note		Threshold based sensor, with manual re-arm.
*
*****************************************************************************/
XStatus IPMI_Sensor_Proc_Get_Latched_Event_Status(const IPMI_Sensor_Proc *InstancePtr,
		const u32 ch, u16 * assert_status, u16 * deassert_status );

/****************************************************************************/
/**
* Read back Current Sensor Event Status
*
* *
* @param	InstancePtr is a pointer to an IPMI_Sensor_Proc instance. The memory
* 		the pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the instance/driver through the Mgmt_Zone_Ctrl API
*		must be made with this pointer.
*
* @param 	Sensor channel number
*
* @param 	Sensor Event Assert Status bitfield definition
*		Same as in IPMI_Sensor_Proc_Get_Latched_Event_Status
*
* @param 	Sensor Event Deassert Status bitfield definition
*		Same as in IPMI_Sensor_Proc_Get_Latched_Event_Status
*
* @return   - XST_INVALID_PARAM if ch parameter out of bound
* 			- XST_SUCCESS otherwise
*
* @note		Threshold based sensor, with manual re-arm.
*
*****************************************************************************/
XStatus IPMI_Sensor_Proc_Get_Current_Event_Status(const IPMI_Sensor_Proc *InstancePtr,
		const u32 ch, u16 * assert_status, u16 * deassert_status );


/****************************************************************************/
/**
* Read back Sensor Reading
*
* The function roughly corresponds to section "35.14 Get Sensor Reading	Command"
* from IPMI Interface Spec v2.0 document.
* *
* @param	InstancePtr is a pointer to an IPMI_Sensor_Proc instance. The memory
* 		the pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the instance/driver through the Mgmt_Zone_Ctrl API
*		must be made with this pointer.
*
* @param 	Sensor channel number
*
* @param        Raw Sensor Reading (16-bit)
*
* @param 	Sensor Event Threshold comparison bitfield definition
* 		bit 5: 1b = at or above >= upper non-recoverable threshold
* 		bit 4: 1b = at or above >= upper critical threshold
* 		bit 3: 1b = at or above >= upper non-critical threshold
* 		bit 2: 1b = at or below <= lower non-recoverable threshold
* 		bit 1: 1b = at or below <= lower critical threshold
* 		bit 0: 1b = at or below <= lower non-critical threshold
*
* @return   - XST_INVALID_PARAM if ch parameter out of bound
* 			- XST_SUCCESS otherwise
*
* @note		Threshold based sensor, with manual re-arm.
*
*****************************************************************************/
XStatus IPMI_Sensor_Proc_Get_Sensor_Reading(const IPMI_Sensor_Proc *InstancePtr,
		const u32 ch, u16 * sensor_reading, u8 * thr_status );

/****************************************************************************/
/**
* Read pending IRQ status
*
* @return   irq status
*
* @note
*
*****************************************************************************/
u32 IPMI_Sensor_Proc_Get_IRQ_Status(const IPMI_Sensor_Proc *InstancePtr);

/****************************************************************************/
/**
* Acknowledge pending IRQ
*
* @note
*
*****************************************************************************/
void IPMI_Sensor_Proc_Ack_IRQ(const IPMI_Sensor_Proc *InstancePtr, u32 irq_ack);

#ifdef __cplusplus
}
#endif

#endif // IPMI_SENSOR_PROC_H
