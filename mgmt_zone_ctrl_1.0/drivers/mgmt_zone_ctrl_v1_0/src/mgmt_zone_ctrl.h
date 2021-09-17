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

#ifndef MGMT_ZONE_CTRL_H
#define MGMT_ZONE_CTRL_H

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
	u32 MZ_CNT; /* Number of MZs supported in FW*/
	u32 HF_CNT; /* Number of hard faults supported in FW */
	u32 PWREN_CNT; /* Number of power enable pins supported in FW*/
} Mgmt_Zone_Ctrl_Config;

/**
 * The Mgmt_Zone_Ctrl driver instance data. The user is required to allocate a
 * variable of this type for every Mgmt_Zone_Ctrl device in the system. A pointer
 * to a variable of this type is then passed to the driver API functions.
 **/
typedef struct {
	UINTPTR BaseAddress; /* Device base address */
	u32 IsReady; /* Device is initialized and ready */
	u32 mz_cnt;  /* Number of MZs supported in FW*/
	u32 hf_cnt;  /* Number of hard faults supported in FW */
	u32 pwren_cnt; /* Number of power enable pins supported in FW*/
} Mgmt_Zone_Ctrl;

/**
 * Power_Enable Config
 *
 * Bit [15:0] up/down timer cfg (in miliseconds)
 * Bit   [16] active_level; set to 0 for active low outputs, set to 1 for active high outputs
 * Bit   [17] drive_enable; set to 0 to tri-state the output, set to 1 to enable the outputs
 **/
typedef u32 PWREN_cfg_t;

/**
 * MZ Config
 **/
typedef struct {

	u64 hardfault_mask; // hard fault enable mask

	u16 fault_holdoff; // amount of time (in ms) to ignore fault conditions
			  // immediately after initiating power up sequence

	PWREN_cfg_t pwren_cfg[32];

} MZ_config;

typedef enum MZ_pwr {
	MZ_PWR_OFF = 0,
	MZ_PWR_TRANS_OFF = 1,
	MZ_PWR_TRANS_ON = 2,
	MZ_PWR_ON = 3
} MZ_pwr;

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
int Mgmt_Zone_Ctrl_Initialize(Mgmt_Zone_Ctrl *InstancePtr, u16 DeviceId);


/****************************************************************************/
/**
* Read back current hard fault status (input to all MZs)
*
* @param	InstancePtr is a pointer to an Mgmt_Zone_Ctrl instance. The memory the
*		pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the instance/driver through the Mgmt_Zone_Ctrl API
*		must be made with this pointer.
*
* @return	64-bit hard fault vector
*
*
* @note		None.
*
*****************************************************************************/
u64 Mgmt_Zone_Ctrl_Get_Hard_Fault_Status(Mgmt_Zone_Ctrl *InstancePtr);


/****************************************************************************/
/**
* Configure Management Zone
*
* @param	InstancePtr is a pointer to an Mgmt_Zone_Ctrl instance. The memory the
*		pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the instance/driver through the Mgmt_Zone_Ctrl API
*		must be made with this pointer.
*
* @param Management Zone select
*
* @param Pointer to Management Zone Configuration
* 		 MZ_config members:
* 		 	u64 hardfault_mask;
* 		    u32 fault_holdoff;
* 		    PWREN_cfg_t pwren_cfg[32]; - if individual element of PWREN_cfg_t set to zero
* 		    							 then it's not controlled by this MZ
*
* @note		None.
*
*****************************************************************************/
void Mgmt_Zone_Ctrl_Set_MZ_Cfg(Mgmt_Zone_Ctrl *InstancePtr, u32 MZ, const MZ_config * cfg);


/****************************************************************************/
/**
* Read back Management Zone configuration
*
* @param	InstancePtr is a pointer to an Mgmt_Zone_Ctrl instance. The memory the
*		pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the instance/driver through the Mgmt_Zone_Ctrl API
*		must be made with this pointer.
*
* @param Management Zone select
*
* @param Management Zone Configuration
*
* @note		None.
*
*****************************************************************************/
void Mgmt_Zone_Ctrl_Get_MZ_Cfg(Mgmt_Zone_Ctrl *InstancePtr, u32 MZ, MZ_config * cfg);


