/*
 * AD7689.cpp
 *
 *  Created on: Sep 19, 2018
 *      Author: mpv
 */

#include <drivers/ad7689/AD7689.h>

#define ADC_RAW_TO_V(v) ((float)v * 2500.0 / 65535.0 / 1000.0)
#define ADC_V_TO_C(v) (v * 25000.0 / 283.0)

AD7689::AD7689(uint16_t DeviceId) {
	// Initialize the low level driver
	configASSERT(XST_SUCCESS == AD7689_S_Initialize(&(this->adc), DeviceId));

	// Apply default configurations
	this->setSamplingFrequency(1000);
	AD7689_S_Set_Master_Ovrrd_Enable(&(this->adc), 0);
}

AD7689::~AD7689() {
	// TODO
}

void AD7689::setSamplingFrequency(uint32_t hz) {
	AD7689_S_Set_Conv_Freq(&(this->adc), hz);
}

float AD7689::getTemperature() {
	// Channel 8 is the internal temperature monitor
	return ADC_V_TO_C(this->getReading(8));
}

float AD7689::getReading(uint8_t channel) {
	uint16_t val;
	// TODO: Throw if invalid channel
	AD7689_S_Get_Reading(&(this->adc), channel, &val);
	return ADC_RAW_TO_V(val);
}
