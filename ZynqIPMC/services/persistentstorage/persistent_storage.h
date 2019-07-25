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

#ifndef SRC_COMMON_ZYNQIPMC_SERVICES_PERSISTENTSTORAGE_PERSISTENT_STORAGE_H_
#define SRC_COMMON_ZYNQIPMC_SERVICES_PERSISTENTSTORAGE_PERSISTENT_STORAGE_H_

#include <functional>
#include <queue>
#include <drivers/generics/eeprom.h>
#include <drivers/watchdog/ps_wdt.h>
#include <libs/threading.h>
#include <services/console/command_parser.h>

/**
 * A persistent storage module backed by an EEPROM.
 *
 * This module will manage background (or foreground) flush of an in-memory
 * cache of the EEPROM contents, as well as allocation of sections of EEPROM for
 * specific uses.
 */
class PersistentStorage final : public ConsoleCommandSupport {
public:
	/**
	 * Instantiate a Persistent Storage module backed by the supplied EEPROM.
	 *
	 * @param eeprom The EEPROM providing the backing for this module.
	 * @param logtree Where to send log messages from this module.
	 * @param watchdog The watchdog to register with & service.
	 * @throw std::domain_error in case of invalid EEPROM.
	 */
	PersistentStorage(EEPROM &eeprom, LogTree &logtree, PSWDT *watchdog=nullptr);
	virtual ~PersistentStorage();

	/**
	 * Enqueue an immediate flush of the full EEPROM.
	 *
	 * @param completion_cb A callback to call upon completion.  (Triggers priority inheritance if not nullptr.)
	 */
	void flush(std::function<void(void)> completion_cb = nullptr);

	/**
	 * Enqueue an immediate flush of a subset of EEPROM.
	 *
	 * @param start A pointer to the start of the range to be flushed.
	 * @param len The length in bytes of the range to be flushed.
	 * @param completion_cb A callback to call upon completion.  (Triggers priority inheritance if not nullptr.)
	 */
	void flush(void *start, size_t len, std::function<void(void)> completion_cb = nullptr);

	/**
	 * Return the current version of the specified section, or 0 if it does not exist.
	 *
	 * @param section_id The section to check the version of
	 * @return The section version, or zero if absent.
	 */
	uint16_t getSectionVersion(uint16_t section_id);

	/**
	 * Set the current version of the specified section, if it exists.
	 *
	 * @param section_id The section that should be updated.
	 * @param section_version The new version number of that section.
	 */
	void setSectionVersion(uint16_t section_id, uint16_t section_version);

	/**
	 * Retrieve the specified persistent storage section, allocating it if necessary.
	 *
	 * When retrieving a persistent storage, the supplied version and size must
	 * match the existing record or an error will occur.
	 *
	 * @param section_id The ID of the section to allocate or retrieve.
	 * @param section_version The version of the section to allocate or retrieve.
	 * @param section_size The size of the section to allocate or retrieve.
	 * @return A pointer to a memory of size section_size, backed by persistent storage, or nullptr on error.
	 */
	void *getSection(uint16_t section_id, uint16_t section_version, uint16_t section_size);

	/**
	 * Delete all instances of the specified persistent storage section.
	 *
	 * @param section_id The ID of the section to delete.
	 */
	void deleteSection(uint16_t section_id);

	//! An entry in the persistent storage section index.
	struct PersistentStorageIndexRecord {
		uint16_t id;      ///< The ID of the section.
		uint16_t pgoff;   ///< The page number of the section start.
		uint16_t pgcount; ///< The length in pages of the section.
		uint16_t version; ///< The version of the section.
	};

	//! Return a list of all persistent storage sections.
	std::vector<struct PersistentStorageIndexRecord> listSections();

	// From base class ConsoleCommandSupport:
	virtual void registerConsoleCommands(CommandParser &parser, const std::string &prefix="");
	virtual void deregisterConsoleCommands(CommandParser &parser, const std::string &prefix="");

	friend class VariablePersistentAllocation;

private:
	// Available console commands:
	class ListSectionsCommand;
	class ReadCommand;
	class WriteCommand;
	class SetSectionVersionCommand;
	class DeleteSectionCommand;

	/**
	 * Put in a priority flush request for the index.
	 *
	 * @note This will take the index_mutex to auto-calculate the index length.
	 */
	void flushIndex();

	//! A pending flush request.
	class FlushRequest {
	public:
		/**
		 * Instantiate a FlushRequest record
		 *
		 * @param start Start of range.
		 * @param end   End of range.
		 * @param complete_cb Completion callback.
		 * @param index_flush Indicate whether this is an index flush (and thus max priority).
		 */
		FlushRequest(uint32_t start, uint32_t end, std::function<void(void)> complete_cb, bool index_flush = false);

		uint32_t start; ///< The start of the flush range
		uint32_t end;   ///< The end of the flush range
		uint32_t process_priority; ///< The priority of the calling process
		std::function<void(void)> complete; ///< A callback to indicate completion.
		u64 requested_at; ///< tick64 when the flush was requested
		bool index_flush; ///< A marker indicating that this is an index flush, which overrides all priority.

