/*
 * This file is part of the ZYNQ-IPMC Framework.
 *
 * The ZYNQ-IPMC Framework is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The ZYNQ-IPMC Framework is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the ZYNQ-IPMC Framework.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_SPI_EEPROM_SPIEEPROM_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_SPI_EEPROM_SPIEEPROM_H_

#include <FreeRTOS.h>
#include <semphr.h>
#include <drivers/generics/spi.h>
#include <drivers/generics/eeprom.h>

/**
 * An EEPROM backed by any of the available SPI interface driver.
 *
 * Designed to support 25AA256-I/ST and 25AA02E48T-I/OT, may be compatible with others.
 */
class SPIEEPROM: public EEPROM {
public:
	/**
	 * Instantiate an EEPROM interface.
	 *
	 * @param spibus       The PS_SPI bus the EEPROM is on
	 * @param chip_select  The chip select for the EEPROM
	 * @param size         The size of the EEPROM
	 * @param page_size    The page size of the EEPROM
	 */
	SPIEEPROM(SPIMaster &spibus, size_t chip_select, size_t size, size_t page_size);
	virtual ~SPIEEPROM();

	// From base class EEPROM:
	virtual size_t read(size_t address, uint8_t *buf, size_t bytes);
	virtual size_t write(size_t address, const uint8_t *buf, size_t bytes);

protected:
	SemaphoreHandle_t mutex;	///< A mutex protecting chip access.
	SPIMaster &spibus;			///< The SPI bus this EEPROM is attached to.
	size_t chip_select;			///< The chip select for this EEPROM.
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_SPI_EEPROM_SPIEEPROM_H_ */
