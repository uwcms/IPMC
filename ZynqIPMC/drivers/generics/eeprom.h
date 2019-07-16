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

#ifndef SRC_COMMON_ZYNQIPMC_DRIVERS_GENERICS_EEPROM_H_
#define SRC_COMMON_ZYNQIPMC_DRIVERS_GENERICS_EEPROM_H_

#include "FreeRTOS.h"
#include <xil_types.h>

/**
 * Abstract EEPROM interface that can be used with generic EEPROMs.
 */
class EEPROM {
public:
	/**
	 * Initialize internal variables common to the interface.
	 * @param size Size in bytes of the EEPROM.
	 * @param page_size  {age size in bytes of the EEPROM.
	 */
	EEPROM(size_t size, size_t page_size) : kTotalSize(size), kPageSize(page_size) { };
	virtual ~EEPROM() { };

	/**
	 * Read from the EEPROM.
	 * @param address Read address.
	 * @param buf Output buffer where the read data will be written to.
	 * @param bytes Number of bytes to read.
	 * @return Total number of bytes read.
	 * @throw std::runtime_error if address out-of-range.
	 */
	virtual size_t read(size_t address, uint8_t *buf, size_t bytes) = 0;

	/**
	 * Write to the EEPROM.
	 * @param address Write address.
	 * @param buf Buffer with data to write.
	 * @param bytes Number of bytes to write.
	 * @return Total number of bytes written.
	 * @throw std::runtime_error if address out-of-range.
	 */
	virtual size_t write(size_t address, const uint8_t *buf, size_t bytes) = 0;

	//! Return the EEPROM total size in bytes.
	inline const size_t getTotalSize() const { return this->kTotalSize; };

	//! Return the EEPROM page size in bytes.
	inline const size_t getPageSize() const { return this->kPageSize; };

protected:
	const size_t kTotalSize;	///< EEPROM total size in bytes.
	const size_t kPageSize;		///< EEPROM page size in bytes.
};

#endif /* SRC_COMMON_ZYNQIPMC_DRIVERS_GENERICS_EEPROM_H_ */
