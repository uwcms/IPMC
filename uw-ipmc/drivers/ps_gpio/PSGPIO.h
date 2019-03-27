/*
 * PSGPIO.h
 *
 *  Created on: Aug 8, 2018
 *      Author: mpv
 */

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_PS_GPIO_PSGPIO_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_PS_GPIO_PSGPIO_H_

#include <vector>
#include <drivers/generics/GPIO.h>
#include "xgpiops.h"

// TODO: Add interrupt support

/**
 * Wraps the low level C driver for Xilinx Zynq MIO pins.
 * Due to how pins are mapped in MIO what the driver does is virtually
 * map the pins and form a virtual bus, this obviously means that setting
 * the bus will be slow.
 */
class PS_GPIO : public GPIO {
public:
	/**
	 * Create a PL based GPIO interface without interrupt support.
	 * @param DeviceId The device ID, normally XPAR_AXI_GPIO_<>.
	 * @param pins Colection of
	 */
	PS_GPIO(uint16_t DeviceId, std::vector<uint8_t> pins);
	virtual ~PS_GPIO();

	// Overrides GPIO
	uint32_t getDirection();
	void setDirection(uint32_t d);
	void setBitDirection(uint32_t b, bool input);
	uint32_t getBus();
	void setBus(uint32_t v);
	void setPin(uint32_t b);
	void clearPin(uint32_t b);

private:
	XGpioPs GpioPs; ///< Internal use only.

	struct pinInfo {
		uint8_t pin; // In bank
		uint8_t bank;
	};

	size_t pinCount;
	pinInfo *pins;
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_PS_GPIO_PSGPIO_H_ */
