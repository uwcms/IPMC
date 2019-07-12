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

#ifndef SRC_COMMON_ZYNQIPMC_DRIVERS_PL_GPIO_PL_GPIO_H_
#define SRC_COMMON_ZYNQIPMC_DRIVERS_PL_GPIO_PL_GPIO_H_

// Only include driver if PL GPIO is detected in the BSP.
#if XSDK_INDEXING || __has_include("xgpio.h")

#include "xgpio.h"
#include <drivers/generics/gpio.h>
#include <drivers/InterruptBasedDriver.h>
#include <functional>

// TODO: Interrupt can only be controlled by a single class so multiple channels with interrupts are not supported

/**
 * High-level interface for Xilinx GPIO IP, supporting two channels and variable lengths.
 * Each channel will need its own instantiation.
 */
class PLGPIO : public GPIO, protected InterruptBasedDriver {
public:
	//! Possible channels, GPIO_CHANNEL1 is the default in all operations.
	enum BusChannel {
		CHANNEL1 = 1,
		CHANNEL2 = 2
	};

	/**
	 * Initialize the driver without interrupt support.
	 * No triggering on input state changes is possible in this mode.
	 * @param channel Bus channel to associate with driver.
	 * @param device_id Hardware device ID, normally XPAR_AXI_GPIO_<>_DEVICE_ID. Check xparameters.h.
	 * @throw except::hardware_error if low level driver fails to configure.
	 */
	PLGPIO(PLGPIO::BusChannel channel, uint16_t device_id);

	/**
	 * Initialize the driver with interrupt support. Can trigger on IRQs.
	 * IP needs to be configured properly.
	 * @param channel Bus channel to associate with driver.
	 * @param device_id Hardware device ID, normally XPAR_AXI_GPIO_<>_DEVICE_ID. Check xparameters.h.
	 * @param intr_id The device interrupt ID from GIC, normally XPAR_FABRIC_AXI_GPIO_<>_IP2INTC_IRPT_INTR. Check xparameters.h.
	 * @throw except::hardware_error if low level driver fails to configure.
	 **/
	PLGPIO(PLGPIO::BusChannel channel, uint16_t device_id, uint32_t intr_id);

	virtual ~PLGPIO();

	// From base class GPIO:
	virtual uint32_t getBusDirection() const;
	virtual void setBusDirection(uint32_t dir);
	virtual void setPinDirection(uint32_t pin, bool input);
	virtual uint32_t getBusValue() const;
	virtual void setBusValue(uint32_t value);
	virtual void clearPin(uint32_t pin);
	virtual void setPin(uint32_t pin);

	///! Set the IRQ callback when the value of an input pin.
	void setIRQCallback(std::function<void(uint32_t)> func);

	///! Check if the IP supports interrupts.
	inline bool supportsInterrupts() { return this->gpio.InterruptPresent; };

private:
	void _InterruptHandler();

	mutable XGpio gpio;						///< Internal GPIO driver data.
	const BusChannel kChannel;				///< Associated channel.
	std::function<void(uint32_t)> callback;	///< IRQ callback function
	SemaphoreHandle_t mutex;				///< Thread synchronization mutex.
};

#endif

#endif /* SRC_COMMON_ZYNQIPMC_DRIVERS_PL_GPIO_PL_GPIO_H_ */
