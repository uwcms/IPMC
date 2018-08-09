/*
 * PLGPIO.cpp
 *
 *  Created on: Aug 8, 2018
 *      Author: mpv
 */

#include "PLGPIO.h"
#include <xscugic.h>
#include "xparameters.h"

extern XScuGic xInterruptController;

void PL_GPIO::InterruptHandler(PL_GPIO *gpio) {
	/* Clear the Interrupt */
	XGpio_InterruptClear(&(gpio->Gpio), XGPIO_IR_MASK);

	if (gpio->callback) {
		gpio->callback(0);
	}
}

PL_GPIO::PL_GPIO(uint16_t DeviceId, uint32_t IntrId)
: IntrId(IntrId), callback(nullptr) {
	// Initialize the Gpio driver so that it's ready to use.
	configASSERT(XST_SUCCESS == XGpio_Initialize(&(this->Gpio), DeviceId));

	// Perform a self-test to ensure that the hardware was built correctly.
	configASSERT(XST_SUCCESS == XGpio_SelfTest(&(this->Gpio)));

	if (this->Gpio.InterruptPresent) {
		// Set proper interrupt priorities
		uint8_t priority, trigger;
		XScuGic_GetPriorityTriggerType(&xInterruptController, this->IntrId, &priority, &trigger);
		XScuGic_SetPriorityTriggerType(&xInterruptController, this->IntrId, priority, 0x3);

		// Connect the driver to the interrupt subsystem.
		configASSERT(XST_SUCCESS ==
				XScuGic_Connect(&xInterruptController, this->IntrId, (Xil_InterruptHandler)InterruptHandler, (void*)this));
		XScuGic_Enable(&xInterruptController, this->IntrId);

		XGpio_InterruptEnable(&(this->Gpio), XGPIO_IR_MASK);
		XGpio_InterruptGlobalEnable(&(this->Gpio));
	}
}

PL_GPIO::~PL_GPIO() {
	if (this->Gpio.InterruptPresent) {
		// Disconnect the interrupt
		XScuGic_Disable(&xInterruptController, this->IntrId);
		XScuGic_Disconnect(&xInterruptController, this->IntrId);
	}
}
