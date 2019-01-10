/*
 * PLLED.h
 *
 *  Created on: Jan 9, 2019
 *      Author: mpv
 */

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_PL_LED_PLLED_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_PL_LED_PLLED_H_

#include <inttypes.h>
#include <led_controller.h>

class LED; // Pre-declaration

/**
 * PL LED Controller high-level driver.
 * Check LED class on how to control individual LEDs.
 */
class PL_LED {
public:
	/**
	 * Create a PL based LED Controller interface.
	 * @param deviceId The device ID, normally XPAR_AXI_LED_CONTROLLER_<>.
	 * @param plFrequency Frequency of the LED Controller.
	 */
	PL_LED(uint16_t deviceId, uint32_t plFrequency);

	friend class LED;
private:
	LED_Controller ledController;   ///< LED controller info.
	uint32_t plFrequency;           ///< IP frequency, used for timing operations.
};

/**
 * Individual LED control from a single controller.
 */
class LED {
public:
	/**
	 * Create a new LED interface from a PL LED Controller.
	 * @param controller Reference of the target controller.
	 * @param interface LED interface number in the controller.
	 */
	LED(PL_LED &controller, uint32_t interface);

	///! Turn the LED on.
	void on();

	///! Turn the LED off.
	void off();

	/**
	 * Dim the LED.
	 * @param intensity Dimming intensity, from 1.0 to 0.0.
	 */
	void dim(float intensity);

	/**
	 * Make the LED blink.
	 * @param periodMs Milliseconds between blinks.
	 * @param timeOnMs Milliseconds in relation to the period that the LED will be on.
	 */
	void blink(uint32_t periodMs, uint32_t timeOnMs);

	/**
	 * Make the LED pulse for a certain period.
	 * @param periodMs Milliseconds between pulses.
	 */
	void pulse(uint32_t periodMs);

private:
	PL_LED &controller;  ///< LED controller.
	uint32_t interface;  ///< LED interface number.
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_PL_LED_PLLED_H_ */
