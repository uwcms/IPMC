/*
 * PSXADC.cpp
 *
 *  Created on: Aug 14, 2018
 *      Author: mpv
 */

#include <drivers/ps_xadc/PSXADC.h>

PS_XADC::PS_XADC(uint16_t deviceId) :
ADC(16, "ZynqXADC") {
	XAdcPs_Config *xadcps_config = XAdcPs_LookupConfig(deviceId);
	XAdcPs_CfgInitialize(&this->xadc, xadcps_config, xadcps_config->BaseAddress);
}

