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

#ifndef SRC_COMMON_ZYNQIPMC_DRIVERS_GENERICS_ADC_H_
#define SRC_COMMON_ZYNQIPMC_DRIVERS_GENERICS_ADC_H_

#include <stdint.h>
#include <string>

/**
 * Generic ADC class that sensors can implement to be supported in different areas of the framework.
 * The ADC class represents the driver of a source of several ADC channels, like an IC ADC chip with
 * several channels, and then the class ADC::Channel discriminates each one of these channels as
 * necessary.
 *
 * See the AD7689 driver for an implementation example.
 */
class ADC {
public:
	/**
	 * Constructs ADC base class.
	 * @param precision Number of precision bits of the ADC source. e.g. 16 for a 16-bit ADC.
	 * @param identifier Custom name that uniquely identifies this ADC. Optional.
	 */
	ADC(size_t precision, const std::string &identifier = "Unknown") : kPrecision(precision), kIdentifier(identifier) {};

	/**
	 * Take a raw ADC reading from a specific channel.
	 * @param channel Target channel within the ADC source.
	 * @return Raw channel reading.
	 * @throw Should throw std::out_of_range if channel is out-of-range.
	 * @note Pure virtual, must be implemented by the ADC source child class.
	 */
	virtual const uint32_t readRaw(size_t channel) const = 0;

	/**
	 * Take an ADC reading from a specific channel. Uses rawToVolts for the conversion.
	 * @param channel Target channel within the ADC source.
	 * @return Channel reading in volts.
	 * @throw Should throw std::out_of_range if channel is out-of-range.
	 * @note Pure virtual, must be implemented by the ADC source child class.
	 */
	virtual const float readVolts(size_t channel) const = 0;

	//! Converts a raw reading to volts. Formula is ADC dependent and must be implemented.
	virtual const float rawToVolts(uint32_t raw) const = 0;

	//! Converts volts to a raw reading value. Formula is ADC dependent and must be implemented.
	virtual const uint32_t voltsToRaw(float volts) const = 0;

	//! Get the ADC precision in bits.
	inline const uint8_t getPrecision() const { return kPrecision; };

	//! Get the ADC identifier name.
	inline const std::string& getIdentifier() const { return kIdentifier; };

private:
	const size_t kPrecision;		///< ADC precision in bits.
	const std::string kIdentifier;	///< Name/identifier of the ADC, used to print channel info.

public:
	/**
	 * Used to discriminate individual channels from an ADC source.
	 * Allows to apply individual conversion factors, linear and non-linear.
	 */
	class Channel {
	public:
		//! Callback type for non-linear conversion factors
		using Callback = float (*)(float);

		/**
		 * Sets the ADC channel with a linear conversion.
		 * @param adc ADC source where the channel exists.
		 * @param channel Channel number within the ADC source.
		 * @param factor Linear scaling factor from volts. Optional, defaults to 1.0.
		 */
		Channel(ADC &adc, size_t channel, float factor = 1.0) : adc(adc), kChannelNumber(channel), kFactor(factor) {};

		/**
		 * Sets the ADC channel with a non-linear conversion.
		 * @param adc ADC source where the channel exists.
		 * @param channel Channel number within the ADC source.
		 * @param factorFn Function/lambda callback which receives the ADC channel voltage and outputs the conversion result.
		 * @param revFactorFn Similar to factorFn but reversed.
		 */
		Channel(ADC &adc, size_t channel, Callback factorFn, Callback revFactorFn) : adc(adc), kChannelNumber(channel), factorFn(factorFn), revFactorFn(revFactorFn) {};

		//! Raw sensor channel reading.
		inline const uint32_t readRaw() const { return adc.readRaw(this->kChannelNumber); };

		//! Sensor channel reading with individual channel conversion automatically applied.
		inline const float readFloat() const { return this->rawToFloat(this->adc.readRaw(this->kChannelNumber)); };

		/**
		 * Raw to float conversion at the channel level.
		 * @param raw Raw channel value to be converted.
		 * @return Float with the channel conversion factor applied to it.
		 */
		float rawToFloat(uint32_t raw) const {
			float volts = this->adc.rawToVolts(raw);
			if (this->factorFn) {
				return this->factorFn(volts);
			} else {
				return volts * this->kFactor;
			}
		};

		/**
		 * Converted to raw reading conversion.
		 * @param f Converted reading.
		 * @return Raw reading value.
		 */
		uint32_t floatToRaw(float f) const {
			if (this->revFactorFn) {
				return this->adc.voltsToRaw(this->revFactorFn(f));
			} else {
				return this->adc.voltsToRaw(f / this->kFactor);
			}
		}

		//! Get the ADC source reference.
		inline ADC& getADC() const { return this->adc; };

		//! Get the channel number.
		inline size_t getChannelNumber() const { return this->kChannelNumber; };

	private:
		ADC& adc;						///< Associated ADC source.
		const size_t kChannelNumber;	///< Channel number of this object.
		const float kFactor = 1.0;		///< Used if ADC has a linear conversion.
		Callback factorFn = nullptr;	///< Used if the ADC has a non-linear conversion.
		Callback revFactorFn = nullptr;	///< Used to reverse the non-linear conversion.
	};
};

#endif /* SRC_COMMON_ZYNQIPMC_DRIVERS_GENERICS_ADC_H_ */
