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

#ifndef SRC_COMMON_ZYNQIPMC_DRIVERS_PL_LED_PL_LED_H_
#define SRC_COMMON_ZYNQIPMC_DRIVERS_PL_LED_PL_LED_H_

// Only include driver if the AD7689 is detected in the BSP.
#if XSDK_INDEXING || __has_include("led_controller.h")

#include "led_controller.h"
#include <drivers/generics/led.h>
#include <inttypes.h>

/**
 * PL LED controller high-level driver.
 * The controller is a custom IP block available on the ZYNQ-IPMC
 * IP library and can control several LEDs. The class PLLED is used
 * to discriminate individual LEDs and control them.
 *
 * First initialize an PLLEDController and then initialize as many PLLEDs
 * as needed for each LED connected to the IP.
 */
class PLLEDController {
public:
	/**
	 * Create a PL based LED Controller interface.
	 * @param device_id The device ID, normally XPAR_AXI_LED_CONTROLLER_<>_DEVICE_ID.
	 * @param pl_frequency Frequency of the LED Controller in Hertz.
	 * @throw except::hardware_error if low level driver fails to configure.
	 */
	PLLEDController(uint16_t device_id, size_t pl_frequency);

	//! Individual PL LED control.
	class PLLED : public LED {
	public:
		/**
		 * Create a new LED interface from a PL LED Controller.
		 * @param controller Reference of the target controller.
		 * @param interface LED interface number in the controller.
		 * @throw std::out_of_range if interface is outside range.
		 */
		PLLED(PLLEDController &controller, size_t interface);

		// From base class LED:
		virtual void on();
		virtual void off();
		virtual bool dim(float intensity);
		virtual bool blink(uint32_t period_ms, uint32_t timeon_ms);
		virtual bool pulse(uint32_t period_ms);

	private:
		PLLEDController &controller;	///< LED controller.
	size_t interface;					///< LED interface number.
	};
private:
	LED_Controller ledctrl;		///< LED Controller driver data.
	const size_t kFrequency;	///< IP frequency in Hertz, used for timing operations.
};

#endif

#endif /* SRC_COMMON_ZYNQIPMC_DRIVERS_PL_LED_PL_LED_H_ */
