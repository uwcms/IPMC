/*
 * InterruptBasedDriver.cpp
 *
 *  Created on: Aug 20, 2018
 *      Author: mpv
 */

#include "InterruptBasedDriver.h"
#include <FreeRTOS.h>
#include <xscugic.h>
#include <libs/except.h>

extern XScuGic xInterruptController;

InterruptBasedDriver::InterruptBasedDriver()
: intr(0), connected(false) {
}

InterruptBasedDriver::InterruptBasedDriver(uint32_t intr)
: intr(intr) {
	this->connectInterrupt();
}

InterruptBasedDriver::InterruptBasedDriver(uint32_t intr, uint8_t trigger)
: intr(intr) {
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
	if (XST_SUCCESS !=
		XScuGic_Connect(&xInterruptController, this->intr, InterruptBasedDriver::_InterruptWrapper, (void*)this))
		throw except::hardware_error("Unable to connect handler to interrupt controller.");
	//this->enableInterrupts();

	this->connected = true;
}

void InterruptBasedDriver::disableInterrupts() {
	XScuGic_Disable(&xInterruptController, this->intr);
}

void InterruptBasedDriver::enableInterrupts() {
	if (!this->connected) {
		throw except::hardware_error("Driver does not have an interrupt connected to it.");
	}
	XScuGic_Enable(&xInterruptController, this->intr);
}

void InterruptBasedDriver::setTriggerLevel(uint8_t trigger) {
	uint8_t priority, dummy;
	XScuGic_GetPriorityTriggerType(&xInterruptController, this->intr, &priority, &dummy);
	XScuGic_SetPriorityTriggerType(&xInterruptController, this->intr, priority, trigger);
}
