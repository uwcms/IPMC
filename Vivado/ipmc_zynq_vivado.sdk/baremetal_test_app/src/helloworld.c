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

int main()
{
	LED_Controller led_intf[2];
	int i = 0, k;
	u8 state[6] = {0};

	LED_Controller *target_ctrl;
	u32 target_intf;
	u32 mode = 0;

    init_platform();

    print("Hello World\n\r");

    // LED demo
    LED_Controller_Initialize(&(led_intf[0]), 0);
    LED_Controller_Initialize(&(led_intf[1]), 1);

    while (1) {
		for (i = 0; i < 6; i++) {
			if (i < 2) {
				target_ctrl = &(led_intf[0]);
				target_intf = i;
			} else {
				target_ctrl = &(led_intf[1]);
				target_intf = i - 2;
			}

			if (mode == 0) {
				LED_Controller_SetOnOff(target_ctrl, target_intf, state[i]);
				state[i] = (state[i]!=0?0:1);
			} else if (mode == 1) {
				LED_Controller_Pulse(target_ctrl, target_intf, LED_PULSE_NORMAL);
			} else {
				LED_Controller_Dim(target_ctrl, target_intf, LED_DIM_50);
			}


			if (i == 5) {
				mode++;
				if (mode > 2) mode = 0;
			}

			usleep(200000);
		}
    }

    cleanup_platform();
    return 0;
}
