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

#ifndef SRC_COMMON_ZYNQIPMC_DRIVERS_GENERICS_LED_H_
#define SRC_COMMON_ZYNQIPMC_DRIVERS_GENERICS_LED_H_

#include <inttypes.h>

/**
 * Abstract individual LED interface.
 * Even thought there are functions for dimming, blink or pulse,
 * the LED doesn't need to support those features.
 */
class LED {
public:
	///! Turn the LED on.
	virtual void on() = 0;

	///! Turn the LED off.
	virtual void off() = 0;

	/**
	 * Dim the LED.
	 * @param intensity Dimming intensity, from 1.0 to 0.0.
	 * @return true if feature supported.
	 */
	virtual bool dim(float intensity) = 0;

	/**
	 * Make the LED blink.
	 * @param period_ms Milliseconds between blinks.
	 * @param timeon_ms Milliseconds in relation to the period that the LED will be on.
	 * @return true if feature supported.
	 */
	virtual bool blink(uint32_t period_ms, uint32_t timeon_ms) = 0;

	/**
	 * Make the LED pulse for a certain period.
	 * @param period_ms Milliseconds between pulses.
	 * @return true if feature supported.
	 */
	virtual bool pulse(uint32_t period_ms) = 0;
};

#endif /* SRC_COMMON_ZYNQIPMC_DRIVERS_GENERICS_LED_H_ */
