#ifndef UW_IPMC_DRIVERS_GENERICS_UART_H_
#define UW_IPMC_DRIVERS_GENERICS_UART_H_

#include "xil_types.h"
#include <FreeRTOS.h>

/**
 * A generic UART driver.
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
	virtual size_t read(u8 *buf, size_t len, TickType_t timeout, TickType_t data_timeout=portMAX_DELAY) = 0;
	/// \overload
	virtual size_t read(char *buf, size_t len, TickType_t timeout, TickType_t data_timeout=portMAX_DELAY) { return this->read(reinterpret_cast<u8*>(buf), len, timeout, data_timeout); };

	/**
	 * Write to the UART.
	 *
	 * \param buf The buffer to write from.
	 * \param len The maximum number of bytes to write.
	 * \param timeout The timeout for this read, in standard FreeRTOS format.
	 */
	virtual size_t write(const u8 *buf, size_t len, TickType_t timeout) = 0;
	/// \overload
	virtual size_t write(const char *buf, size_t len, TickType_t timeout) { return this->write(reinterpret_cast<const u8*>(buf), len, timeout); }

	/**
	 * Clear the input queue.
	 * @return false if clear failed, true otherwise.
	 */
	virtual bool clear() = 0;
};

#endif /* UW_IPMC_DRIVERS_GENERICS_UART_H_ */
