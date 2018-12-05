/*
 * PLGPIO.h
 *
 *  Created on: Aug 8, 2018
 *      Author: mpv
 */

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_PL_GPIO_PLGPIO_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_PL_GPIO_PLGPIO_H_

#include <FreeRTOS.h>
#include <task.h>
#include <drivers/InterruptBasedDriver.h>
#include "xgpio.h"
#include <functional>

// TODO: Have a generic class GPIO where channels do not exists so that PL and PS GPIOs can coexist.

/**
 * Wraps the low level C driver for Xilinx AXI GPIO IP.
 */
class PL_GPIO : protected InterruptBasedDriver {
public:
	/**
	 * Create a PL based GPIO interface without interrupt support.
	 * @param DeviceId The device ID, normally XPAR_AXI_GPIO_<>.
	 */
	PL_GPIO(uint16_t DeviceId);

	/**
	 * Create a PL based GPIO interface with interrupt support.
	 * IP must support interrupts as well.
	 * @param DeviceId The device ID, normally XPAR_AXI_GPIO_<>.
	 * @param IntrId The device interrupt ID from GIC, normally XPAR_FABRIC_AXI_GPIO_<>.
	 **/
	PL_GPIO(uint16_t DeviceId, uint32_t IntrId);
	~PL_GPIO() {};

	///! Possible channels, GPIO_CHANNEL1 is the default in all operation.
	enum Channel {
		GPIO_CHANNEL1 = 1,
		GPIO_CHANNEL2 = 2
	};

	/**
	 * Set the direction of all pins in a channel.
	 * @param d Each bit represents the direction of a pin. Bits set
	 * to 1 will become inputs while bits set to 0 will become outputs.
	 * @param c GPIO channel, default is channel 1.
	 */
	inline void setDirection(uint32_t d, Channel c = GPIO_CHANNEL1) {
		XGpio_SetDataDirection(&(this->Gpio), c, d);
	}

	/**
	 * Individually set the direction of pin in a channel.
	 * @param b The pin number (from 0 to 31).
	 * @param input true if pin should become input, false if output.
	 * @param c GPIO channel, default is channel 1.
	 */
	inline void setBitDirection(uint32_t b, bool input, Channel c = GPIO_CHANNEL1) {
		uint32_t d = this->getDirection(c);
		if (input) d |= (1 << b);
		else d &= ~(1 << b);
		this->setDirection(d, c);
	}

	/**
	 * Set a single pin to input.
	 * @param b The pin number (from 0 to 31).
	 * @param c GPIO channel, default is channel 1.
	 * @warning The maximum pin number is based on the number of IOs defined in the IP.
	 */
	inline void setPinToInput(uint32_t b, Channel c = GPIO_CHANNEL1) {
		this->setBitDirection(b, true, c);
	}

	/**
	 * Set a single pin to o.
	 * @param b The pin number (from 0 to 31).
	 * @param c GPIO channel, default is channel 1.
	 * @warning The maximum pin number is based on the number of IOs defined in the IP.
	 */
	inline void setPinToOutput(uint32_t b, Channel c = GPIO_CHANNEL1) {
		this->setBitDirection(b, false, c);
	}

	/**
	 * Get the whole channel direction.
	 * @param c GPIO channel, default is channel 1.
	 * @return Number where each bit corresponds to a pin. Bits set to 1 correspond to
	 * inputs while bits set to 0 correspond to outputs.
	 */
	inline uint32_t getDirection(Channel c = GPIO_CHANNEL1) {
		return XGpio_GetDataDirection(&(this->Gpio), c);
	}

	/**
	 * Set the value of a channel whos pins are ouputs. Pins configured as input
	 * are not affected.
	 * @param v Value to apply where each bit corresponds to one of the pins.
	 * @param c GPIO channel, default is channel 1.
	 */
	inline void setChannel(uint32_t v, Channel c = GPIO_CHANNEL1) {
		XGpio_DiscreteWrite(&(this->Gpio), c, v);
	}

	/**
	 * Set a single pin to high. Pin must be configured as output for changes to take effect.
	 * @param b The pin number (from 0 to 31).
	 * @param c GPIO channel, default is channel 1.
	 * @warning The maximum pin number is based on the number of IOs defined in the IP.
	 */
	inline void setPin(uint32_t b, Channel c = GPIO_CHANNEL1) {
		XGpio_DiscreteSet(&(this->Gpio), c, 1 << b);
	}

	/**
	 * Set a single pin to low. Pin must be configured as output for changes to take effect.
	 * @param b The pin number (from 0 to 31).
	 * @param c GPIO channel, default is channel 1.
	 * @warning The maximum pin number is based on the number of IOs defined in the IP.
	 */
	inline void clearPin(uint32_t b, Channel c = GPIO_CHANNEL1) {
		XGpio_DiscreteClear(&(this->Gpio), c, 1 << b);
	}

	/**
	 * Get the bus value for target channel or default channel 1.
	 * @param c The channel number.
	 * @return Current bus value.
	 */
	inline uint32_t getChannel(Channel c = GPIO_CHANNEL1) {
		return XGpio_DiscreteRead(&(this->Gpio), c);
	};

	/**
	 * Check if the current pin/wire is set in a channel
	 * @param pin The pin number
	 * @param c The channel number.
	 * @return true if set, false if unset
	 */
	inline bool isPinSet(uint32_t pin, Channel c = GPIO_CHANNEL1) {
		return (this->getChannel(c) >> pin) & 0x1;
	};

	///! Set the IRQ callback when the value of a channel changes.
	void setIRQCallback(std::function<void(uint32_t)> func);

	///! Check if the IP supports interrupts
	inline bool supportsInterrupts() { return this->Gpio.InterruptPresent; };

protected:
	void _InterruptHandler();

private:
	XGpio Gpio; ///< Internal use only.
	std::function<void(uint32_t)> callback; ///< Internal use only.
};


// TODO: Move
class ResetPin {
public:
	virtual void release() = 0;
	virtual void assert() = 0;
	virtual void deassert() = 0;
	virtual void toggle(uint32_t ms = 100) = 0;
};

class NegResetPin : public ResetPin {
public:
	NegResetPin(PL_GPIO &gpio, uint32_t pin) : gpio(gpio), pin(pin) {
		// Default value should be configured in the IP.
	};

	inline void release() { this->gpio.setPinToInput(this->pin); }
	inline void assert() { this->gpio.setPinToOutput(this->pin); this->gpio.clearPin(this->pin); }
	inline void deassert() { this->gpio.setPinToOutput(this->pin); this->gpio.setPin(this->pin); }
	inline void toggle(uint32_t ms) { this->assert(); vTaskDelay(pdMS_TO_TICKS(ms)); this->deassert(); }

private:
	PL_GPIO &gpio;
	uint32_t pin;
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_PL_GPIO_PLGPIO_H_ */
