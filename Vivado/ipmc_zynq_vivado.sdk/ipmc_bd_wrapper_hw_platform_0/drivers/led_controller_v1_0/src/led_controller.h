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

#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#ifdef __cplusplus
extern "C" {
#endif

/****************** Include Files ********************/
#include "xil_types.h"
#include "xstatus.h"

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

/**
 * Registers
 */
#define LED_CONTROLLER_INTERFACE_OFFSET 2
#define LED_CONTROLLER_PERIOD_REG 0x0
#define LED_CONTROLLER_COMP_REG 0x4

/*
 * API Basic functions implemented in led_controller.c
 */
int LED_Controller_Initialize(LED_Controller *InstancePtr, u16 DeviceId);
LED_Controller_Config *LED_Controller_LookupConfig(u16 DeviceId);

int LED_Controller_CfgInitialize(LED_Controller *InstancePtr, LED_Controller_Config * Config, UINTPTR EffectiveAddr);
void LED_Controller_Set(LED_Controller *InstancePtr, u32 InterfaceNumber, u8 EnablePWM, u32 PeriodInClockTicks, u32 TransitionInClockTicks);

#ifdef __cplusplus
}
#endif

#endif // LED_CONTROLLER_H
