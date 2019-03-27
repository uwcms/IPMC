/*
 * GPIO.h
 *
 *  Created on: Mar 26, 2019
 *      Author: mpv
 */

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_GENERICS_GPIO_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_GENERICS_GPIO_H_

#include <FreeRTOS.h>
#include <semphr.h>
#include <libs/ThreadingPrimitives.h>

/**
 * Abstract class for GPIOs with base functions for bus or individual pin operation declared.
 */
class GPIO {
public:
	GPIO() {
		this->mutex = xSemaphoreCreateMutex();
		configASSERT(this->mutex);
	};

	virtual ~GPIO() {
		vSemaphoreDelete(this->mutex);
	};

	/**
	 * Get the whole bus direction.
	 * @return Number where each bit corresponds to a pin. Bits set to 1 correspond to
	 * inputs while bits set to 0 correspond to outputs.
	 */
	virtual uint32_t getDirection() = 0;

	/**
	 * Set the direction of all pins in the bus.
	 * @param d Each bit represents the direction of a pin. Bits set
	 * to 1 will become inputs while bits set to 0 will become outputs.
	 */
	virtual void setDirection(uint32_t d) = 0;

	/**
	 * Individually set the direction of pin in the bus.
	 * @param b The pin number (from 0 to 31).
	 * @param input true if pin should become input, false if output.
	 */
	virtual void setBitDirection(uint32_t b, bool input) = 0;

	/**
	 * Set a single pin to input.
	 * @param b The pin number (from 0 to 31).
	 * @warning The maximum pin number is based on the number of IOs defined in the IP.
	 */
	inline void setPinToInput(uint32_t b) {
		this->setBitDirection(b, true);
	}

	/**
	 * Set a single pin to o.
	 * @param b The pin number (from 0 to 31).
	 * @warning The maximum pin number is based on the number of IOs defined in the IP.
	 */
	inline void setPinToOutput(uint32_t b) {
		this->setBitDirection(b, false);
	}

	/**
	 * Get the bus value.
	 * @return Current bus value.
	 */
	virtual uint32_t getBus() = 0;

	/**
	 * Set the value of the bus whos pins are outputs. Pins configured as input
	 * are not affected.
	 * @param v Value to apply where each bit corresponds to one of the pins.
	 */
	virtual void setBus(uint32_t v) = 0;

	/**
	 * Set the value of the bus while masking a set of bits.
	 * @param v Value to apply where each bit of the bus.
	 * @param mask Bits that will be masked from changing.
	 */
	inline void setBusMask(uint32_t v, uint32_t mask) {
		//MutexGuard<false>(this->mutex);
		uint32_t c = this->getBus();
		c = (c & ~mask) | (v & mask);
		this->setBus(c);
	}

	/**
	 * Set a single pin to low. Pin must be configured as output for changes to take effect.
	 * @param b The pin number (from 0 to 31).
	 * @warning The maximum pin number is based on the number of IOs defined in the IP.
	 */
	virtual void clearPin(uint32_t b) = 0;

	/**
	 * Set a single pin to high. Pin must be configured as output for changes to take effect.
	 * @param b The pin number (from 0 to 31).
	 * @warning The maximum pin number is based on the number of IOs defined in the IP.
	 */
	virtual void setPin(uint32_t b) = 0;

	/**
	 * Check if the current pin/wire is set in the bus
	 * @param pin The pin number
	 * @return true if set, false if unset
	 */
	inline bool isPinSet(uint32_t pin) {
		return (this->getBus() >> pin) & 0x1;
	};

protected:
	SemaphoreHandle_t mutex;
};

class ResetPin {
public:
	virtual void release() = 0;
	virtual void assert() = 0;
	virtual void deassert() = 0;
	virtual void toggle(uint32_t ms = 100) = 0;
};

class NegResetPin : public ResetPin {
public:
	NegResetPin(GPIO &gpio, uint32_t pin) : gpio(gpio), pin(pin) {
		// Default value should be configured in the IP.
	};

	inline void release() { this->gpio.setPinToInput(this->pin); }
	inline void assert() { this->gpio.setPinToOutput(this->pin); this->gpio.clearPin(this->pin); }
	inline void deassert() { this->gpio.setPinToOutput(this->pin); this->gpio.setPin(this->pin); }
	inline void toggle(uint32_t ms) { this->assert(); vTaskDelay(pdMS_TO_TICKS(ms)); this->deassert(); }

private:
	GPIO &gpio;
	uint32_t pin;
};



#endif /* SRC_COMMON_UW_IPMC_DRIVERS_GENERICS_GPIO_H_ */
