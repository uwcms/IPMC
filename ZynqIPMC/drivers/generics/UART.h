#ifndef UW_IPMC_DRIVERS_GENERICS_UART_H_
#define UW_IPMC_DRIVERS_GENERICS_UART_H_

#include "xil_types.h"
#include <FreeRTOS.h>

/**
 * A generic UART abstract interface driver.
 */
class UART {
public:
	UART() { };
	virtual ~UART() { };

	/**
	 * Read from the UART.
	 *
	 * \param buf The buffer to read into.
	 * \param len The maximum number of bytes to read.
	 * \param timeout The timeout for this read, in standard FreeRTOS format.
	 * \param data_timeout A second timeout used to shorten `timeout` if data is present.
	 *
	 * \note This function is interrupt and critical safe if timeout=0.
	 */
	virtual size_t read(u8 *buf, size_t len, TickType_t timeout=portMAX_DELAY, TickType_t data_timeout=portMAX_DELAY) = 0;

	/**
	 * Write to the UART.
	 *
	 * \param buf The buffer to write from.
	 * \param len The maximum number of bytes to write.
	 * \param timeout The timeout for this read, in standard FreeRTOS format.
	 */
	virtual size_t write(const u8 *buf, size_t len, TickType_t timeout=portMAX_DELAY) = 0;

	/**
	 * Clear the input queue.
	 * @return false if clear failed, true otherwise.
	 */
	virtual bool clear() = 0;
};

#endif /* UW_IPMC_DRIVERS_GENERICS_UART_H_ */