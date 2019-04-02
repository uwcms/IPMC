/*
 * PSGPIO.cpp
 *
 *  Created on: Aug 8, 2018
 *      Author: mpv
 */

#include "PSGPIO.h"
#include <libs/printf.h>

PS_GPIO::PS_GPIO(uint16_t DeviceId, std::vector<uint8_t> pins) : GPIO(), pinCount(0), pins(NULL) {
	XGpioPs_Config *config = XGpioPs_LookupConfig(DeviceId);
	if (!config) throw except::hardware_error(stdsprintf("Unable retrieve configuration for PS_GPIO(%hu)", DeviceId));

	// Initialize the Gpio driver so that it's ready to use.
	if (XST_SUCCESS != XGpioPs_CfgInitialize(&(this->GpioPs), config, config->BaseAddr))
		throw except::hardware_error(stdsprintf("Unable to initialize PS_GPIO(%hu)", DeviceId));

	// Perform a self-test to ensure that the hardware was built correctly.
	if (XST_SUCCESS != XGpioPs_SelfTest(&(this->GpioPs)))
		throw except::hardware_error(stdsprintf("Self-test failed for PS_GPIO(%hu)", DeviceId));

	this->pinCount = pins.size();

	if (this->pinCount == 0) {
		throw except::hardware_error("Cannot create an empty bus");
	}

	if (this->pinCount > 32) {
		throw except::hardware_error("Cannot create bus with more than 32 pins");
	}

	// Verify pins
	for (size_t i = 0; i < this->pinCount; i++) {
		if (pins[i] > 117)
			throw except::hardware_error(stdsprintf("Pin %d in PS_GPIO(%hu) is out-of-range", i, DeviceId));
	}

	this->pins = new pinInfo[this->pinCount];
	if (!this->pins) throw except::hardware_error("Out-of-memory");

	for (size_t i = 0; i < this->pinCount; i++) {
		XGpioPs_GetBankPin(pins[i], &(this->pins[i].bank), &(this->pins[i].pin));
	}
}

PS_GPIO::~PS_GPIO() {
	if (this->pins) delete this->pins;
}

uint32_t PS_GPIO::getDirection() {
	uint32_t r = 0, t = 0;
	uint8_t bank = 0xff;

	MutexGuard<false>(this->mutex);

	for (size_t i = 0; i < this->pinCount; i++) {
		if (this->pins[i].bank != bank) {
			// Avoid reading the same bank several times
			bank = this->pins[i].bank;
			t = XGpioPs_GetDirection(&(this->GpioPs), bank);
		}

		// This is different than PL_GPIO, pins set as outputs show up as 1,
		// so invert it to have everything consistent with interface
		if (!(t & (1 << this->pins[i].pin))) r |= (1 << i);
	}

	return r;
}

void PS_GPIO::setDirection(uint32_t d) {
	MutexGuard<false>(this->mutex);

	for (size_t i = 0; i < this->pinCount; i++) {
		uint32_t dir = XGpioPs_GetDirection(&(this->GpioPs), this->pins[i].bank);
		if (!(d & (1 << i))) dir |= (1 << this->pins[i].pin);
		else dir &= ~(1 << this->pins[i].pin);
		XGpioPs_SetDirection(&(this->GpioPs), this->pins[i].bank, dir);
	}

	for (size_t i = 0; i < this->pinCount; i++) {
		uint32_t en = XGpioPs_GetOutputEnable(&(this->GpioPs), this->pins[i].bank);
		if (!(d & (1 << i))) en |= (1 << this->pins[i].pin);
		else en &= ~(1 << this->pins[i].pin);
		XGpioPs_SetOutputEnable(&(this->GpioPs), this->pins[i].bank, en);
	}
}

void PS_GPIO::setBitDirection(uint32_t b, bool input) {
	if (b >= this->pinCount) return;

	MutexGuard<false>(this->mutex);

	uint32_t dir = XGpioPs_GetDirection(&(this->GpioPs), this->pins[b].bank);
	if (!input) dir |= 1 << this->pins[b].pin;
	else dir &= ~(1 << this->pins[b].pin);
	XGpioPs_SetDirection(&(this->GpioPs), this->pins[b].bank, dir);
}

uint32_t PS_GPIO::getBus() {
	uint32_t r = 0, t = 0;
	uint8_t bank = 0xff;

	MutexGuard<false>(this->mutex);

	for (size_t i = 0; i < this->pinCount; i++) {
		if (this->pins[i].bank != bank) {
			// Avoid reading the same bank several times
			bank = this->pins[i].bank;
			t = XGpioPs_Read(&(this->GpioPs), bank);
		}

		if (t & (1 << this->pins[i].pin)) r |= (1 << i);
	}

	return r;
}

void PS_GPIO::setBus(uint32_t v) {
	MutexGuard<false>(this->mutex);

	for (size_t i = 0; i < this->pinCount; i++) {
		uint32_t bus = XGpioPs_Read(&(this->GpioPs), this->pins[i].bank);
		if (v & (1 << i)) bus |= (1 << this->pins[i].pin);
		else bus &= ~(1 << this->pins[i].pin);
		XGpioPs_Write(&(this->GpioPs), this->pins[i].bank, bus);
	}
}

void PS_GPIO::setPin(uint32_t b) {
	if (b >= this->pinCount) return;

	MutexGuard<false>(this->mutex);

	uint32_t bus = XGpioPs_Read(&(this->GpioPs), this->pins[b].bank);
	bus |= (1 << this->pins[b].pin);
	XGpioPs_Write(&(this->GpioPs), this->pins[b].bank, bus);
}

void PS_GPIO::clearPin(uint32_t b) {
	if (b >= this->pinCount) return;

	MutexGuard<false>(this->mutex);

	uint32_t bus = XGpioPs_Read(&(this->GpioPs), this->pins[b].bank);
	bus &= ~(1 << this->pins[b].pin);
	XGpioPs_Write(&(this->GpioPs), this->pins[b].bank, bus);
}

