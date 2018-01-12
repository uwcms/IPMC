
#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H


/****************** Include Files ********************/
#include "xil_types.h"
#include "xstatus.h"


#define LED_CONTROLLER_INTERFACE_OFFSET 2
#define LED_CONTROLLER_MODE_REG 0x0
#define LED_CONTROLLER_FACTOR_REG 0x4

/**************************** Type Definitions *****************************/

/**
 * This typedef contains configuration information for the device.
 */
typedef struct {
    u16 DeviceId; /* Unique ID of device */
    UINTPTR BaseAddress; /* Device base address */
    u32 InterfaceCount; /* Number of available interfaces */
} LED_Controller_Config;

/**
 * The LED_Controller driver instance data. The user is required to allocate a
 * variable of this type for every LED_Controller device in the system. A pointer
 * to a variable of this type is then passed to the driver API functions.
 **/
typedef struct {
    UINTPTR BaseAddress;    /* Device base address */
    u32 IsReady;        /* Device is initialized and ready */
    u32 InterfaceCount; /* Number of available interfaces */
} LED_Controller;

#define LED_PULSE_SLOW 0x02
#define LED_PULSE_NORMAL 0x10
#define LED_PULSE_FAST 0x20

#define LED_DIM_25 (u8)(255/4)
#define LED_DIM_50 (u8)(255/2)
#define LED_DIM_75 (u8)(255*3/4)

/*
 * API Basic functions implemented in axi_led_controller.c
 */
int LED_Controller_Initialize(LED_Controller *InstancePtr, u16 DeviceId);
LED_Controller_Config *LED_Controller_LookupConfig(u16 DeviceId);

int LED_Controller_CfgInitialize(LED_Controller *InstancePtr, LED_Controller_Config * Config, UINTPTR EffectiveAddr);
void LED_Controller_SetOnOff(LED_Controller *InstancePtr, u32 InterfaceNumber, u8 turnOn);
void LED_Controller_Pulse(LED_Controller *InstancePtr, u32 InterfaceNumber, u8 pwmFactor);
void LED_Controller_Dim(LED_Controller *InstancePtr, u32 InterfaceNumber, u8 dimFactor);




#endif // LED_CONTROLLER_H
