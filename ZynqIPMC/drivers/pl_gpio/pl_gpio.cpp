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

#include <drivers/pl_gpio/pl_gpio.h>
#include <libs/except.h>

// Only include driver if PL GPIO is detected in the BSP.
#if XSDK_INDEXING || __has_include("xgpio.h")

PLGPIO::PLGPIO(PLGPIO::BusChannel channel, uint16_t device_id)
: GPIO(), InterruptBasedDriver(), kChannel(channel), callback(nullptr) {
	// Initialize the Gpio driver so that it's ready to use.
	if (XST_SUCCESS != XGpio_Initialize(&(this->gpio), device_id)) {
		throw except::hardware_error("Unable to initialize PLGPIO(device_id=" + std::to_string(device_id) + ")");
	}

	// Perform a self-test to ensure that the hardware was built correctly.
	if (XST_SUCCESS != XGpio_SelfTest(&(this->gpio))) {
		throw except::hardware_error("Unable to initialize PLGPIO(device_id=" + std::to_string(device_id) + ")");
	}

	this->mutex = xSemaphoreCreateMutex();
	configASSERT(this->mutex);
}

PLGPIO::PLGPIO(PLGPIO::BusChannel channel, uint16_t device_id, uint32_t intr_id)
: GPIO(), InterruptBasedDriver(), kChannel(channel), callback(nullptr) {
	// Initialize the Gpio driver so that it's ready to use.
	if (XST_SUCCESS != XGpio_Initialize(&(this->gpio), device_id)) {
		throw except::hardware_error("Unable to initialize PLGPIO(device_id=" + std::to_string(device_id) + ")");
	}

	// Perform a self-test to ensure that the hardware was built correctly.
	if (XST_SUCCESS != XGpio_SelfTest(&(this->gpio))) {
		throw except::hardware_error("Unable to initialize PLGPIO(device_id=" + std::to_string(device_id) + ")");
	}

	if (this->isInterruptConnected()) {
		this->connectInterrupt(intr_id, 0x03);

		XGpio_InterruptEnable(&(this->gpio), XGPIO_IR_MASK);
		XGpio_InterruptGlobalEnable(&(this->gpio));

		this->enableInterrupts();
	}

	this->mutex = xSemaphoreCreateMutex();
	configASSERT(this->mutex);
}

PLGPIO::~PLGPIO() {
	vSemaphoreDelete(this->mutex);
}

uint32_t PLGPIO::getBusDirection() const {
	return XGpio_GetDataDirection(&(this->gpio), this->kChannel);
}

void PLGPIO::setBusDirection(uint32_t dir) {
	XGpio_SetDataDirection(&(this->gpio), this->kChannel, dir);
}

void PLGPIO::setPinDirection(uint32_t pin, bool input) {
	MutexGuard<false>(this->mutex);
	uint32_t d = this->getBusDirection();
	if (input) d |= (1 << pin);
	else d &= ~(1 << pin);
	this->setBusDirection(d);
}

uint32_t PLGPIO::getBusValue() const {
	return XGpio_DiscreteRead(&(this->gpio), this->kChannel);
}

void PLGPIO::setBusValue(uint32_t v) {
	XGpio_DiscreteWrite(&(this->gpio), this->kChannel, v);
}

void PLGPIO::setPin(uint32_t b) {
	XGpio_DiscreteSet(&(this->gpio), this->kChannel, 1 << b);
}

void PLGPIO::clearPin(uint32_t b) {
	XGpio_DiscreteClear(&(this->gpio), this->kChannel, 1 << b);
}

void PLGPIO::setIRQCallback(std::function<void(uint32_t)> func) {
	CriticalGuard critical(true); // We can't have this assignment interrupted, but it's not atomic
	this->callback = func;
};

void PLGPIO::_InterruptHandler() {
	/* Clear the Interrupt */
	XGpio_InterruptClear(&(this->gpio), XGPIO_IR_MASK);

	if (this->callback) {
		this->callback(0);
	}
}

#endif
