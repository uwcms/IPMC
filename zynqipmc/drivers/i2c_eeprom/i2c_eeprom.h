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

#ifndef SRC_COMMON_ZYNQIPMC_DRIVERS_I2C_EEPROM_I2CEEPROM_H_
#define SRC_COMMON_ZYNQIPMC_DRIVERS_I2C_EEPROM_I2CEEPROM_H_

#include <FreeRTOS.h>
#include <semphr.h>
#include <drivers/generics/i2c.h>
#include <drivers/generics/eeprom.h>

/**
 * An EEPROM backed by any of the available SPI interface driver.
 *
 * Designed to support 24AA256, 24LC256, and 24FC256, may be compatible with others.
 */
class I2CEEPROM: public EEPROM {
public:
	/**
	 * Instantiate an EEPROM interface.
	 *
	 * @param i2cbus       The I2C bus the EEPROM is on
	 * @param address      The I2C address the EEPROM is on
	 * @param size         The size of the EEPROM
	 * @param page_size    The page size of the EEPROM
	 */
	I2CEEPROM(I2C &i2cbus, uint8_t address, size_t size, size_t page_size);
	virtual ~I2CEEPROM();

	// From base class EEPROM:
	virtual size_t read(size_t address, uint8_t *buf, size_t bytes);
	virtual size_t write(size_t address, const uint8_t *buf, size_t bytes);

protected:
	SemaphoreHandle_t mutex;	///< A mutex protecting chip access.
	I2C &i2cbus;				///< The I2C bus this EEPROM is attached to.
	uint8_t address;			///< The I2C address for this EEPROM.
};

#endif /* SRC_COMMON_ZYNQIPMC_DRIVERS_SPI_EEPROM_SPIEEPROM_H_ */
