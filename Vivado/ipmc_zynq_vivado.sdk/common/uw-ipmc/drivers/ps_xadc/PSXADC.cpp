/*
 * PSXADC.cpp
 *
 *  Created on: Aug 14, 2018
 *      Author: mpv
 */

#include <drivers/ps_xadc/PSXADC.h>

PS_XADC::PS_XADC(uint16_t deviceId) {
	XAdcPs_Config *xadcps_config = XAdcPs_LookupConfig(deviceId);
	XAdcPs_CfgInitialize(&this->xadc, xadcps_config, xadcps_config->BaseAddress);

	//startRefreshTask("xadc");
}

/*SensorSource::SensorList PS_XADC::refreshSensorList() {
	float temp = XAdcPs_RawToTemperature(XAdcPs_GetAdcData(&xadc, XADCPS_CH_TEMP));
	float vccint = XAdcPs_RawToVoltage(XAdcPs_GetAdcData(&xadc, XADCPS_CH_VCCINT));
	float vccaux = XAdcPs_RawToVoltage(XAdcPs_GetAdcData(&xadc, XADCPS_CH_VCCAUX));
	float vbram = XAdcPs_RawToVoltage(XAdcPs_GetAdcData(&xadc, XADCPS_CH_VBRAM));
	float vccpint = XAdcPs_RawToVoltage(XAdcPs_GetAdcData(&xadc, XADCPS_CH_VCCPINT));
	float vccpaux = XAdcPs_RawToVoltage(XAdcPs_GetAdcData(&xadc, XADCPS_CH_VCCPAUX));
	float vccpdr0 = XAdcPs_RawToVoltage(XAdcPs_GetAdcData(&xadc, XADCPS_CH_VCCPDRO));

	return {
		{"xadc", "temp", UNIT_CELSIUS, SENSORDATA(temp)},
		{"xadc", "vccint", UNIT_VOLT, SENSORDATA(vccint)},
		{"xadc", "vccaux", UNIT_VOLT, SENSORDATA(vccaux)},
		{"xadc", "vbram", UNIT_VOLT, SENSORDATA(vbram)},
		{"xadc", "vccpint", UNIT_VOLT, SENSORDATA(vccpint)},
		{"xadc", "vccpaux", UNIT_VOLT, SENSORDATA(vccpaux)},
		{"xadc", "vccpdr0", UNIT_VOLT, SENSORDATA(vccpdr0)},
	};
}
*/

