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
		Channel(ADC& adc, uint8_t channel, Callback callback, Callback callback_reverse) : adc(adc), channel(channel), callback(callback), callback_reverse(callback_reverse) { };
		explicit operator float() {
			return convert_from_raw(this->adc.getReading(this->channel));
		};
		float convert_from_raw(float r) {
			if (this->callback)
				return this->callback(r);
			return r * this->factor;
		};
		float convert_to_raw(float v) {
			if (this->callback_reverse)
				return this->callback_reverse(v);
			return v / this->factor;
		}
		inline uint8_t getChannel() { return this->channel; };
	private:
		ADC& adc;
		uint8_t channel;
		Callback callback = nullptr;
		Callback callback_reverse = nullptr;
		float factor = 1.0;
	};
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_GENERICS_ADC_H_ */
