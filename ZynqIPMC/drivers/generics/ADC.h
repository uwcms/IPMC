/*
 * ADC.h
 *
 *  Created on: Sep 19, 2018
 *      Author: mpv
 */

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_GENERICS_ADC_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_GENERICS_ADC_H_

#include <stdint.h>
#include <string>

class ADC {
public:
	ADC(uint8_t precision, const std::string &identifier = "Unknown") : kPrecision(precision), kIdentifier(identifier) {};

	virtual const uint32_t readRaw(size_t channel) const = 0;
	virtual const float readVolts(size_t channel) const = 0;

	virtual const uint32_t voltsToRaw(float volts) const = 0;
	virtual const float rawToVolts(uint32_t raw) const = 0;

	inline const uint8_t getPrecision() { return kPrecision; };
	inline const std::string& getIdentifier() { return kIdentifier; };

private:
	const uint8_t kPrecision;		///< Maximum value when using readRaw().
	const std::string kIdentifier;	///< Name/identifier of the ADC, used to print channel info.

public:
	class Channel {
	public:
		using Callback = float (*)(float);

		Channel(ADC &adc, size_t channel, float factor = 1.0) : adc(adc), kChannel(channel), factor(factor) {};
		Channel(ADC &adc, size_t channel, Callback factorFn, Callback revFactorFn) : adc(adc), kChannel(channel), factorFn(factorFn), revFactorFn(revFactorFn) {};

		inline const uint32_t readRaw() const { return adc.readRaw(this->kChannel); };
		inline const float readFloat() const {
			return this->rawToFloat(this->adc.readRaw(this->kChannel));
		};

		float rawToFloat(uint32_t raw) const {
			float volts = this->adc.rawToVolts(raw);
			if (this->factorFn) {
				return this->factorFn(volts);
			} else {
				return volts * this->factor;
			}
		};

		uint32_t floatToRaw(float f) const {
			if (this->revFactorFn) {
				return this->adc.voltsToRaw(this->revFactorFn(f));
			} else {
				return this->adc.voltsToRaw(f / this->factor);
			}
		}

		inline ADC& getADC() const { return this->adc; };
		inline size_t getChannelNumber() const { return this->kChannel; };

	private:
		ADC& adc;
		const size_t kChannel;
		float factor = 1.0;				///< Used if ADC has a linear conversion.
		Callback factorFn = nullptr;	///< Used if the ADC has a non-linear conversion.
		Callback revFactorFn = nullptr;	///< Used to reverse the non-linear conversion.
	};
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_GENERICS_ADC_H_ */
