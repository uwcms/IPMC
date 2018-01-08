/*
 * SPI.h
 *
 *  Created on: Jan 8, 2018
 *      Author: jtikalsky
 */

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_GENERICS_SPI_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_GENERICS_SPI_H_

#include "FreeRTOS.h"
#include <xil_types.h>

/**
 * A generic SPI driver.
 */
class SPI {
public:
	SPI() { };
	virtual ~SPI() { };

	/**
	 * This function will perform a SPI transfer in a blocking manner.
	 *
	 * \param chip     The chip select to enable.
	 * \param sendbuf  The data to send.
	 * \param recvbuf  A buffer for received data.
	 * \param bytes    The number of bytes to transfer
	 * \return false on error, else true
	 */
	virtual bool transfer(u8 chip, u8 *sendbuf, u8 *recvbuf, size_t bytes) { configASSERT(0); return false; };
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_GENERICS_SPI_H_ */
