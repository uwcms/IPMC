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

// Only include driver if PS GPIO is detected in the BSP.
#if XSDK_INDEXING || __has_include("xgpiops.h")

#include "ps_gpio.h"

PSGPIO::PSGPIO(uint16_t device_id, std::vector<uint8_t> pins) : GPIO(), kPinCount(pins.size()), pins(nullptr) {
	if (this->kPinCount == 0) {
		throw std::out_of_range("Cannot create an empty bus for PSGPIO(device_id=" + std::to_string(device_id) + ")");
	}

	if (this->kPinCount > 32) {
		throw std::out_of_range("Cannot create bus with more than 32 pins for PSGPIO(device_id=" + std::to_string(device_id) + ")");
	}

	// Verify pins
	for (size_t i = 0; i < this->kPinCount; i++) {
		if (pins[i] > 117) {
			throw std::out_of_range("Pin " + std::to_string(i) + " in out-of-range for PSGPIO(device_id=" + std::to_string(device_id) + ")");
		}
	}

	XGpioPs_Config *config = XGpioPs_LookupConfig(device_id);
	if (!config) {
		throw except::hardware_error("Unable retrieve configuration for PLGPIO(device_id=" + std::to_string(device_id) + ")");
	}

	// Initialize the Gpio driver so that it's ready to use.
	if (XST_SUCCESS != XGpioPs_CfgInitialize(&(this->gpiops), config, config->BaseAddr)) {
		throw except::hardware_error("Unable to initialize PSGPIO(device_id=" + std::to_string(device_id) + ")");
	}

	// Perform a self-test to ensure that the hardware was built correctly.
	if (XST_SUCCESS != XGpioPs_SelfTest(&(this->gpiops))) {
		throw except::hardware_error("Unable to initialize PSGPIO(device_id=" + std::to_string(device_id) + ")");
	}

	this->pins = new PinInfo[this->kPinCount];
	if (!this->pins) throw std::runtime_error("Out-of-memory");

	for (size_t i = 0; i < this->kPinCount; i++) {
		XGpioPs_GetBankPin(pins[i], &(this->pins[i].bank), &(this->pins[i].pin));
	}

	this->mutex = xSemaphoreCreateMutex();
	configASSERT(this->mutex);
}

PSGPIO::~PSGPIO() {
	if (this->pins) delete this->pins;

	vSemaphoreDelete(this->mutex);
}

uint32_t PSGPIO::getBusDirection() const {
	uint32_t r = 0, t = 0;
	uint8_t bank = 0xff;

	MutexGuard<false>(this->mutex);

	for (size_t i = 0; i < this->kPinCount; i++) {
		if (this->pins[i].bank != bank) {
			// Avoid reading the same bank several times
			bank = this->pins[i].bank;
			t = XGpioPs_GetDirection(&(this->gpiops), bank);
		}

		// This is different than PL_GPIO, pins set as outputs show up as 1,
		// so invert it to have everything consistent with interface
		if (!(t & (1 << this->pins[i].pin))) r |= (1 << i);
	}

	return r;
}

void PSGPIO::setBusDirection(uint32_t d) {
	MutexGuard<false>(this->mutex);

	for (size_t i = 0; i < this->kPinCount; i++) {
		uint32_t dir = XGpioPs_GetDirection(&(this->gpiops), this->pins[i].bank);
		if (!(d & (1 << i))) dir |= (1 << this->pins[i].pin);
		else dir &= ~(1 << this->pins[i].pin);
		XGpioPs_SetDirection(&(this->gpiops), this->pins[i].bank, dir);
	}

	for (size_t i = 0; i < this->kPinCount; i++) {
		uint32_t en = XGpioPs_GetOutputEnable(&(this->gpiops), this->pins[i].bank);
		if (!(d & (1 << i))) en |= (1 << this->pins[i].pin);
		else en &= ~(1 << this->pins[i].pin);
		XGpioPs_SetOutputEnable(&(this->gpiops), this->pins[i].bank, en);
	}
}

void PSGPIO::setPinDirection(uint32_t pin, bool input) {
	if (pin >= this->kPinCount) return;

	MutexGuard<false>(this->mutex);

	uint32_t dir = XGpioPs_GetDirection(&(this->gpiops), this->pins[pin].bank);
	if (!input) dir |= 1 << this->pins[pin].pin;
	else dir &= ~(1 << this->pins[pin].pin);
	XGpioPs_SetDirection(&(this->gpiops), this->pins[pin].bank, dir);
}

uint32_t PSGPIO::getBusValue() const {
	uint32_t r = 0, t = 0;
	uint8_t bank = 0xff;

	MutexGuard<false>(this->mutex);

	for (size_t i = 0; i < this->kPinCount; i++) {
		if (this->pins[i].bank != bank) {
			// Avoid reading the same bank several times
			bank = this->pins[i].bank;
			t = XGpioPs_Read(&(this->gpiops), bank);
		}

		if (t & (1 << this->pins[i].pin)) r |= (1 << i);
	}

	return r;
}

void PSGPIO::setBusValue(uint32_t v) {
	MutexGuard<false>(this->mutex);

	for (size_t i = 0; i < this->kPinCount; i++) {
		uint32_t bus = XGpioPs_Read(&(this->gpiops), this->pins[i].bank);
		if (v & (1 << i)) bus |= (1 << this->pins[i].pin);
		else bus &= ~(1 << this->pins[i].pin);
		XGpioPs_Write(&(this->gpiops), this->pins[i].bank, bus);
	}
}

void PSGPIO::setPin(uint32_t pin) {
	if (pin >= this->kPinCount) return;

	MutexGuard<false>(this->mutex);

	uint32_t bus = XGpioPs_Read(&(this->gpiops), this->pins[pin].bank);
	bus |= (1 << this->pins[pin].pin);
	XGpioPs_Write(&(this->gpiops), this->pins[pin].bank, bus);
}

void PSGPIO::clearPin(uint32_t pin) {
	if (pin >= this->kPinCount) return;

	MutexGuard<false>(this->mutex);

	uint32_t bus = XGpioPs_Read(&(this->gpiops), this->pins[pin].bank);
	bus &= ~(1 << this->pins[pin].pin);
	XGpioPs_Write(&(this->gpiops), this->pins[pin].bank, bus);
}

#endif
