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

#ifndef SRC_COMMON_ZYNQIPMC_DRIVERS_PS_XADC_PS_XADC_H_
#define SRC_COMMON_ZYNQIPMC_DRIVERS_PS_XADC_PS_XADC_H_

// Only include driver if PS XADC is detected in the BSP.
#if XSDK_INDEXING || __has_include("xadcps.h")

#include "xadcps.h"
#include <drivers/generics/adc.h>

/**
 * Driver for Xilinx XADC in the Zynq PS.
 * Allows to read voltages and temperatures internal to the Zynq.
 */
class PSXADC : public ADC {
public:
	/**
	 * Initializes the XADC driver.
	 * @param device_id Hardware device ID, normally XPAR_XADCPS_0_DEVICE_ID. Check xparameters.h.
	 * @throw except::hardware_error if low level driver fails to configure.
	 */
	PSXADC(uint16_t device_id);
	virtual ~PSXADC() {};

	// From base class ADC:
	virtual const uint32_t readRaw(size_t channel) const;
	//virtual const float readVolts(size_t channel) const; // Don't override
	virtual const uint32_t voltsToRaw(float volts) const;
	virtual const float rawToVolts(uint32_t raw) const;

	//! Get on-die temperature in Celcius.
	inline float getTemperature() {
		return XAdcPs_RawToTemperature(XAdcPs_GetAdcData(&this->xadc, XADCPS_CH_TEMP));
	};

	//! Get internal voltage in Volts.
	inline float getVccInt() {
		return XAdcPs_RawToVoltage(XAdcPs_GetAdcData(&xadc, XADCPS_CH_VCCINT));
	};

	//! Get auxiliary voltage in Volts.
	inline float getVccAux() {
		return XAdcPs_RawToVoltage(XAdcPs_GetAdcData(&xadc, XADCPS_CH_VCCAUX));
	};

	//! Get BRAM voltage in Volts.
	inline float getVbram() {
		return XAdcPs_RawToVoltage(XAdcPs_GetAdcData(&xadc, XADCPS_CH_VBRAM));
	};

	//! Get processor internal voltage in Volts.
	inline float getVccPInt() {
		return XAdcPs_RawToVoltage(XAdcPs_GetAdcData(&xadc, XADCPS_CH_VCCPINT));
	};

	//! Get processor auxiliary voltage in Volts.
	inline float getVccPAux() {
		return XAdcPs_RawToVoltage(XAdcPs_GetAdcData(&xadc, XADCPS_CH_VCCPAUX));
	};

	//! Get processor memory voltage in Volts.
	inline float getVccPdro() {
		return XAdcPs_RawToVoltage(XAdcPs_GetAdcData(&xadc, XADCPS_CH_VCCPDRO));
	};

private:
	mutable XAdcPs xadc;	///< Internal XADC driver data.
};

#endif

#endif /* SRC_COMMON_ZYNQIPMC_DRIVERS_PS_XADC_PS_XADC_H_ */