/****************************************************************************/
/**
* Read back Management Zone status (power state as defined with enum MZ_pwr)
*
* @param	InstancePtr is a pointer to an Mgmt_Zone_Ctrl instance. The memory the
*		pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the instance/driver through the Mgmt_Zone_Ctrl API
*		must be made with this pointer.
*
* @param Management Zone select
*
* @return enum MZ_pwr
*
* @note		None.
*
*****************************************************************************/
MZ_pwr Mgmt_Zone_Ctrl_Get_MZ_Status(Mgmt_Zone_Ctrl *InstancePtr, u32 MZ);

/****************************************************************************/
/**
* Read back Power Enable Logical State vector
*
* @param	InstancePtr is a pointer to an Mgmt_Zone_Ctrl instance. The memory the
*		pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the instance/driver through the Mgmt_Zone_Ctrl API
*		must be made with this pointer.
**
* @return MZ-independent, aggregate status vector of power enable logical states
*
* @note		None.
*
*****************************************************************************/
u32 Mgmt_Zone_Ctrl_Get_Pwr_En_Status(Mgmt_Zone_Ctrl *InstancePtr);


/****************************************************************************/
/**
* Start Power-On Sequence on the selected MZ
*
* @param	InstancePtr is a pointer to an Mgmt_Zone_Ctrl instance. The memory the
*		pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the instance/driver through the Mgmt_Zone_Ctrl API
*		must be made with this pointer.
*
* @param Management Zone select
**
* @note		None.
*
*****************************************************************************/
void Mgmt_Zone_Ctrl_Pwr_ON_Seq(Mgmt_Zone_Ctrl *InstancePtr, u32 MZ);


/****************************************************************************/
/**
* Start Power-Off Sequence on the selected MZ
*
* @param	InstancePtr is a pointer to an Mgmt_Zone_Ctrl instance. The memory the
*		pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the instance/driver through the Mgmt_Zone_Ctrl API
*		must be made with this pointer.
*
* @param Management Zone select
**
* @note		None.
*
*****************************************************************************/
void Mgmt_Zone_Ctrl_Pwr_OFF_Seq(Mgmt_Zone_Ctrl *InstancePtr, u32 MZ);


/****************************************************************************/
/**
* Dispatch a Soft Fault event to the selected MZ
*
* @param	InstancePtr is a pointer to an Mgmt_Zone_Ctrl instance. The memory the
*		pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the instance/driver through the Mgmt_Zone_Ctrl API
*		must be made with this pointer.
*
* @param Management Zone select
**
* @note		None.
*
*****************************************************************************/
void Mgmt_Zone_Ctrl_Dispatch_Soft_Fault(Mgmt_Zone_Ctrl *InstancePtr, u32 MZ);


/****************************************************************************/
/**
* Set IRQ enable vector (32 bit mask for MZs)
*
* @param	InstancePtr is a pointer to an Mgmt_Zone_Ctrl instance. The memory the
*		pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the instance/driver through the Mgmt_Zone_Ctrl API
*		must be made with this pointer.
*
* @param IRQ enable vector
**
* @note		None.
*
*****************************************************************************/
void Mgmt_Zone_Ctrl_Set_IRQ_Enables(Mgmt_Zone_Ctrl *InstancePtr, u32 irq_enables);


/****************************************************************************/
/**
* Read back IRQ enable vector (32 bit mask for MZs)
*
* @param	InstancePtr is a pointer to an Mgmt_Zone_Ctrl instance. The memory the
*		pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the instance/driver through the Mgmt_Zone_Ctrl API
*		must be made with this pointer.
*
* @param IRQ enable vector
**
* @note		None.
*
*****************************************************************************/
u32 Mgmt_Zone_Ctrl_Get_IRQ_Enables(Mgmt_Zone_Ctrl *InstancePtr);



/****************************************************************************/
/**
* Acknowledge IRQ  (32 bit mask for MZs)
*
* @param	InstancePtr is a pointer to an Mgmt_Zone_Ctrl instance. The memory the
*		pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the instance/driver through the Mgmt_Zone_Ctrl API
*		must be made with this pointer.
*
* @param IRQ acknowledge vector
**
* @note		None.
*
*****************************************************************************/
void Mgmt_Zone_Ctrl_Ack_IRQ(Mgmt_Zone_Ctrl *InstancePtr, u32 irq_acks);


