# LED controller IP

The LED controller IP provides an AXI interface for a controller than can drive up to 16 LEDs. The IP can be configured with system designer where the total number of LED interfaces and global polarity are defined.

The number of available registers will depend on the total number of LED interfaces and each interface will have a register to select the mode (**MODE** register) and another to define the factor (**FACTOR** register).

The use of the C driver is recommended, an example is presented bellow.



## 1. Register mapping

Registers for different interfaces are offset by 0x8.

### 1.1. MODE register (offset 0)

Bits | 31-2 | 1-0
---|---|---
Mode | - | MODE

### 1.2. FACTOR register (offset 4)

Bits | 31-8 | 7-0
---|---|---
Value | - | FACTOR

## 2. Modes of operation
There are a total of 3 modes of operation for each LED that can be configured through the **MOD**E register (offset 0):

Mode | Name | Description
------|------|------------
0 | On/Off | Turn on or off the LED by setting bit zero of **FACTOR** register.
1 | Pulse | A variable PWM drives the LED, making it pulse. The pulsing speed is adjusting by setting the **FACTOR** register.
2 | Dim | A PWM signal is sent to the LED, dimming it. The dimming factor can be adjusted by setting the **FACTOR** register.

### 2.1. Pulse mode

It is recommended to use LED_PULSE_SLOW, LED_PULSE_NORMAL, LED_PULSE_FAST for reasonable pulses.

### 2.2. Dim mode

It is recommended to use LED_DIM_25, LED_DIM_50, LED_DIM_75 for reasonable dim percentages.

## 3. Examples

```c
#include "led_controller.h"

/* Three different demos, 0-On/Off, 1-Pulse, 2-Dim */
int LED_Controller_demo(int demo)
{
	int i, k, Status;

	LED_Controller led_controller_list[XPAR_LED_CONTROLLER_NUM_INSTANCES];

	/* Initialize the LED Controller drivers */
	for (i = 0; i < XPAR_LED_CONTROLLER_NUM_INSTANCES; i++) {
		Status = LED_Controller_Initialize(&(led_controller_list[i]), i);
		if (Status != XST_SUCCESS) {
			xil_printf("LED_Controller Initialization Failed\r\n");
			return XST_FAILURE;
		}
	}

	/* Switch based on the desired demo */
	switch (demo) {
		case 0:
			// Demo 1: Turn ON all LEDs
			for (i = 0; i < XPAR_LED_CONTROLLER_NUM_INSTANCES; i++) {
				for (k = 0; k < led_controller_list[i].InterfaceCount; k++) {
					LED_Controller_SetOnOff(&(led_controller_list[i]), k, 1);
				}
			}
			break;
		case 1:
			// Demo 2: Set all LEDs on normal pulsing mode
			for (i = 0; i < XPAR_LED_CONTROLLER_NUM_INSTANCES; i++) {
				for (k = 0; k < led_controller_list[i].InterfaceCount; k++) {
					LED_Controller_Pulse(&(led_controller_list[i]), k, LED_PULSE_NORMAL);
				}
			}
			break;
		case 2:
			// Demo 3: Set all LEDs at 50% dimming
			for (i = 0; i < XPAR_LED_CONTROLLER_NUM_INSTANCES; i++) {
				for (k = 0; k < led_controller_list[i].InterfaceCount; k++) {
					LED_Controller_Dim(&(led_controller_list[i]), k, LED_DIM_50);
				}
			}
			break;
		default: break;
	}

	return XST_SUCCESS;
}
```

# 4. Revision log

* Revision 0.1:
    - First release, needs to be tested.
* Revision 1.0:
    - Driver name changed from **led** to **led_controller**.
    - C driver and BSP generation scripts added to the IP.
