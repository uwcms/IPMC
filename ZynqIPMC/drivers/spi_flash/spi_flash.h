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

#ifndef SRC_COMMON_ZYNQIPMC_DRIVERS_SPI_FLASH_SPI_FLASH_H_
#define SRC_COMMON_ZYNQIPMC_DRIVERS_SPI_FLASH_SPI_FLASH_H_

#include <stdint.h>
#include <drivers/generics/flash.h>
#include <drivers/generics/spi.h>
#include <libs/LogTree.h>

/**
 * JEDEC flash implementation with SPI interface, check flash.h for more information.
 * Initialize the flash before any operation by executing initialize().
 */
class SPIFlash : public Flash {
public:
	/**
	 * Constructs a new SPI flash interface.
	 * @param spi SPI interface that is wired to the flash.
	 * @param chip_select The chip select bit in the interface to which the flash responds.
	 */
	SPIFlash(SPIMaster& spi, size_t chip_select, LogTree &log);
	virtual ~SPIFlash() {};

	// From base class Flash:
	bool initialize();
	bool read(uint32_t address, uint8_t *buffer, size_t bytes);
	bool write(uint32_t address, const uint8_t *buffer, size_t bytes);

private:
	// From base class Flash:
	bool getJEDECInfo();
	bool getManufacturerID();
	bool disableWriteProtections();
	bool enableWriting();
	bool disableWriting();
	bool waitForWriteComplete();
	bool selectBank(uint8_t bank);
	bool getSelectedBank(uint8_t &bank);
	bool writePage(uint32_t address, const uint8_t *buffer, size_t bytes);
	bool eraseSectors(uint32_t address, size_t bytes);
	bool getStatusRegister(StatusRegister &status);

	//! Attempt to enable Quad operation mode if flash supports it.
	bool enableQuadBit();

	//! Erase a specific sector. Address must be sector aligned.
	bool eraseSector(uint32_t address);

	SPIMaster &spi;				///< Target SPI interface.
	const size_t kChipSelect;	///< Flash chip-select.
	LogTree &log;				///< Logging reference for progress.
	bool quadModeEnabled;		///< Indicates if Quad Mode is enabled. Enabled through enableQuadBit().
};

#endif /* SRC_COMMON_ZYNQIPMC_DRIVERS_SPI_FLASH_SPI_FLASH_H_ */
