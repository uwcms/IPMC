/*
 * PSXADC.h
 *
 *  Created on: Aug 14, 2018
 *      Author: mpv
 */

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_PS_XADC_PSXADC_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_PS_XADC_PSXADC_H_

#include <xadcps.h>
#include <libs/SensorSource.h>

class PS_XADC /*: public SensorSource*/ {
public:
	PS_XADC(uint16_t deviceId);
	virtual ~PS_XADC() {};

	///! Get on-die temperature in Celcius.
	inline float getTemperature() { return XAdcPs_RawToTemperature(XAdcPs_GetAdcData(&this->xadc, XADCPS_CH_TEMP)); };

	///! Get internal voltage in Volts.
	inline float getVccInt() { return XAdcPs_RawToVoltage(XAdcPs_GetAdcData(&xadc, XADCPS_CH_VCCINT)); };

	///! Get auxiliary voltage in Volts.
	inline float getVccAux() { return XAdcPs_RawToVoltage(XAdcPs_GetAdcData(&xadc, XADCPS_CH_VCCAUX)); };

	///! Get BRAM voltage in Volts.
	inline float getVbram() { return XAdcPs_RawToVoltage(XAdcPs_GetAdcData(&xadc, XADCPS_CH_VBRAM)); };

	///! Get processor internal voltage in Volts.
	inline float getVccPInt() { return XAdcPs_RawToVoltage(XAdcPs_GetAdcData(&xadc, XADCPS_CH_VCCPINT)); };

	///! Get processor auxiliary voltage in Volts.
	inline float getVccPAux() { return XAdcPs_RawToVoltage(XAdcPs_GetAdcData(&xadc, XADCPS_CH_VCCPAUX)); };

	///! Get processor memory (?) voltage in Volts.
	inline float getVccPdro() { return XAdcPs_RawToVoltage(XAdcPs_GetAdcData(&xadc, XADCPS_CH_VCCPDRO)); };

	//SensorList refreshSensorList();

private:
	XAdcPs xadc;
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_PS_XADC_PSXADC_H_ */
