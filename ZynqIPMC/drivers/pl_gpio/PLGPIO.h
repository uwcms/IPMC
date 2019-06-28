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
#include <drivers/generics/GPIO.h>
#include <drivers/InterruptBasedDriver.h>
#include "xgpio.h"
#include <functional>

// TODO: Interrupt can only be controlled by a single class so multiple channels with interrupts are not supported

/**
 * Wraps the low level C driver for Xilinx AXI GPIO IP.
 */
class PL_GPIO : public GPIO, protected InterruptBasedDriver {
public:
	///! Possible channels, GPIO_CHANNEL1 is the default in all operation.
	enum BusChannel {
		CHANNEL1 = 1,
		CHANNEL2 = 2
	};

	/**
	 * Create a PL based GPIO interface without interrupt support.
	 * @param Channel The channel in the IP to associate with this class.
	 * @param DeviceId The device ID, normally XPAR_AXI_GPIO_<>.
	 */
	PL_GPIO(PL_GPIO::BusChannel Channel, uint16_t DeviceId);

	/**
	 * Create a PL based GPIO interface with interrupt support.
	 * IP must support interrupts as well.
	 * @param DeviceId The device ID, normally XPAR_AXI_GPIO_<>.
	 * @param Channel The channel in the IP to associate with this class.
	 * @param IntrId The device interrupt ID from GIC, normally XPAR_FABRIC_AXI_GPIO_<>.
	 **/
	PL_GPIO(PL_GPIO::BusChannel Channel, uint16_t DeviceId, uint32_t IntrId);
	virtual ~PL_GPIO() {};

	// Overrides GPIO
	uint32_t getDirection();
	void setDirection(uint32_t d);
	void setBitDirection(uint32_t b, bool input);
	uint32_t getBus();
	void setBus(uint32_t v);
	void setPin(uint32_t b);
	void clearPin(uint32_t b);

	///! Set the IRQ callback when the value of a channel changes.
	void setIRQCallback(std::function<void(uint32_t)> func);

	///! Check if the IP supports interrupts
	inline bool supportsInterrupts() { return this->Gpio.InterruptPresent; };

protected:
	void _InterruptHandler();

private:
	XGpio Gpio;                             ///< Internal use only.
	PL_GPIO::BusChannel Channel;            ///< Internal use only.
	std::function<void(uint32_t)> callback; ///< Internal use only.
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_PL_GPIO_PLGPIO_H_ */
