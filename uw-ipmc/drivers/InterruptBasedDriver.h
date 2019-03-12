/*
 * InterruptBasedDriver.h
 *
 *  Created on: Aug 20, 2018
 *      Author: mpv
 */

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_INTERRUPTBASEDDRIVER_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_INTERRUPTBASEDDRIVER_H_

#include <stdint.h>

// TODO: Expand to allow both GIC and INTC.

/**
 * Base class for a driver that is interrupt driven.
 * Includes the declaration of the interrupt handler and the implementation
 * of the connect function that needs to be called to enabled interrupts.
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
	 * @param intr Set the interrupt ID and connect.
	 */
	void connectInterrupt(uint32_t intr);

	/**
	 * Only required if constructor was empty.
	 * @param intr Set the interrupt ID and connect.
	 * @param trigger Trigger level to apply.
	 */
	void connectInterrupt(uint32_t intr, uint8_t trigger);

	///! Return true if interrupt is connected, false otherwise.
	inline bool isInterruptConnected() { return this->connected; };

	///! Masks the interrupts associated with the driver. Use when entering localized critical sections.
	void disableInterrupts();

	///! Unmasks the interrupts associated with the driver. Use when exiting localized critical sections.
	void enableInterrupts();

protected:
	///! Interrupt handler
	virtual void _InterruptHandler() = 0;

private:
	///! Internal use. Interrupt wrapper.
	inline static void _InterruptWrapper(void *p) {
		((InterruptBasedDriver*)p)->_InterruptHandler();
	}

	///! Internal use. Connect the interrupt handler to the interrupt.
	void connectInterrupt();

	///! Internal use. Change the interrupt trigger level.
	void setTriggerLevel(uint8_t trigger);

	uint32_t intr; ///< The interrupt ID.
	bool connected; ///< true if interrupt is connected.
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_INTERRUPTBASEDDRIVER_H_ */
