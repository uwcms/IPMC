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

#ifndef SRC_COMMON_ZYNQIPMC_DRIVERS_PS_GPIO_PS_GPIO_H_
#define SRC_COMMON_ZYNQIPMC_DRIVERS_PS_GPIO_PS_GPIO_H_

// Only include driver if PS GPIO is detected in the BSP.
#if XSDK_INDEXING || __has_include("xgpiops.h")

#include "xgpiops.h"
#include <vector>
#include <drivers/generics/gpio.h>

// TODO: Add interrupt support.

/**
 * High-level driver for Xilinx Zynq MIO pins.
 * Due to how pins are mapped in MIO what the driver does is virtually
 * map the pins and form a virtual bus, this obviously means that setting
 * the bus will be slow.
 */
class PSGPIO : public GPIO {
public:
	/**
	 * Create a PL based GPIO interface without interrupt support.
	 * @param DeviceId The device ID, normally XPAR_AXI_GPIO_<>.
	 * @param pins Collection of pins to form the virtual bus
	 * @throw std::out_of_range if the number of pins is improper.
	 * @throw except::hardware_error if low level driver fails to configure.
	 */
	PSGPIO(uint16_t device_id, std::vector<uint8_t> pins);
	virtual ~PSGPIO();

	// From base class GPIO:
	virtual uint32_t getBusDirection() const;
	virtual void setBusDirection(uint32_t dir);
	virtual void setPinDirection(uint32_t pin, bool input);
	virtual uint32_t getBusValue() const ;
	virtual void setBusValue(uint32_t value);
	virtual void clearPin(uint32_t pin);
	virtual void setPin(uint32_t pin);

private:
	struct PinInfo {
		uint8_t pin; // In bank
		uint8_t bank;
	};

	mutable XGpioPs gpiops;		///< Internal ADC driver data.
	const size_t kPinCount;		///< Number of pins associated.
	PinInfo *pins;				///< Associated pins.
	SemaphoreHandle_t mutex;	///< Thread synchronization mutex.
};

#endif

#endif /* SRC_COMMON_ZYNQIPMC_DRIVERS_PS_GPIO_PS_GPIO_H_ */
