/*
 * PSSPI.h
 *
 *  Created on: Jan 2, 2018
 *      Author: jtikalsky
 */

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_PS_SPI_PSSPI_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_PS_SPI_PSSPI_H_

#include <FreeRTOS.h>
#include <semphr.h>
#include <queue.h>
#include <IPMC.h>
#include <vector>

/**
 * An interrupt-based driver for the PS SPI.
 */
class PS_SPI {
public:
	PS_SPI(u16 DeviceId, u32 IntrId, std::vector<u8> cs_gpios);
	virtual ~PS_SPI();

	bool transfer(u8 chip, u8 *sendbuf, u8 *recvbuf, size_t bytes);
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_PS_SPI_PSSPI_H_ */
