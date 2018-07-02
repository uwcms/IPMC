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
#include "semphr.h"
#include <functional>

/**
 * A generic I2C driver interface.
 */
class I2C {
public:
	I2C() {
		this->mutex = xSemaphoreCreateMutex();
		configASSERT(this->mutex);
	};

	virtual ~I2C() {
		vSemaphoreDelete(this->mutex);
	};

	/**
	 * Read buffer from the I2C interface
	 *
	 * \param addr The target I2C slave address.
	 * \param buf The buffer to read into.
	 * \param len The maximum number of bytes to read.
	 * \param timeout The timeout for this read, in standard FreeRTOS format.
	 * \return The total number of bytes read, 0 if an error occurred.
	 * \note Wrap around I2C::chain if multiple threads have access to the interface.
	 */
	virtual size_t read(uint8_t addr, uint8_t *buf, size_t len, TickType_t timeout) { configASSERT(0); return 0; };

	/**
	 * Write buffer to the I2C interface
	 *
	 * \param addr The target I2C slave address.
	 * \param buf The buffer to write from.
	 * \param len The maximum number of bytes to write.
	 * \param timeout The timeout for this read, in standard FreeRTOS format.
	 * \return The total number of bytes written, 0 if an error occurred.
	 * \note Wrap around I2C::chain if multiple threads have access to the interface.
	 */
	virtual size_t write(uint8_t addr, const uint8_t *buf, size_t len, TickType_t timeout) { configASSERT(0); return 0; };

	/**
	 * If I2C interface is used by several devices then several read/write
	 * operations can be chained by wrapping around a lambda function inside
	 * chain function which will grant exclusive access to the interface
	 *
	 * @param lambda_func The lambda function to run
	 **/
	inline virtual void chain(std::function<void(void)> lambda_func) final {
		xSemaphoreTake(this->mutex, portMAX_DELAY);
		lambda_func();
		xSemaphoreGive(this->mutex);
	}

private:
	SemaphoreHandle_t mutex;
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_GENERICS_I2C_H_ */
