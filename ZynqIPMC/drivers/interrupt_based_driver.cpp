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

#include <drivers/interrupt_based_driver.h>
#include <FreeRTOS.h>
#include <xscugic.h>
#include <libs/except.h>

//! Global variable defined inside FreeRTOS code.
extern XScuGic xInterruptController;

InterruptBasedDriver::InterruptBasedDriver() :
intr(0), connected(false) {
}

InterruptBasedDriver::InterruptBasedDriver(uint32_t intr) :
intr(intr) {
	this->connectInterrupt();
}

InterruptBasedDriver::InterruptBasedDriver(uint32_t intr, uint8_t trigger) :
intr(intr) {
	this->setTriggerLevel(trigger);
	this->connectInterrupt();
}

InterruptBasedDriver::~InterruptBasedDriver() {
	// Disconnect the interrupt
	if (this->isInterruptConnected()) {
		XScuGic_Disable(&xInterruptController, this->intr);
		XScuGic_Disconnect(&xInterruptController, this->intr);
	}
}

void InterruptBasedDriver::connectInterrupt(uint32_t intr) {
	this->intr = intr;

	this->connectInterrupt();
}

void InterruptBasedDriver::connectInterrupt(uint32_t intr, uint8_t trigger) {
	this->intr = intr;

	this->setTriggerLevel(trigger);
	this->connectInterrupt();
}

void InterruptBasedDriver::connectInterrupt() {
	if (XST_SUCCESS != XScuGic_Connect(&xInterruptController, this->intr, InterruptBasedDriver::_InterruptWrapper, (void*)this)) {
		throw except::hardware_error("Unable to connect handler to interrupt controller.");
	}

	this->connected = true;
}

void InterruptBasedDriver::disableInterrupts() {
	XScuGic_Disable(&xInterruptController, this->intr);
}

void InterruptBasedDriver::enableInterrupts() {
	if (!this->connected) {
		throw std::runtime_error("Driver does not have an interrupt connected to it.");
	}
	XScuGic_Enable(&xInterruptController, this->intr);
}

void InterruptBasedDriver::setTriggerLevel(uint8_t trigger) {
	uint8_t priority, dummy;
	XScuGic_GetPriorityTriggerType(&xInterruptController, this->intr, &priority, &dummy);
	XScuGic_SetPriorityTriggerType(&xInterruptController, this->intr, priority, trigger);
}
