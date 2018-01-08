/*
 * EEPROM.h
 *
 *  Created on: Jan 8, 2018
 *      Author: jtikalsky
 */

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_GENERICS_EEPROM_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_GENERICS_EEPROM_H_

#include <xil_types.h>

/**
 * A generic EEPROM interface driver.
 */
class EEPROM {
public:
	/**
	 * Instantiate an EEPROM.
	 *
	 * @param size  The size in bytes of the EEPROM
	 * @param page_size  The page size in bytes of the EEPROM
	 */
	EEPROM(u32 size, u8 page_size) : size(size), page_size(page_size) { };
	virtual ~EEPROM() { };

	const u32 size;     ///< The total EEPROM size
	const u8 page_size; ///< The page size for write transactions

	/**
	 * Read from the EEPROM.
	 *
	 * @param address The read address
	 * @param buf     The output buffer
	 * @param bytes   The number of bytes to read
	 * @return        The number of bytes read
	 */
	virtual size_t read(u16 address, u8 *buf, size_t bytes);

	/**
	 * Write to the EEPROM.
	 *
	 * @param address The write address
	 * @param buf     The output buffer
	 * @param bytes   The number of bytes to write
	 * @return        The number of bytes written
	 */
	virtual size_t write(u16 address, u8 *buf, size_t bytes);
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_GENERICS_EEPROM_H_ */