/****************************************************************************/
/**
* Get active IRQ status vector (cleared by Mgmt_Zone_Ctrl_Ack_IRQ)
*
* @param	InstancePtr is a pointer to an Mgmt_Zone_Ctrl instance. The memory the
*		pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the instance/driver through the Mgmt_Zone_Ctrl API
*		must be made with this pointer.
*
* @note		None.
*
*****************************************************************************/
u32 Mgmt_Zone_Ctrl_Get_IRQ_Status(Mgmt_Zone_Ctrl *InstancePtr);

/****************************************************************************/
/**
* Enable or disable override of the enable lines, useful for bring up or testing
*
* @param	InstancePtr is a pointer to an Mgmt_Zone_Ctrl instance. The memory the
*		pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the instance/driver through the Mgmt_Zone_Ctrl API
*		must be made with this pointer.
*
* @param Enable selection, LSB matches enable 0
*
* @note		None.
*
*****************************************************************************/
void Mgmt_Zone_Ctrl_Set_Enable_Override(Mgmt_Zone_Ctrl *InstancePtr, u32 enables);
u32 Mgmt_Zone_Ctrl_Get_Enable_Override(Mgmt_Zone_Ctrl *InstancePtr);

/****************************************************************************/
/**
* Set the override drive (en/tri) of the enable lines, useful for bring up or testing
*
* @param	InstancePtr is a pointer to an Mgmt_Zone_Ctrl instance. The memory the
*		pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the instance/driver through the Mgmt_Zone_Ctrl API
*		must be made with this pointer.
*
* @param Drive state selection, LSB matches enable 0
*
* @note		None.
*
*****************************************************************************/
void Mgmt_Zone_Ctrl_Set_Override_Drive(Mgmt_Zone_Ctrl *InstancePtr, u32 drive);
u32 Mgmt_Zone_Ctrl_Get_Override_Drive(Mgmt_Zone_Ctrl *InstancePtr);

/****************************************************************************/
/**
* Set the override drive level of the enable lines, useful for bring up or testing
*
* @param	InstancePtr is a pointer to an Mgmt_Zone_Ctrl instance. The memory the
*		pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the instance/driver through the Mgmt_Zone_Ctrl API
*		must be made with this pointer.
*
* @param Drive level selection, LSB matches enable 0
*
* @note		None.
*
*****************************************************************************/
void Mgmt_Zone_Ctrl_Set_Override_Level(Mgmt_Zone_Ctrl *InstancePtr, u32 level);
u32 Mgmt_Zone_Ctrl_Get_Override_Level(Mgmt_Zone_Ctrl *InstancePtr);

/****************************************************************************/
/**
* Get the override input on the pins if the driver is off.
*
* @param	InstancePtr is a pointer to an Mgmt_Zone_Ctrl instance. The memory the
*		pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the instance/driver through the Mgmt_Zone_Ctrl API
*		must be made with this pointer.
*
* @note		None.
*
*****************************************************************************/
u32 Mgmt_Zone_Ctrl_Get_Override_Input(Mgmt_Zone_Ctrl *InstancePtr);

/****************************************************************************/
/**
* Set or Get the maximum enable sequence timer value used across all power
* enables.
*
* This must be greater than or equal to the highest sequence timer value used by
* any power enable configured within this MZ controller.
*
* When a MZ is asked to sequence off, the sequence timer counts down from this
* value, and each power enable pin is deasserted when its "delay" value is
* reached.  This value can therefore also be higher, in order to provide an
* additional delay if desired.
*
* @param	InstancePtr is a pointer to an Mgmt_Zone_Ctrl instance. The memory the
*		pointer references must be pre-allocated by the caller. Further
*		calls to manipulate the instance/driver through the Mgmt_Zone_Ctrl API
*		must be made with this pointer.
*
* @param ms The highest sequence timer value, in milliseconds
*
* @note		None.
*
*****************************************************************************/
void Mgmt_Zone_Ctrl_Set_Sequence_Timer_Max(Mgmt_Zone_Ctrl *InstancePtr, u32 ms);
u32 Mgmt_Zone_Ctrl_Get_Sequence_Timer_Max(Mgmt_Zone_Ctrl *InstancePtr);

#ifdef __cplusplus
}
#endif

#endif // MGMT_ZONE_CTRL_H
