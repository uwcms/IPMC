/*
 * PersistentStorage.h
 *
 *  Created on: Jan 2, 2018
 *      Author: jtikalsky
 */

#ifndef SRC_COMMON_UW_IPMC_SERVICES_PERSISTENTSTORAGE_PERSISTENTSTORAGE_H_
#define SRC_COMMON_UW_IPMC_SERVICES_PERSISTENTSTORAGE_PERSISTENTSTORAGE_H_

#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>
#include <IPMC.h>
#include <drivers/spi_eeprom/SPIEEPROM.h>
#include <libs/SkyRoad.h>
#include <libs/ThreadingPrimitives.h>

/**
 * A persistent storage module backed by a SPI_EEPROM.
 *
 * This module will manage background (or foreground) flush of an in-memory
 * cache of the EEPROM contents, as well as allocation of sections of EEPROM for
 * specific uses.
 */
class PersistentStorage {
public:
	PersistentStorage(SPI_EEPROM &eeprom, LogTree &logtree);
	virtual ~PersistentStorage();
	bool flush(TickType_t timeout = 0);

	u16 get_section_version(u16 section_id);
	void set_section_version(u16 section_id, u16 section_version);
	void *get_section(u16 section_id, u16 section_version, u16 section_size);
	void delete_section(u16 section_id);

protected:
	SPI_EEPROM &eeprom; ///< The eeprom backing this storage
	TaskHandle_t flushtask; ///< The background flush task
	u8 *cache; ///< The cache of true EEPROM contents for comparison in flush.
	u8 *data; ///< The data for real use.
	LogTree &logtree; ///< Log target
	Event storage_loaded; ///< A threading event indicating the storage is loaded.
	WaitList *flushwait[2]; ///< A pair of waitlists used to allow for synchronous flush.
	SemaphoreHandle_t index_mutex; ///< A mutex protecting the section index.
	SemaphoreHandle_t prio_mutex; ///< A mutex protecting the flush task's priority escalation.

public:
	void run_flush_thread(); ///< \protected Internal.
};

/**
 * This namespace contains ID allocations for sections of persistent storage.
 *
 * An ID is a u16 consisting of two one byte fields:
 *   MSB: Vendor ID
 *   LSB: Record ID (vendor specific)
 *
 * If you write any module or variant of this IPMC which makes use of persistent
 * storage, please reserve your Vendor ID by making a pull request to the main
 * repository.
 */
namespace PersistentStorageAllocations {
	/* Vendor 0: RESERVED */
	const u16 RESERVED_END_OF_INDEX = 0x0000; ///< A marker to internally denote the end of the index.
	/* Vendor 1: University of Wisconsin */
	const u16 WISC_SDR_REPOSITORY = 0x0101; ///< The SDR repository.
};

#endif /* SRC_COMMON_UW_IPMC_SERVICES_PERSISTENTSTORAGE_PERSISTENTSTORAGE_H_ */
