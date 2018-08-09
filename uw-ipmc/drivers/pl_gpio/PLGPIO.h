/*
 * PLGPIO.h
 *
 *  Created on: Aug 8, 2018
 *      Author: mpv
 */

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_PL_GPIO_PLGPIO_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_PL_GPIO_PLGPIO_H_

#include <FreeRTOS.h>
#include "xgpio.h"

/**
 * Wraps the low level C driver for Xilinx GPIO IP
 * TODO: Needs to be expanded for other cases
 */
class PL_GPIO {
public:
	/**
	 * Create a PL based GPIO interface.
	 * @param DeviceId The device ID, normally XPAR_AXI_GPIO_<>
	 * @param IntrId The device interrupt ID from GIC, normally XPAR_FABRIC_AXI_GPIO_<>
	 **/
	PL_GPIO(uint16_t DeviceId, uint32_t IntrId);
	virtual ~PL_GPIO();

	enum Channel {
		GPIO_CHANNEL1 = 1,
		GPIO_CHANNEL2 = 2
	};

	/**
	 * Get the bus value for target channel or default channel 1.
	 * @param c The channel number.
	 * @return Current bus value.
	 */
	inline uint32_t getBusValue(Channel c = GPIO_CHANNEL1) {
		return XGpio_DiscreteRead(&(this->Gpio), c);
	};

	/**
	 * Check if the current pin/wire is set in a channel
	 * @param pin The pin number
	 * @param c The channel number.
	 * @return true if set, false if unset
	 */
	inline bool isPinSet(uint32_t pin, Channel c = GPIO_CHANNEL1) {
		return (getBusValue(c) >> pin) & 0x1;
	};

	///! Set the IRQ callback when the value of a channel changes.
	inline void setIRQCallback(void (func)(uint32_t c)) { this->callback = func; };

	///! Check if the IP supports interrupts
	inline bool supportsInterrupts() { return this->Gpio.InterruptPresent; };

private:
	XGpio Gpio; ///< Internal use only.
	uint32_t IntrId; ///< Internal use only.
	void (*callback)(uint32_t); ///< Internal use only.

	///! Internal use only.
	static void InterruptHandler(PL_GPIO *gpio);

};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_PL_GPIO_PLGPIO_H_ */
