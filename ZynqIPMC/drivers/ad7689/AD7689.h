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
#include <services/console/CommandParser.h>
#include "ad7689_s.h"

class AD7689 : public ADC {
public:
	AD7689(uint16_t DeviceId, uint32_t SlaveInterface = 0);
	virtual ~AD7689();

	void setSamplingFrequency(uint32_t hz);
	float getTemperature();

	virtual uint16_t getRawReading(uint8_t channel) const;
	virtual float convertReading(uint16_t raw_reading) const;
	virtual uint16_t convertReading(float reading) const;

	void register_console_commands(CommandParser &parser, const std::string &prefix="");
	void deregister_console_commands(CommandParser &parser, const std::string &prefix="");

	friend class adc_override;

private:
	mutable AD7689_S adc = {0};
	uint32_t SlaveInterface;
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_AD7689_AD7689_H_ */
