/*
 * PLI2C.h
 *
 *  Created on: Feb 26, 2018
 *      Author: mpv
 */

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_PL_I2C_PLI2C_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_PL_I2C_PLI2C_H_

#include <functional>
#include <xiic.h>
#include "drivers/generics/I2C.h"
#include "semphr.h"

/**
 * A FreeRTOS interrupt based driver for Xilinx IIC IP core.
 *
 * Simple IIC wrapper supporting interrupts and I2C master operations.
 * No repeated start support at this moment.
 */
class PL_I2C : public I2C {
public:
	/**
	 * Instantiate a PL_I2C driver.
	 *
	 * \note This performs hardware setup (mainly interrupt configuration).
	 *
	 * \param DeviceId             The DeviceId, used for XIicPs_LookupConfig(), etc
	 * \param IntrId               The interrupt ID, for configuring the GIC.
	 */
	PL_I2C(uint16_t DeviceId, uint32_t IntrId);
	virtual ~PL_I2C();

	size_t read(uint8_t addr, uint8_t *buf, size_t len, TickType_t timeout=portMAX_DELAY);
	size_t write(uint8_t addr, const uint8_t *buf, size_t len, TickType_t timeout=portMAX_DELAY);

private:
	uint16_t DeviceId; ///< Used internally.
	uint32_t IntrId; ///< Used internally.
	XIic IicInst;  ///< Used internally.

public:
	SemaphoreHandle_t semaphoreSync; ///< Used internally. DO NOT USE.
	volatile uint32_t IrqStatus; ///< Used internally. DO NOT USE.
	volatile bool wasEvent; ///< Used internally. DO NOT USE.
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_PL_I2C_PLI2C_H_ */