		//! A comparison used to manage the priority queue.
		bool operator<(const FlushRequest &other) const;
	};

	EEPROM &eeprom; ///< The eeprom backing this storage.
	TaskHandle_t flushtask; ///< The background flush task.
	uint8_t *cache; ///< The cache of true EEPROM contents for comparison in flush.
	uint8_t *data; ///< The data for real use.
	LogTree &logtree; ///< Log target.
	EventGroupHandle_t storage_loaded; ///< An event indicating storage loaded.
	SemaphoreHandle_t index_mutex; ///< A mutex protecting the section index.
	SemaphoreHandle_t flushq_mutex; ///< A mutex protecting the flush queue
	std::priority_queue< FlushRequest, std::deque<FlushRequest> > flushq; ///< A queue of pending range flushes.
	const TickType_t flush_ticks = 10 * configTICK_RATE_HZ; ///< The delay between background flushes.
	PSWDT *wdt; ///< The watchdog.
	PSWDT::slot_handle_t wdt_slot; ///< The watchdog slot handle to activate and service.

	void flushThread();

	/**
	 * Flush a given range of the raw persistent storage.
	 *
	 * @param start The first byte of the range.
	 * @param end The first byte after the range.
	 * @return true if changes were flushed, else false.
	 */
	bool doFlushRange(uint32_t start, uint32_t end);

	//! Dump the PersistentStorage index to the tracebuffer.
	void traceIndex();
};

/**
 * A helper class allowing the easy management of a single block of un-versioned
 * variable length data within persistent storage.
 *
 * @warning Use of this mechanism may induce fragmentation within the EEPROM
 *          over time if the data length changes in conjunction with other
 *          storage changes.  There is presently no defragmentation mechanism
 *          available for Persistent Storage.
 */
class VariablePersistentAllocation final {
public:
	/**
	 * Instantiate a copy of this helper class for the given storage allocation and area.
	 * @param storage The storage area this helper should access.
	 * @param allocation_id The allocation ID this helper manages.
	 */
	VariablePersistentAllocation(PersistentStorage &storage, uint16_t allocation_id)
		: storage(storage), id(allocation_id) {
		configASSERT(this->mutex = xSemaphoreCreateMutex());
	};
	~VariablePersistentAllocation() {
		vSemaphoreDelete(this->mutex);
	};

	//! Retrieves the data stored in this allocation.
	std::vector<uint8_t> getData();

	/**
	 * Writes the provided data to this allocation.
	 *
	 * @param data The data to store.
	 * @param flush_completion_cb A completion callback passed through to storage.flush().
	 * @return true on success, else failure.
	 */
	bool setData(const std::vector<uint8_t> &data, std::function<void(void)> flush_completion_cb = nullptr);

private:
	PersistentStorage &storage; ///< The PersistentStorage containing the allocation.
	uint16_t id; ///< The ID of the allocation managed by this mechanism.
	SemaphoreHandle_t mutex; ///< A mutex serializing operations on this allocation.
};

#ifndef PERSISTENT_STORAGE_ALLOCATE
#define PERSISTENT_STORAGE_ALLOCATE(id, name) \
		const uint16_t name = id;
#endif // !defined(PERSISTENT_STORAGE_ALLOCATE)

/**
 * This namespace contains ID allocations for sections of persistent storage.
 *
 * An ID is a uint16_t consisting of two one byte fields:
 *   MSB: Vendor ID
 *   LSB: Record ID (vendor specific)
 *
 * If you write any module or variant of this IPMC which makes use of persistent
 * storage, please reserve your Vendor ID by making a pull request to the main
 * repository.
 *
 * Application specific configurations can use APPLICATION_CONFIG.
 */
namespace PersistentStorageAllocations {
	extern std::map<uint16_t, std::string> *id_to_name; ///< Allocation ID to Name mapping.
	extern std::map<std::string, uint16_t> *name_to_id; ///< Allocation Name to ID mapping.

	/* Vendor 0: RESERVED */
	PERSISTENT_STORAGE_ALLOCATE(0x0000, RESERVED_END_OF_INDEX); ///< A marker to internally denote the end of the index.
	/* Vendor 1: University of Wisconsin */
	PERSISTENT_STORAGE_ALLOCATE(0x0101, WISC_SDR_REPOSITORY); ///< The SDR repository.
	PERSISTENT_STORAGE_ALLOCATE(0x0102, WISC_INFLUXDB_CONFIG); ///< InfluxDB configuration.
	PERSISTENT_STORAGE_ALLOCATE(0x0103, WISC_NETWORK_AUTH); ///< Auth configuration for network services.
	PERSISTENT_STORAGE_ALLOCATE(0x0104, WISC_FRU_DATA); ///< The FRU Data Area.
	/* Application specific, not a vendor */
	PERSISTENT_STORAGE_ALLOCATE(0xFE00, APPLICATION_CONFIG); ///< For application configuration.
};

#undef PERSISTENT_STORAGE_ALLOCATE

#endif /* SRC_COMMON_ZYNQIPMC_SERVICES_PERSISTENTSTORAGE_PERSISTENT_STORAGE_H_ */
