/******************************************************************************
*
* Copyright (C) 2009 - 2014 Xilinx, Inc.  All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* Use of the Software is limited solely to applications:
* (a) running on a Xilinx device, or
* (b) that interact with a Xilinx device through a bus or interconnect.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* XILINX  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
* OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
* Except as contained in this notice, the name of the Xilinx shall not be used
* in advertising or otherwise to promote the sale, use or other dealings in
* this Software without prior written authorization from Xilinx.
*
******************************************************************************/

/*
 * helloworld.c: simple test application
 *
 * This application configures UART 16550 to baud rate 9600.
 * PS7 UART (Zynq) is not initialized by this application, since
 * bootrom/bsp configures it to baud rate 115200
 *
 * ------------------------------------------------
 * | UART TYPE   BAUD RATE                        |
 * ------------------------------------------------
 *   uartns550   9600
 *   uartlite    Configurable only in HW design
 *   ps7_uart    115200 (configured by bootrom/bsp)
 */

#include <stdio.h>
#include <time.h>
#include "platform.h"
#include "xil_printf.h"

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

int main()
{
    init_platform();

    print("Hello World\n\r");

    // LED demo
    while (1) {
    	LED_Controller_demo(0);
		sleep(3);
		LED_Controller_demo(1);
		sleep(3);
		LED_Controller_demo(2);
		sleep(3);
    }

    cleanup_platform();
    return 0;
}
