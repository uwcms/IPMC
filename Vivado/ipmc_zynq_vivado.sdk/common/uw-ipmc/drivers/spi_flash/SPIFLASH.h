/*
 * SPIFLASH.h
 *
 *  Created on: Aug 20, 2018
 *      Author: mpv
 */

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_SPI_FLASH_SPIFLASH_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_SPI_FLASH_SPIFLASH_H_

#include <stdint.h>
#include <drivers/generics/Flash.h>
#include <drivers/generics/SPI.h>

/**
 * Flash implementation with SPI interface, check Flash.h for more information.
 * Initialize the flash before any operation by executing initialize().
 */
class SPIFlash : public Flash {
public:
	/**
	 * Constructs a new SPI flash interface.
	 * @param spi The SPI interface to use.
	 * @param cs The chip select bit in the interface to which the flash responds.
	 */
	SPIFlash(SPIMaster& spi, uint8_t cs);
	virtual ~SPIFlash() {};

	bool read(uint32_t address, uint8_t *buffer, size_t bytes);
	bool write(uint32_t address, const uint8_t *buffer, size_t bytes);

private:
	bool getJEDECInfo();
	bool disableWriteProtections();
	bool enableWriting();
	bool disableWriting();
	bool waitForWriteComplete();
	bool writePage(uint32_t address, const uint8_t *buffer, size_t bytes);
	bool eraseSectors(uint32_t address, size_t bytes);
	StatusRegister getStatusRegister();

	SPIMaster &spi;
	uint8_t cs;
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_SPI_FLASH_SPIFLASH_H_ */
