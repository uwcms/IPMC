/*
 * LED.h
 *
 *  Created on: Mar 27, 2019
 *      Author: mpv
 */

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_GENERICS_LED_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_GENERICS_LED_H_

#include <inttypes.h>

/**
 * Individual LED control from a single controller.
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
	 */
	virtual void dim(float intensity) = 0;

	/**
	 * Make the LED blink.
	 * @param periodMs Milliseconds between blinks.
	 * @param timeOnMs Milliseconds in relation to the period that the LED will be on.
	 */
	virtual void blink(uint32_t periodMs, uint32_t timeOnMs) = 0;

	/**
	 * Make the LED pulse for a certain period.
	 * @param periodMs Milliseconds between pulses.
	 */
	virtual void pulse(uint32_t periodMs) = 0;
};



#endif /* SRC_COMMON_UW_IPMC_DRIVERS_GENERICS_LED_H_ */
