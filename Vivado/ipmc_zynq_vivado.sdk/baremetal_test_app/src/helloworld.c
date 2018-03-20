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
#include "xparameters.h"

#include "led_controller.h"
#include "pyld_pwr_ctrl.h"
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


	xil_printf("\n\rMgmt_Zone_Ctrl test completed successfully.\n\r");


	return XST_SUCCESS;
}

int pyld_pwr_ctrl_demo(void) {
	int Status, idx;

	Pyld_Pwr_Ctrl Pyld_Pwr_Ctrl_inst;

	/* Initialize the Payload Power Controller driver */
	Status = Pyld_Pwr_Ctrl_Initialize(&Pyld_Pwr_Ctrl_inst, 0);
	if (Status != XST_SUCCESS) {
		xil_printf("Pyld_Pwr_Ctrl_inst Initialization Failed\n\r");
		return XST_FAILURE;
	}

	int core_ver = Pyld_Pwr_Ctrl_Get_Core_Ver(&Pyld_Pwr_Ctrl_inst);
	xil_printf("Pyld_Pwr_Ctrl_inst  core version: 0x%08x\n\r", core_ver);

	int PE_cnt = Pyld_Pwr_Ctrl_Get_PE_Cnt(&Pyld_Pwr_Ctrl_inst);
	xil_printf("Pyld_Pwr_Ctrl_inst Power Enable Count Config: %d\n\r", PE_cnt);

	int PG_cnt = Pyld_Pwr_Ctrl_Get_PG_Cnt(&Pyld_Pwr_Ctrl_inst);
	xil_printf("Pyld_Pwr_Ctrl_inst Power Good Count Config: %d\n\r", PG_cnt);

	if (PE_cnt < TEST_MIN_PE_CNT)
		return XST_INVALID_VERSION;

	PE_cfg_t PE_cfg[PE_cnt];

	memset(PE_cfg, 0, sizeof(PE_cfg));

	// PE Group 1 config (0,1,2,3,7)
	PE_cfg[0].ext_pd_en = 0;
	PE_cfg[0].group = 1;
	PE_cfg[0].seq_tmr = 100;
	PE_cfg[0].sw_pd_en = 1;

	PE_cfg[1].ext_pd_en = 0;
	PE_cfg[1].group = 1;
	PE_cfg[1].seq_tmr = 200;
	PE_cfg[1].sw_pd_en = 1;

	PE_cfg[2].ext_pd_en = 0;
	PE_cfg[2].group = 1;
	PE_cfg[2].seq_tmr = 10;
	PE_cfg[2].sw_pd_en = 1;

	PE_cfg[3].ext_pd_en = 0;
	PE_cfg[3].group = 1;
	PE_cfg[3].seq_tmr = 300;
	PE_cfg[3].sw_pd_en = 1;

	PE_cfg[7].ext_pd_en = 0;
	PE_cfg[7].group = 1;
	PE_cfg[7].seq_tmr = 300;
	PE_cfg[7].sw_pd_en = 1;

	// PE Group 2 config (10,11,12)
	PE_cfg[10].ext_pd_en = 0;
	PE_cfg[10].group = 2;
	PE_cfg[10].seq_tmr = 500;
	PE_cfg[10].sw_pd_en = 1;

	PE_cfg[11].ext_pd_en = 0;
	PE_cfg[11].group = 2;
	PE_cfg[11].seq_tmr = 60;
	PE_cfg[11].sw_pd_en = 1;

	PE_cfg[12].ext_pd_en = 0;
	PE_cfg[12].group = 2;
	PE_cfg[12].seq_tmr = 10;
	PE_cfg[12].sw_pd_en = 1;

	// PE Group 3 config (8)
	PE_cfg[8].ext_pd_en = 0;
	PE_cfg[8].group = 3;
	PE_cfg[8].seq_tmr = 3000;
	PE_cfg[8].sw_pd_en = 1;



	for (idx = 0; idx < PE_cnt; idx++)
		Pyld_Pwr_Ctrl_Set_Pin_Cfg(&Pyld_Pwr_Ctrl_inst, idx, PE_cfg[idx]);

	int volatile PE_status = Pyld_Pwr_Ctrl_Get_PE_Status(&Pyld_Pwr_Ctrl_inst);
	int volatile PG_status = Pyld_Pwr_Ctrl_Get_PE_Status(&Pyld_Pwr_Ctrl_inst);

	xil_printf(" Power Enable Status: 0x%04x\n\r", PE_status);
	xil_printf(" Power Good Status: 0x%04x\n\r", PG_status);

/////////////

	xil_printf("\n\r *** Pyld Pwr Ctrl Test 1 ***\n\r");

	xil_printf(" PE Group 1 Power Up Sequence starting... \n\r");
	Pyld_Pwr_Ctrl_Init_PUp_Seq(&Pyld_Pwr_Ctrl_inst, PE_GROUP_1);

	sleep(1);

	PE_status = Pyld_Pwr_Ctrl_Get_PE_Status(&Pyld_Pwr_Ctrl_inst);

	xil_printf(" Power Enable Status: 0x%04x\n\r", PE_status);

	if (PE_status != 0x8F)
	{
		xil_printf("\n\rPyld_Pwr_Ctrl test FAILED.\n\r");
		return XST_FAILURE;
	}

	xil_printf(" PE Group 1 Power Down Sequence starting... \n\r");
	Pyld_Pwr_Ctrl_Init_PDown_Seq(&Pyld_Pwr_Ctrl_inst, PE_GROUP_1);

	sleep(1);

	PE_status = Pyld_Pwr_Ctrl_Get_PE_Status(&Pyld_Pwr_Ctrl_inst);

	xil_printf(" Power Enable Status: 0x%04x\n\r", PE_status);

	if (PE_status != 0)
	{
		xil_printf("\n\rPyld_Pwr_Ctrl test FAILED.\n\r");
		return XST_FAILURE;
	}

/////////////
	xil_printf("\n\r *** Pyld Pwr Ctrl Test 2 ***\n\r");

	xil_printf(" PE Group 2 Power Up Sequence starting... \n\r");
	Pyld_Pwr_Ctrl_Init_PUp_Seq(&Pyld_Pwr_Ctrl_inst, PE_GROUP_2);

	sleep(1);

	PE_status = Pyld_Pwr_Ctrl_Get_PE_Status(&Pyld_Pwr_Ctrl_inst);

	xil_printf(" Power Enable Status: 0x%04x\n\r", PE_status);
	if (PE_status != 0x1C00)
	{
		xil_printf("\n\rPyld_Pwr_Ctrl test FAILED.\n\r");
		return XST_FAILURE;
	}

	xil_printf(" PE Group 2 Power Down Sequence starting... \n\r");
	Pyld_Pwr_Ctrl_Init_PDown_Seq(&Pyld_Pwr_Ctrl_inst, PE_GROUP_2);

	sleep(1);

	PE_status = Pyld_Pwr_Ctrl_Get_PE_Status(&Pyld_Pwr_Ctrl_inst);

	xil_printf(" Power Enable Status: 0x%04x\n\r", PE_status);
	if (PE_status != 0)
	{
		xil_printf("\n\rPyld_Pwr_Ctrl test FAILED.\n\r");
		return XST_FAILURE;
	}

/////////////
		xil_printf("\n\r *** Pyld Pwr Ctrl Test 3 ***\n\r");

		xil_printf(" PE Group 3 Power Up Sequence starting... \n\r");
		Pyld_Pwr_Ctrl_Init_PUp_Seq(&Pyld_Pwr_Ctrl_inst, PE_GROUP_3);

		PE_status = Pyld_Pwr_Ctrl_Get_PE_Status(&Pyld_Pwr_Ctrl_inst);

		xil_printf(" Power Enable Status: 0x%04x\n\r", PE_status);
		// Group 3 (PE pin 8) was configured with 3000ms count up period before turning on
		// It should not be up at this point yet
		if (PE_status != 0)
		{
			xil_printf("\n\rPyld_Pwr_Ctrl test FAILED.\n\r");
			return XST_FAILURE;
		}

		xil_printf("Waiting 3000ms...\n\r");
		sleep(3);
		PE_status = Pyld_Pwr_Ctrl_Get_PE_Status(&Pyld_Pwr_Ctrl_inst);

		xil_printf(" Power Enable Status: 0x%04x\n\r", PE_status);
		// Group 3 (PE pin 8) was configured with 3000ms count up period before turning on
		// Now it should be up
		if (PE_status != 0x0100)
		{
			xil_printf("\n\rPyld_Pwr_Ctrl test FAILED.\n\r");
			return XST_FAILURE;
		}

		xil_printf(" PE Group 3 Power Down Sequence starting... \n\r");
		Pyld_Pwr_Ctrl_Init_PDown_Seq(&Pyld_Pwr_Ctrl_inst, PE_GROUP_3);

		sleep(3); // wait for at least 3000ms before checking

		PE_status = Pyld_Pwr_Ctrl_Get_PE_Status(&Pyld_Pwr_Ctrl_inst);

		xil_printf(" Power Enable Status: 0x%04x\n\r", PE_status);
		if (PE_status != 0)
		{
			xil_printf("\n\rPyld_Pwr_Ctrl test FAILED.\n\r");
			return XST_FAILURE;
		}
	/////////////


/////////////

	xil_printf("\n\r *** Pyld Pwr Ctrl Test 4 ***\n\r");

	xil_printf(" PE Group 1 and 2 Power Up Sequence starting... \n\r");
	Pyld_Pwr_Ctrl_Init_PUp_Seq(&Pyld_Pwr_Ctrl_inst, PE_GROUP_1 | PE_GROUP_2);


	sleep(1);
	PE_status = Pyld_Pwr_Ctrl_Get_PE_Status(&Pyld_Pwr_Ctrl_inst);

	xil_printf(" Power Enable Status: 0x%04x\n\r", PE_status);
	if (PE_status != 0x1C8F)
	{
		xil_printf("\n\rPyld_Pwr_Ctrl test FAILED.\n\r");
		return XST_FAILURE;
	}

	xil_printf(" Force PE Power Down... \n\r");
	Pyld_Pwr_Ctrl_PDown_Force(&Pyld_Pwr_Ctrl_inst);

	PE_status = Pyld_Pwr_Ctrl_Get_PE_Status(&Pyld_Pwr_Ctrl_inst);

	xil_printf(" Power Enable Status: 0x%04x\n\r", PE_status);
	if (PE_status != 0)
	{
		xil_printf("\n\rPyld_Pwr_Ctrl test FAILED.\n\r");
		return XST_FAILURE;
	}


	Pyld_Pwr_Ctrl_PDown_Release(&Pyld_Pwr_Ctrl_inst);

	xil_printf("\n\rPyld_Pwr_Ctrl test completed successfully.\n\r");


	return XST_SUCCESS;
}

int main() {
	init_platform();

	xil_printf("ZYNQ-IPMC low-level driver testbench\n\r");

	pyld_pwr_ctrl_demo();

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
