/*
 * EEPROM.h
 *
 *  Created on: Jan 2, 2018
 *      Author: jtikalsky
 */

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_SPI_EEPROM_SPIEEPROM_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_SPI_EEPROM_SPIEEPROM_H_

#include <FreeRTOS.h>
#include <semphr.h>
#include <IPMC.h>
#include <drivers/generics/EEPROM.h>
#include <drivers/generics/spi.h>

/**
 * A SPI EEPROM backed by the PS_SPI interface driver.
 *
 * Designed to support 25AA256-I/ST and 25AA02E48T-I/OT.
 */
class SPI_EEPROM : public EEPROM {
public:
	SPI_EEPROM(SPIMaster &spibus, u8 chip_select, u32 size, u8 page_size);
	virtual ~SPI_EEPROM();

	virtual size_t read(u16 address, u8 *buf, size_t bytes);
	virtual size_t write(u16 address, u8 *buf, size_t bytes);

protected:
	SemaphoreHandle_t mutex; ///< A mutex protecting chip access.
	SPIMaster &spibus; ///< The SPI bus this EEPROM is attached to.
	u8 cs; ///< The chip select ID for this EEPROM.
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_SPI_EEPROM_SPIEEPROM_H_ */
