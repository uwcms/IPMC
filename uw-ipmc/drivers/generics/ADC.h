/*
 * ADC.h
 *
 *  Created on: Sep 19, 2018
 *      Author: mpv
 */

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_GENERICS_ADC_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_GENERICS_ADC_H_

#include <stdint.h>

class ADC {
public:
	virtual float getReading (uint8_t channel) = 0;

	class Channel {
	public:
		using Callback = float (*)(float);
		Channel(ADC& adc, uint8_t channel, float factor = 1.0) : adc(adc), channel(channel), factor(factor) {};
		Channel(ADC& adc, uint8_t channel, Callback callback) : adc(adc), channel(channel), callback(callback) {};
		explicit operator float() {
			float r = this->adc.getReading(this->channel);
			if (this->callback) return this->callback(r);
			else return r * this->factor;
		};
		inline uint8_t getChannel() { return this->channel; };
	private:
		ADC& adc;
		uint8_t channel;
		Callback callback = nullptr;
		float factor = 1.0;
	};
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_GENERICS_ADC_H_ */
