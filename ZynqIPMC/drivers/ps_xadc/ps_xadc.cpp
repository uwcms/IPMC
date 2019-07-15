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

#include <drivers/ps_xadc/ps_xadc.h>
#include <libs/except.h>

// Only include driver if PS XADC is detected in the BSP.
#if XSDK_INDEXING || __has_include("xadcps.h")

PSXADC::PSXADC(uint16_t device_id) :
ADC(16, "ZynqXADC") {
	XAdcPs_Config *xadcps_config = XAdcPs_LookupConfig(device_id);
	if (xadcps_config == nullptr) {
		throw except::hardware_error("Unable to initialize PSXADC(device_id=" + std::to_string(device_id) + ")");
	}

	XAdcPs_CfgInitialize(&this->xadc, xadcps_config, xadcps_config->BaseAddress);
}

const uint32_t PSXADC::readRaw(size_t channel) const {
	if (channel >= 32) {
		throw std::out_of_range("Target channel ( " + std::to_string(channel) + ") is out-of-range, maximum is 31");
	}

	return XAdcPs_GetAdcData((XAdcPs*)&this->xadc, channel);
}

const uint32_t PSXADC::voltsToRaw(float volts) const {
	return XAdcPs_VoltageToRaw(volts);
}

const float PSXADC::rawToVolts(uint32_t raw) const {
	return XAdcPs_RawToVoltage(raw);
}

#endif
