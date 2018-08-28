/*
 * SPI.h
 *
 *  Created on: Jan 8, 2018
 *      Author: jtikalsky
 */

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_GENERICS_SPI_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_GENERICS_SPI_H_

#include <drivers/AtomicitySupport.h>
#include <FreeRTOS.h>
#include <functional>
#include <xil_types.h>

/**
 * An abstract SPI master driver.
 * Chain operations supported by AddressableAtomicitySupport::atomic.
 */
class SPIMaster : public AddressableAtomicitySupport {
public:
	/**
	 * This function will perform a SPI transfer in a blocking manner.
	 * \param chip     The chip select to enable.
	 * \param sendbuf  The data to send.
	 * \param recvbuf  A buffer for received data. Can be NULL if no data needs to be kept.
	 * \param bytes    The number of bytes to transfer.
	 * \param timeout  Timeout in ticks, default is portMAX_DELAY (wait forever).
	 * \return false on error, else true.
	 * \note This function is thread-safe.
	 */
	virtual bool transfer(u8 chip, const u8 *sendbuf, u8 *recvbuf, size_t bytes, TickType_t timeout = portMAX_DELAY) = 0;

	/**
	 * Execute a SPI transfer without selecting or de-selecting a device. Useful for chaining.
	 * @param sendbuf The data to send.
	 * @param recvbuf A buffer for received data. Can be NULL if no data needs to be kept.
	 * @param bytes Number of bytes to transfer.
	 * @param timeout Timeout in ticks, default is portMAX_DELAY (wait forever).
	 * @return flase on error, else true.
	 * @warning this function is *NOT* thread-safe but can, and should, be used
	 * inside AddressableAtomicitySupport::atomic safely.
	 */
	virtual bool transfer_unsafe(const u8 *sendbuf, u8 *recvbuf, size_t bytes, TickType_t timeout = portMAX_DELAY) = 0;
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_GENERICS_SPI_H_ */
