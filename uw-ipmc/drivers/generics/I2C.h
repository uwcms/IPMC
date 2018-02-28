/*
 * I2C.h
 *
 *  Created on: Jan 8, 2018
 *      Author: jtikalsky
 */

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_GENERICS_I2C_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_GENERICS_I2C_H_

#include "FreeRTOS.h"
#include <xil_types.h>

/**
 * A generic I2C driver interface.
 */
class I2C {
public:
	//I2C() { };
	virtual ~I2C() { };

	/**
	 * Read command from the I2C.
	 *
	 * \param addr The target I2C slave address.
	 * \param buf The buffer to read into.
	 * \param len The maximum number of bytes to read.
	 * \param timeout The timeout for this read, in standard FreeRTOS format.
	 *
	 * \note This function is interrupt and critical safe if timeout=0.
	 */
	virtual size_t read(u8 addr, u8 *buf, size_t len, TickType_t timeout) { configASSERT(0); return 0; };

	/**
	 * Write command to the I2C.
	 *
	 * \param addr The target I2C slave address.
	 * \param buf The buffer to write from.
	 * \param len The maximum number of bytes to write.
	 * \param timeout The timeout for this read, in standard FreeRTOS format.
	 */
	virtual size_t write(u8 addr, const u8 *buf, size_t len, TickType_t timeout) { configASSERT(0); return 0; };
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_GENERICS_I2C_H_ */
