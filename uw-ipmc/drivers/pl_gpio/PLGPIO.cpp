/*
 * PLGPIO.cpp
 *
 *  Created on: Aug 8, 2018
 *      Author: mpv
 */

#include "PLGPIO.h"
#include "xparameters.h"

PL_GPIO::PL_GPIO(uint16_t DeviceId)
: InterruptBasedDriver(), callback(nullptr) {
	// Initialize the Gpio driver so that it's ready to use.
	configASSERT(XST_SUCCESS == XGpio_Initialize(&(this->Gpio), DeviceId));

	// Perform a self-test to ensure that the hardware was built correctly.
	configASSERT(XST_SUCCESS == XGpio_SelfTest(&(this->Gpio)));
}

PL_GPIO::PL_GPIO(uint16_t DeviceId, uint32_t IntrId)
: InterruptBasedDriver(), callback(nullptr) {
	// Initialize the Gpio driver so that it's ready to use.
	configASSERT(XST_SUCCESS == XGpio_Initialize(&(this->Gpio), DeviceId));

	// Perform a self-test to ensure that the hardware was built correctly.
	configASSERT(XST_SUCCESS == XGpio_SelfTest(&(this->Gpio)));

	if (this->Gpio.InterruptPresent) {
		this->connectInterrupt(IntrId, 0x03);

		XGpio_InterruptEnable(&(this->Gpio), XGPIO_IR_MASK);
		XGpio_InterruptGlobalEnable(&(this->Gpio));
	}
}

void PL_GPIO::_InterruptHandler() {
	/* Clear the Interrupt */
	XGpio_InterruptClear(&(this->Gpio), XGPIO_IR_MASK);

	if (this->callback) {
		this->callback(0);
	}
}
