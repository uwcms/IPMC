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

#ifndef SRC_COMMON_ZYNQIPMC_DRIVERS_INTERRUPT_BASED_DRIVER_H_
#define SRC_COMMON_ZYNQIPMC_DRIVERS_INTERRUPT_BASED_DRIVER_H_

#include <stdint.h>

// TODO: Expand to allow both GIC and INTC.

/**
 * Base class for a driver that is interrupt driven.
 * Includes the declaration of the interrupt handler and the implementation
 * of the connect function that needs to be called to enabled interrupts.
 *
 * InterruptBasedDriver::enableInterrupts needs to be called when driver
 * finishes initialization in order to enable interrupts.
 */
class InterruptBasedDriver {
protected:
	/**
	 * Default constructor. No interrupt ID is provided so interrupt won't be
	 * configured by default.
	 */
	InterruptBasedDriver();

	/**
	 * Construct the interrupt based driver.
	 * @param intr Interrupt ID to connect to.
	 */
	InterruptBasedDriver(uint32_t intr);

	/**
	 * Construct the interrupt based driver with non-standard trigger.
	 * @param intr Interrupt ID to connect to.
	 * @param trigger The desired trigger level.
	 */
	InterruptBasedDriver(uint32_t intr, uint8_t trigger);
	virtual ~InterruptBasedDriver();

	/**
	 * Only required if constructor was empty.
	 * @param intr Set the interrupt ID and connect it.
	 * @throw except::hardware_error if failed to connect handler.
	 */
	void connectInterrupt(uint32_t intr);

	/**
	 * Only required if constructor was empty.
	 * @param intr Set the interrupt ID and connect.
	 * @param trigger Trigger level to apply.
	 */
	void connectInterrupt(uint32_t intr, uint8_t trigger);

	//! Return true if interrupt is connected, false otherwise.
	inline bool isInterruptConnected() { return this->connected; };

	//! Masks the interrupts associated with the driver. Use when entering localized critical sections.
	void disableInterrupts();

	/**
	 * Unmasks the interrupts associated with the driver. Use when exiting localized critical sections.
	 * @throw std::runtime_error if interrupt isn't connected.
	 */
	void enableInterrupts();

protected:
	//! Interrupt handler.
	virtual void _InterruptHandler() = 0;

private:
	//! Internal use. Interrupt wrapper.
	inline static void _InterruptWrapper(void *p) {
		((InterruptBasedDriver*)p)->_InterruptHandler();
	}

	//! Internal use. Connect the interrupt handler to the interrupt.
	void connectInterrupt();

	//! Internal use. Change the interrupt trigger level.
	void setTriggerLevel(uint8_t trigger);

	uint32_t intr;	///< The interrupt ID.
	bool connected;	///< true if interrupt is connected.
};

#endif /* SRC_COMMON_ZYNQIPMC_DRIVERS_INTERRUPTBASEDDRIVER_H_ */
