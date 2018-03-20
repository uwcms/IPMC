#ifndef MGMT_ZONE_CTRL_H
#define MGMT_ZONE_CTRL_H

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

	u32 fault_holdoff; // amount of time (in ms) to ignore fault conditions
			  // immediately after initiating power up sequence

	PWREN_cfg_t pwren_cfg[32];

} MZ_config;

typedef enum MZ_pwr {
	MZ_PWR_ON = 0,
	MZ_PWR_TRANS_ON = 1,
	MZ_PWR_TRANS_OFF = 2,
	MZ_PWR_OFF = 3
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
* @param Management Zone Configuration
* 		 MZ_config members:
* 		 	u64 hardfault_mask;
* 		    u32 fault_holdoff;
* 		    PWREN_cfg_t pwren_cfg[32]; - if individual element of PWREN_cfg_t set to zero
* 		    							 then it's not controlled by this MZ
*
* @note		None.
*
*****************************************************************************/
void Mgmt_Zone_Ctrl_Set_MZ_Cfg(Mgmt_Zone_Ctrl *InstancePtr, u32 MZ, MZ_config cfg);


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

#endif // MGMT_ZONE_CTRL_H
