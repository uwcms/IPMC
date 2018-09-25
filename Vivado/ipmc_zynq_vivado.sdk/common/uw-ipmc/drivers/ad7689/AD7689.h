/*
 * AD7689.h
 *
 *  Created on: Sep 19, 2018
 *      Author: mpv
 */

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_AD7689_AD7689_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_AD7689_AD7689_H_

#include <FreeRTOS.h>
#include <drivers/generics/ADC.h>
#include "ad7689_s.h"

class AD7689 : public ADC {
public:
	AD7689(uint16_t DeviceId);
	virtual ~AD7689();

	void setSamplingFrequency(uint32_t hz);
	float getTemperature();

	float getReading(uint8_t channel);

private:
	AD7689_S adc = {0};
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_AD7689_AD7689_H_ */
