/*
 * PLLED.cpp
 *
 *  Created on: Jan 9, 2019
 *      Author: mpv
 */

#include <libs/ThreadingPrimitives.h>
#include <string>

#include "PLLED.h"

PL_LED::PL_LED(uint16_t deviceId, uint32_t plFrequency) :
plFrequency(plFrequency) {
	// Initialize the Gpio driver so that it's ready to use.
	if (XST_SUCCESS != LED_Controller_Initialize(&(this->ledController), deviceId))
		throw except::hardware_error("Unable to initialize PL_LED" + std::to_string(deviceId) + ")");
}

LED::LED(PL_LED &controller, uint32_t interface) :
controller(controller), interface(interface) {
	if (interface > controller.ledController.InterfaceCount)
		throw except::hardware_error("Out-of-range LED interface: " + std::to_string(interface));
}

void LED::on() {
	LED_Controller_Set(&(this->controller.ledController), this->interface, 0, 0, 1);
}

void LED::off() {
	LED_Controller_Set(&(this->controller.ledController), this->interface, 0, 0, 0);
}

void LED::dim(float intensity) {
	LED_Controller_Set(&(this->controller.ledController), this->interface, 0, 0xffff, 0xffff * intensity);
}

void LED::blink(uint32_t periodMs, uint32_t timeOnMs) {
	uint32_t periodInTicks = periodMs * (this->controller.plFrequency / 1000);
	uint32_t timeOnInTicks = timeOnMs * (this->controller.plFrequency / 1000);

	LED_Controller_Set(&(this->controller.ledController), this->interface, 0, periodInTicks, timeOnInTicks);
}

void LED::pulse(uint32_t periodMs) {
	uint32_t periodInTicks = periodMs * (this->controller.plFrequency / 1000);

	LED_Controller_Set(&(this->controller.ledController), this->interface, 1, periodInTicks, 0);
}
