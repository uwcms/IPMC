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
	virtual float getReading (uint8_t channel) const final { return this->convertReading(this->getRawReading(channel)); };
	virtual uint16_t getRawReading(uint8_t channel) const = 0;
	virtual float convertReading(uint16_t raw_reading) const = 0;
	virtual uint16_t convertReading(float reading) const = 0;

	class Channel {
	public:
		using Callback = float (*)(float);
		Channel(ADC& adc, uint8_t channel, float factor = 1.0) : adc(adc), channel(channel), factor(factor) {};
		Channel(ADC& adc, uint8_t channel, Callback callback, Callback callback_reverse) : adc(adc), channel(channel), callback(callback), callback_reverse(callback_reverse) { };
		explicit operator float() const {
			return convert_from_raw(this->adc.getRawReading(this->channel));
		};
		explicit operator uint16_t() const {
			return this->adc.getRawReading(this->channel);
		}
		float convert_from_raw(uint16_t r) const {
			float v = this->adc.convertReading(r);
			if (this->callback)
				return this->callback(v);
			return v * this->factor;
		};
		uint16_t convert_to_raw(float v) const {
			if (this->callback_reverse)
				return this->adc.convertReading(this->callback_reverse(v));
			return this->adc.convertReading(v / this->factor);
		}
		inline uint8_t getChannel() const { return this->channel; };
	private:
		ADC& adc;
		uint8_t channel;
		Callback callback = nullptr;
		Callback callback_reverse = nullptr;
		float factor = 1.0;
	};
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_GENERICS_ADC_H_ */
