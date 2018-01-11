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
#include <event_groups.h>
#include <IPMC.h>
#include <drivers/generics/EEPROM.h>
#include <libs/SkyRoad.h>
#include <libs/ThreadingPrimitives.h>
#include <functional>
#include <queue>
#include <deque>

/**
 * A persistent storage module backed by a SPI_EEPROM.
 *
 * This module will manage background (or foreground) flush of an in-memory
 * cache of the EEPROM contents, as well as allocation of sections of EEPROM for
 * specific uses.
 */
class PersistentStorage {
public:
	PersistentStorage(EEPROM &eeprom, LogTree &logtree);
	virtual ~PersistentStorage();
	void flush(std::function<void(void)> completion_cb = NULL);
	void flush(void *start = NULL, size_t len = SIZE_MAX, std::function<void(void)> completion_cb = NULL);
protected:
	void flush_index();

public:
	u16 get_section_version(u16 section_id);
	void set_section_version(u16 section_id, u16 section_version);
	void *get_section(u16 section_id, u16 section_version, u16 section_size);
	void delete_section(u16 section_id);

protected:
	/// A pending flush request
	class FlushRequest {
	public:
		FlushRequest(u32 start, u32 end, std::function<void(void)> complete_cb, bool index_flush = false);
		u32 start; ///< The start of the flush range
		u32 end;   ///< The end of the flush range
		u32 process_priority; ///< The priority of the calling process
		std::function<void(void)> complete; ///< A callback to indicate completion.
		u64 requested_at; ///< tick64 when the flush was requested
		bool index_flush; ///< A marker indicating that this is an index flush, which overrides all priority.
		bool operator<(const FlushRequest &other) const;
	};
	EEPROM &eeprom; ///< The eeprom backing this storage
	TaskHandle_t flushtask; ///< The background flush task
	u8 *cache; ///< The cache of true EEPROM contents for comparison in flush.
	u8 *data; ///< The data for real use.
	LogTree &logtree; ///< Log target
	EventGroupHandle_t storage_loaded; ///< An event indicating storage loaded.
	SemaphoreHandle_t index_mutex; ///< A mutex protecting the section index.
	SemaphoreHandle_t flushq_mutex; ///< A mutex protecting the flush queue
	std::priority_queue< FlushRequest, std::deque<FlushRequest> > flushq; ///< A queue of pending range flushes.
	const TickType_t flush_ticks = 10 * configTICK_RATE_HZ; ///< The delay between background flushes.

public:
	void run_flush_thread(); ///< \protected Internal.
protected:
	bool do_flush_range(u32 start, u32 end);
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
