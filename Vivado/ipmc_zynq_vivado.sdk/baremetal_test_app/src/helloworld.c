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
#include <string.h>
#include <time.h>
#include "platform.h"
#include "xil_printf.h"
#include "xparameters.h"

#include "led_controller.h"
#include "mgmt_zone_ctrl.h"

#define TEST_MIN_PE_CNT 12

/* Three different demos, 0-On/Off, 1-Pulse, 2-Dim */
int LED_Controller_demo(int demo) {
	int i, k, Status;

	LED_Controller led_controller_list[XPAR_LED_CONTROLLER_NUM_INSTANCES];

	/* Initialize the LED Controller drivers */
	for (i = 0; i < XPAR_LED_CONTROLLER_NUM_INSTANCES; i++) {
		Status = LED_Controller_Initialize(&(led_controller_list[i]), i);
		if (Status != XST_SUCCESS) {
			xil_printf("LED_Controller Initialization Failed\n\r");
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
				LED_Controller_Pulse(&(led_controller_list[i]), k,
				LED_PULSE_NORMAL);
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
	default:
		break;
	}

	return XST_SUCCESS;
}

int mgmt_zone_ctrl_demo(void) {
	int Status;

	Mgmt_Zone_Ctrl Mgmt_Zone_Ctrl_inst;

	/* Initialize the Payload Power Controller driver */
	Status = Mgmt_Zone_Ctrl_Initialize(&Mgmt_Zone_Ctrl_inst, 0);
	if (Status != XST_SUCCESS) {
		xil_printf("Mgmt_Zone_Ctrl_inst Initialization Failed\n\r");
		return XST_FAILURE;
	}

	MZ_config cfg, cfg_rb;

	memset(&cfg, 0, sizeof(cfg));
	memset(&cfg_rb, 0, sizeof(cfg_rb));

	cfg.fault_holdoff = 10;
	cfg.hardfault_mask = 0xABCD00000ff0f0f;

	for (int i = 0; i<32; i++)
	{
		cfg.pwren_cfg[i] = 0;
	}

/*
 * /**
 *  from mgmt_zone_ctrl.h
 *
 * Power_Enable Config
 *
 * Bit [15:0] up/down timer cfg (in miliseconds)
 * Bit   [16] active_level; set to 0 for active low outputs, set to 1 for active high outputs
 * Bit   [17] drive_enable; set to 0 to tri-state the output, set to 1 to enable the outputs
 * typedef u32 PWREN_cfg_t;
 *
 */

	cfg.pwren_cfg[0] = (1 << 17) | (1 << 16) | 1000;
	cfg.pwren_cfg[1] = (1 << 17) | (1 << 16) | 2000;
	cfg.pwren_cfg[2] = (1 << 17) | (1 << 16) | 3000;
	cfg.pwren_cfg[3] = (1 << 17) | (1 << 16) | 4000;
	cfg.pwren_cfg[4] = (1 << 17) | (1 << 16) | 5000;
	cfg.pwren_cfg[5] = (1 << 17) | (1 << 16) | 6000;
	cfg.pwren_cfg[6] = (1 << 17) | (1 << 16) | 7000;


	u32 MZ = 1;

	// Configure MZ
	Mgmt_Zone_Ctrl_Set_MZ_Cfg(&Mgmt_Zone_Ctrl_inst,  MZ,  &cfg);

	//Read back MZ configuration
	Mgmt_Zone_Ctrl_Get_MZ_Cfg(&Mgmt_Zone_Ctrl_inst,  MZ,  &cfg_rb);

    s32 rb_valid = memcmp(&cfg, &cfg_rb, sizeof(cfg));

    if (rb_valid != 0)
    {
    	xil_printf("MZ_cfg readback validation failure");
    	xil_printf("\n\rMgmt_Zone_Ctrl test failed.\n\r");
    	return XST_FAILURE;
    }

	// Initiate power ON sequence
	Mgmt_Zone_Ctrl_Pwr_ON_Seq(&Mgmt_Zone_Ctrl_inst, MZ);

	// with the configuration above, it takes 7 seconds for the last pwr en pin to activate
	for (int idx = 0; idx < 10; idx++)
	{
		MZ_pwr mz_pwr = Mgmt_Zone_Ctrl_Get_MZ_Status(&Mgmt_Zone_Ctrl_inst, MZ);
		u32 pwr_en_status = Mgmt_Zone_Ctrl_Get_Pwr_En_Status(&Mgmt_Zone_Ctrl_inst);

		sleep(1);

		xil_printf("Power ON sequence in progress...  MZ pwr state: %d; Pwr En Status: 0x%08x\n\r", mz_pwr, pwr_en_status);
	}

	xil_printf("\n\r");

	// Initiate power OFF sequence
	Mgmt_Zone_Ctrl_Pwr_OFF_Seq(&Mgmt_Zone_Ctrl_inst, MZ);
	// with the configuration above, it takes 7 seconds for the last pwr en pin to deactivate
	for (int idx = 0; idx < 10; idx++)
	{
		MZ_pwr mz_pwr = Mgmt_Zone_Ctrl_Get_MZ_Status(&Mgmt_Zone_Ctrl_inst, MZ);
		u32 pwr_en_status = Mgmt_Zone_Ctrl_Get_Pwr_En_Status(&Mgmt_Zone_Ctrl_inst);

		sleep(1);

		xil_printf("Power OFF sequence in progress... MZ pwr state: %d; Pwr En Status: 0x%08x\n\r", mz_pwr, pwr_en_status);
	}

	xil_printf("\n\r");


	//Renable power enable outputs
	Mgmt_Zone_Ctrl_Pwr_ON_Seq(&Mgmt_Zone_Ctrl_inst, MZ);
	for (int idx = 0; idx < 10; idx++)
	{
		MZ_pwr mz_pwr = Mgmt_Zone_Ctrl_Get_MZ_Status(&Mgmt_Zone_Ctrl_inst, MZ);
		u32 pwr_en_status = Mgmt_Zone_Ctrl_Get_Pwr_En_Status(&Mgmt_Zone_Ctrl_inst);

		sleep(1);

		xil_printf("Power ON sequence in progress...  MZ pwr state: %d; Pwr En Status: 0x%08x\n\r", mz_pwr, pwr_en_status);
	}

	xil_printf("Dispatch soft fault to MZ %d...\n\r", MZ);
	// Power off instantly all pwr en pins managed by this MZ
	Mgmt_Zone_Ctrl_Dispatch_Soft_Fault(&Mgmt_Zone_Ctrl_inst, MZ);

	MZ_pwr mz_pwr = Mgmt_Zone_Ctrl_Get_MZ_Status(&Mgmt_Zone_Ctrl_inst, MZ);
	u32 pwr_en_status = Mgmt_Zone_Ctrl_Get_Pwr_En_Status(&Mgmt_Zone_Ctrl_inst);

	xil_printf("\n\r");

	xil_printf("Post soft fault dispatch status... MZ pwr state: %d; Pwr En Status: 0x%08x\n\r", mz_pwr, pwr_en_status);


	xil_printf("\n\rMgmt_Zone_Ctrl test completed successfully.\n\r");

	return XST_SUCCESS;
}

int main() {
	init_platform();

	xil_printf("ZYNQ-IPMC low-level driver testbench\n\r");

	mgmt_zone_ctrl_demo();

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