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
#include <drivers/watchdog/PSWDT.h>
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
	PersistentStorage(EEPROM &eeprom, LogTree &logtree, PS_WDT *watchdog=NULL);
	virtual ~PersistentStorage();
	void flush(std::function<void(void)> completion_cb = NULL);
	void flush(void *start, size_t len, std::function<void(void)> completion_cb = NULL);
protected:
	void flush_index();

public:
	u16 get_section_version(u16 section_id);
	void set_section_version(u16 section_id, u16 section_version);
	void *get_section(u16 section_id, u16 section_version, u16 section_size);
	void delete_section(u16 section_id);

	/**
	 * An entry in the persistent storage section index.
	 */
	struct PersistentStorageIndexRecord {
		u16 id;      ///< The ID of the section.
		u16 pgoff;   ///< The page number of the section start.
		u16 pgcount; ///< The length in pages of the section.
		u16 version; ///< The version of the section.
	};
	std::vector<struct PersistentStorageIndexRecord> list_sections();

	void register_console_commands(CommandParser &parser, const std::string &prefix="");
	void deregister_console_commands(CommandParser &parser, const std::string &prefix="");

	EEPROM &eeprom; ///< The eeprom backing this storage

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
	TaskHandle_t flushtask; ///< The background flush task
	u8 *cache; ///< The cache of true EEPROM contents for comparison in flush.
	u8 *data; ///< The data for real use.
	LogTree &logtree; ///< Log target
	EventGroupHandle_t storage_loaded; ///< An event indicating storage loaded.
	SemaphoreHandle_t index_mutex; ///< A mutex protecting the section index.
	SemaphoreHandle_t flushq_mutex; ///< A mutex protecting the flush queue
	std::priority_queue< FlushRequest, std::deque<FlushRequest> > flushq; ///< A queue of pending range flushes.
	const TickType_t flush_ticks = 10 * configTICK_RATE_HZ; ///< The delay between background flushes.
	PS_WDT *wdt; ///< The watchdog.
	PS_WDT::slot_handle_t wdt_slot; ///< The watchdog slot handle to activate and service.

public:
	void run_flush_thread(); ///< \protected Internal.
protected:
	bool do_flush_range(u32 start, u32 end);
};

#ifdef __PERSISTENT_STORAGE_CPP
#define PERSISTENT_STORAGE_ALLOCATE(id, name) \
	const u16 name = id; \
	static void _ps_reg_section_ ## name() __attribute__((constructor(1000))); \
	static void _ps_reg_section_ ## name() { \
		if (!name_to_id) \
			name_to_id = new std::map<std::string, u16>(); \
		if (!id_to_name) \
			id_to_name = new std::map<u16, std::string>(); \
		(*name_to_id)[#name] = id; \
		(*id_to_name)[id] = #name; \
	}
#else // __PERSISTENT_STORAGE_CPP
#define PERSISTENT_STORAGE_ALLOCATE(id, name) \
		const u16 name = id;
#endif // __PERSISTENT_STORAGE_CPP

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
	extern std::map<u16, std::string> *id_to_name; ///< Allocation ID to Name mapping.
	extern std::map<std::string, u16> *name_to_id; ///< Allocation Name to ID mapping.

	/* Vendor 0: RESERVED */
	PERSISTENT_STORAGE_ALLOCATE(0x0000, RESERVED_END_OF_INDEX); ///< A marker to internally denote the end of the index.
	/* Vendor 1: University of Wisconsin */
	PERSISTENT_STORAGE_ALLOCATE(0x0101, WISC_SDR_REPOSITORY); ///< The SDR repository.
	PERSISTENT_STORAGE_ALLOCATE(0x0102, WISC_INFLUXDBCLIENT_CONFIG); ///< InfluxDB configuration
	PERSISTENT_STORAGE_ALLOCATE(0x0103, WISC_NETWORK_AUTH); ///< Auth configuration for network services
};

#undef PERSISTENT_STORAGE_ALLOCATE

#endif /* SRC_COMMON_UW_IPMC_SERVICES_PERSISTENTSTORAGE_PERSISTENTSTORAGE_H_ */
