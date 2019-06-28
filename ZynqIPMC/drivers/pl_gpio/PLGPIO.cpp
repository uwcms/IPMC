/*
 * PLGPIO.cpp
 *
 *  Created on: Aug 8, 2018
 *      Author: mpv
 */

#include "PLGPIO.h"
#include "xparameters.h"
#include <libs/printf.h>
#include <libs/except.h>
#include <libs/ThreadingPrimitives.h>

PL_GPIO::PL_GPIO(PL_GPIO::BusChannel Channel, uint16_t DeviceId)
: GPIO(), InterruptBasedDriver(), Channel(Channel), callback(nullptr) {
	// Initialize the Gpio driver so that it's ready to use.
	if (XST_SUCCESS != XGpio_Initialize(&(this->Gpio), DeviceId))
		throw except::hardware_error(stdsprintf("Unable to initialize PL_GPIO(%hu)", DeviceId));

	// Perform a self-test to ensure that the hardware was built correctly.
	if (XST_SUCCESS != XGpio_SelfTest(&(this->Gpio)))
		throw except::hardware_error(stdsprintf("Self-test failed for PL_GPIO(%hu)", DeviceId));
}

PL_GPIO::PL_GPIO(PL_GPIO::BusChannel Channel, uint16_t DeviceId, uint32_t IntrId)
: GPIO(), InterruptBasedDriver(), Channel(Channel), callback(nullptr) {
	// Initialize the Gpio driver so that it's ready to use.
	if (XST_SUCCESS != XGpio_Initialize(&(this->Gpio), DeviceId))
		throw except::hardware_error(stdsprintf("Unable to initialize PL_GPIO(%hu, %lu)", DeviceId, IntrId));

	// Perform a self-test to ensure that the hardware was built correctly.
	if (XST_SUCCESS != XGpio_SelfTest(&(this->Gpio)))
		throw except::hardware_error(stdsprintf("Self-test failed for PL_GPIO(%hu)", DeviceId));

	if (this->Gpio.InterruptPresent) {
		this->connectInterrupt(IntrId, 0x03);

		XGpio_InterruptEnable(&(this->Gpio), XGPIO_IR_MASK);
		XGpio_InterruptGlobalEnable(&(this->Gpio));

		this->enableInterrupts();
	}
}

uint32_t PL_GPIO::getDirection() {
	return XGpio_GetDataDirection(&(this->Gpio), this->Channel);
}

void PL_GPIO::setDirection(uint32_t d) {
	XGpio_SetDataDirection(&(this->Gpio), this->Channel, d);
}

void PL_GPIO::setBitDirection(uint32_t b, bool input) {
	MutexGuard<false>(this->mutex);
	uint32_t d = this->getDirection();
	if (input) d |= (1 << b);
	else d &= ~(1 << b);
	this->setDirection(d);
}

uint32_t PL_GPIO::getBus() {
	return XGpio_DiscreteRead(&(this->Gpio), this->Channel);
}

void PL_GPIO::setBus(uint32_t v) {
	XGpio_DiscreteWrite(&(this->Gpio), this->Channel, v);
}

void PL_GPIO::setPin(uint32_t b) {
	XGpio_DiscreteSet(&(this->Gpio), this->Channel, 1 << b);
}

void PL_GPIO::clearPin(uint32_t b) {
	XGpio_DiscreteClear(&(this->Gpio), this->Channel, 1 << b);
}

void PL_GPIO::setIRQCallback(std::function<void(uint32_t)> func) {
	CriticalGuard critical(true); // We can't have this assignment interrupted, but it's not atomic
	this->callback = func;
};

void PL_GPIO::_InterruptHandler() {
	/* Clear the Interrupt */
	XGpio_InterruptClear(&(this->Gpio), XGPIO_IR_MASK);

	if (this->callback) {
		this->callback(0);
	}
}
