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

#include <drivers/pl_led/pl_led.h>

// Only include driver if the AD7689 is detected in the BSP.
#if XSDK_INDEXING || __has_include("led_controller.h")

#include <drivers/pl_led/pl_led.h>
#include <libs/except.h>
#include <string>

PLLEDController::PLLEDController(uint16_t device_id, size_t pl_frequency) :
kFrequency(pl_frequency) {
	if (XST_SUCCESS != LED_Controller_Initialize(&(this->ledctrl), device_id)) {
		throw except::hardware_error("Unable to initialize PLLED(device_id=" + std::to_string(device_id) + ")");
	}
}

PLLEDController::PLLED::PLLED(PLLEDController &controller, size_t interface) :
controller(controller), interface(interface) {
	if (interface > controller.ledctrl.InterfaceCount) {
		throw except::hardware_error("LED interface " + std::to_string(interface) + " is out-of-range");
	}
}

void PLLEDController::PLLED::on() {
	LED_Controller_Set(&(this->controller.ledctrl), this->interface, 0, 0, 1);
}

void PLLEDController::PLLED::off() {
	LED_Controller_Set(&(this->controller.ledctrl), this->interface, 0, 0, 0);
}

bool PLLEDController::PLLED::dim(float intensity) {
	LED_Controller_Set(&(this->controller.ledctrl), this->interface, 0, 0xffff, 0xffff * intensity);

	return true;
}

bool PLLEDController::PLLED::blink(uint32_t period_ms, uint32_t timeon_ms) {
	uint32_t period_in_ticks = period_ms * (this->controller.kFrequency / 1000);
	uint32_t timeon_in_ticks = timeon_ms * (this->controller.kFrequency / 1000);

	LED_Controller_Set(&(this->controller.ledctrl), this->interface, 0, period_in_ticks, timeon_in_ticks);

	return true;
}

bool PLLEDController::PLLED::pulse(uint32_t period_ms) {
	uint32_t period_in_ticks = period_ms * (this->controller.kFrequency / 1000);

	LED_Controller_Set(&(this->controller.ledctrl), this->interface, 1, period_in_ticks, 0);

	return true;
}

#endif

