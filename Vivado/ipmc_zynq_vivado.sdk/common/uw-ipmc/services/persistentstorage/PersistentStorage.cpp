/*
 * PersistentStorage.cpp
 *
 *  Created on: Jan 2, 2018
 *      Author: jtikalsky
 */

#define __PERSISTENT_STORAGE_CPP
#include <services/persistentstorage/PersistentStorage.h>
#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>
#include <event_groups.h>
#include <IPMC.h>
#include <drivers/spi_eeprom/SPIEEPROM.h>
#include <drivers/tracebuffer/TraceBuffer.h>
#include <libs/SkyRoad.h>
#include <services/console/CommandParser.h>
#include <services/console/ConsoleSvc.h>
#include <string.h>

#define NVREG32(baseptr, offset) (*((uint32_t*)((baseptr)+(offset))))

static void run_persistentstorage_thread(void *cb_ps) {
	reinterpret_cast<PersistentStorage*>(cb_ps)->run_flush_thread();
}

/**
 * Instantiate a Persistent Storage module backed by the supplied EEPROM.
 *
 * @param eeprom  The EEPROM providing the backing for this module.
 * @param logtree Where to send log messages from this module
 * @param watchdog The watchdog to register with & service
 */
PersistentStorage::PersistentStorage(EEPROM &eeprom, LogTree &logtree, PS_WDT *watchdog)
	: eeprom(eeprom), logtree(logtree), wdt(watchdog) {
	configASSERT((eeprom.size / eeprom.page_size) <= UINT16_MAX); // Ensure that the EEPROM will not overflow our u16 fields.
	this->cache = (u8*)pvPortMalloc(this->eeprom.size*2 + 4);
	NVREG32(this->cache, this->eeprom.size) = 0x1234dead; // Set Canary
	this->data = this->cache + this->eeprom.size + 4;
	this->logtree.log("Persistent storage task starting.", LogTree::LOG_INFO);
	this->storage_loaded = xEventGroupCreate();
	this->index_mutex = xSemaphoreCreateMutex();
	this->flushq_mutex = xSemaphoreCreateMutex();
	// We are a driver task until the initial load is complete, then will change to a background task.
	if (this->wdt) {
		this->wdt_slot = this->wdt->register_slot(this->flush_ticks * 10); // We're background, but we should get service EVENTUALLY.
		this->wdt->activate_slot(this->wdt_slot);
	}
	configASSERT(xTaskCreate(run_persistentstorage_thread, "PersistentFlush", UWIPMC_STANDARD_STACK_SIZE, this, TASK_PRIORITY_DRIVER, &this->flushtask));
}

PersistentStorage::~PersistentStorage() {
	configASSERT(0); // Unsupported, no way to safely shutdown run_flush_thread at the moment.
	NVREG32(this->cache, this->eeprom.size) = 0; // Clear the canary for good measure.
	vSemaphoreDelete(this->flushq_mutex);
	vSemaphoreDelete(this->index_mutex);
	vEventGroupDelete(this->storage_loaded);
	vPortFree(this->cache);
}

/**
 * The global header for the persistent storage space.
 */
struct PersistentStorageHeader {
	u16 version; ///< The version of this persistent storage format.
};

/**
 * Return the current version of the specified section, or 0 if it does not exist.
 *
 * @param section_id The section to check the version of
 * @return The section version, or zero if absent.
 */
u16 PersistentStorage::get_section_version(u16 section_id) {
	xEventGroupWaitBits(this->storage_loaded, 1, 0, pdTRUE, portMAX_DELAY);
	xSemaphoreTake(this->index_mutex, portMAX_DELAY);
	struct PersistentStorageIndexRecord *index = reinterpret_cast<struct PersistentStorageIndexRecord *>(this->data + sizeof(struct PersistentStorageHeader));
	for (int i = 0; index[i].id != PersistentStorageAllocations::RESERVED_END_OF_INDEX; ++i) {
		if (index[i].id == section_id) {
			xSemaphoreGive(this->index_mutex);
			return index[i].version;
		}
	}
	xSemaphoreGive(this->index_mutex);
	return 0;
}

/**
 * Set the current version of the specified section, if it exists.
 *
 * @param section_id The section that should be updated
 * @param section_version The new version number of that section.
 */
void PersistentStorage::set_section_version(u16 section_id, u16 section_version) {
	xEventGroupWaitBits(this->storage_loaded, 1, 0, pdTRUE, portMAX_DELAY);
	xSemaphoreTake(this->index_mutex, portMAX_DELAY);
	struct PersistentStorageIndexRecord *index = reinterpret_cast<struct PersistentStorageIndexRecord *>(this->data + sizeof(struct PersistentStorageHeader));
	for (int i = 0; index[i].id != PersistentStorageAllocations::RESERVED_END_OF_INDEX; ++i) {
		if (index[i].id == section_id) {
			index[i].version = section_version;
			this->logtree.log(stdsprintf("PersistentStorage set section[%04hx].version = %04hx", section_id, section_version), LogTree::LOG_INFO);
		}
	}
	xSemaphoreGive(this->index_mutex);
	this->flush_index();
}

/**
 * Retrieve the specified persistent storage section, allocating it if necessary.
 *
 * When retrieving a persistent storage, the supplied version and size must
 * match the existing record or an error will occur.
 *
 * @param section_id  The ID of the section to allocate or retrieve.
 * @param section_version  The version of the section to allocate or retrieve.
 * @param section_size  The size of the section to allocate or retrieve.
 * @return A pointer to a memory of size section_size, backed by persistent storage, or NULL on error.
 */
void *PersistentStorage::get_section(u16 section_id, u16 section_version, u16 section_size) {
	configASSERT(section_id != PersistentStorageAllocations::RESERVED_END_OF_INDEX);
	xEventGroupWaitBits(this->storage_loaded, 1, 0, pdTRUE, portMAX_DELAY);
	xSemaphoreTake(this->index_mutex, portMAX_DELAY);
	struct PersistentStorageIndexRecord *index = reinterpret_cast<struct PersistentStorageIndexRecord *>(this->data + sizeof(struct PersistentStorageHeader));

	u16 section_pgcount = (section_size / this->eeprom.page_size) + (section_size % this->eeprom.page_size ? 1 : 0);
	int i;
	for (i = 0; index[i].id != PersistentStorageAllocations::RESERVED_END_OF_INDEX; ++i) {
		if (index[i].id == section_id) {
			if (index[i].version != section_version) {
				this->logtree.log(stdsprintf("Version mismatch retrieving persistent storage section 0x%04hx: %hu requested, %hu present.", section_id, section_version, index[i].version), LogTree::LOG_ERROR);
				xSemaphoreGive(this->index_mutex);
				return NULL;
			}
			if (index[i].pgcount != section_pgcount) {
				this->logtree.log(stdsprintf("Size mismatch retrieving persistent storage section 0x%04hx: %hu pages requested, %hu pages present.", section_id, section_pgcount, index[i].pgcount), LogTree::LOG_ERROR);
				xSemaphoreGive(this->index_mutex);
				return NULL;
			}
			xSemaphoreGive(this->index_mutex);
			this->logtree.log(stdsprintf("Persistent storage section[%04hx] (version = %04hx) retrieved.", section_id, section_version), LogTree::LOG_DIAGNOSTIC);
			return this->data + (index[i].pgoff * this->eeprom.page_size);
		}
	}

	/* If we got this far, the section doesn't exist yet.  We'll need to allocate it.
	 *
	 * index[i] is now the EOL marker.  We'll need to ensure we have space for
	 * one additional record (the new EOL), and find a space to allocate the
	 * storage section itself.
	 *
	 * One thing at a time.
	 */
	u16 minimum_address = sizeof(struct PersistentStorageHeader) + (i+2)*sizeof(struct PersistentStorageIndexRecord);
	u16 minimum_page = (minimum_address / this->eeprom.page_size) + (minimum_address % this->eeprom.page_size ? 1 : 0);

	// We start allocations from the end of EEPROM.
	u16 allocpg = (this->eeprom.size/this->eeprom.page_size)-section_pgcount;
	bool potential_overlap = true;
	while (allocpg < minimum_page && potential_overlap) {
		// Ok, find an overlap with this allocation.
		potential_overlap = false; // Unless we find something below, we know there is no overlap.
		for (int x = 0; index[x].id != PersistentStorageAllocations::RESERVED_END_OF_INDEX; ++x) {
			if (( allocpg >= index[x].pgoff && !(allocpg >= index[x].pgoff + index[x].pgcount) ) /* our start is in this range */ ||
				( allocpg+section_pgcount >= index[x].pgoff && !(allocpg+section_pgcount >= index[x].pgoff + index[x].pgcount) ) /* our end is in this range */ ) {
				// We overlap.  Move us to before the start of this section.
				potential_overlap = true;
				if (index[x].pgoff < section_pgcount)
					allocpg = 0; // No further space, we'll find our way out through the failure path.
				else
					allocpg = index[x].pgoff - section_pgcount;
			}
		}
	}
	if (allocpg < minimum_page) {
		// We failed to find a valid allocation.
		this->logtree.log(stdsprintf("Unable to allocate %hu contiguous pages for persistent storage section 0x%04hx.", section_pgcount, section_id), LogTree::LOG_ERROR);
		xSemaphoreGive(this->index_mutex);
		return NULL;
	}
	else {
		// Record the allocation.
		index[i].id = section_id;
		index[i].pgoff = allocpg;
		index[i].pgcount = section_pgcount;
		index[i].version = section_version;

		// Mark the new terminator.
		index[i+1].id = PersistentStorageAllocations::RESERVED_END_OF_INDEX;

		this->logtree.log(stdsprintf("Persistent storage section[0x%04hx] (version = %hu) allocated at 0x%04hx for %hu pages.", section_id, section_version, index[i].pgoff, index[i].pgcount), LogTree::LOG_DIAGNOSTIC);
		xSemaphoreGive(this->index_mutex);
		this->flush_index();
		return this->data + (index[i].pgoff * this->eeprom.page_size);
	}
}

/**
 * Delete all instances of the specified persistent storage section.
 *
 * @param section_id The ID of the section to delete.
 */
void PersistentStorage::delete_section(u16 section_id) {
	xEventGroupWaitBits(this->storage_loaded, 1, 0, pdTRUE, portMAX_DELAY);
	struct PersistentStorageIndexRecord *index = reinterpret_cast<struct PersistentStorageIndexRecord *>(this->data + sizeof(struct PersistentStorageHeader));
	xSemaphoreTake(this->index_mutex, portMAX_DELAY);
	for (int i = 0; index[i].id != PersistentStorageAllocations::RESERVED_END_OF_INDEX; ++i) {
		if (index[i].id == section_id) {
			this->logtree.log(stdsprintf("Deleting persistent storage allocation for section 0x%04hx (version %hu) at 0x%04hx, freeing %hu pages.", index[i].id, index[i].version, index[i].pgoff, index[i].pgcount), LogTree::LOG_NOTICE);
			for (int x = i; index[x].id != PersistentStorageAllocations::RESERVED_END_OF_INDEX; ++x)
				index[x] = index[x+1];
			i--; // Recheck this index, it's not the same record anymore.
		}
	}
	xSemaphoreGive(this->index_mutex);
	this->flush_index();
}

/**
 * Return a list of all persistent storage sections.
 * @return The list.
 */
std::vector<struct PersistentStorage::PersistentStorageIndexRecord> PersistentStorage::list_sections() {
	xEventGroupWaitBits(this->storage_loaded, 1, 0, pdTRUE, portMAX_DELAY);
	struct PersistentStorageIndexRecord *index = reinterpret_cast<struct PersistentStorageIndexRecord *>(this->data + sizeof(struct PersistentStorageHeader));
	xSemaphoreTake(this->index_mutex, portMAX_DELAY);
	std::vector<struct PersistentStorageIndexRecord> sections;
	for (int i = 0; index[i].id != PersistentStorageAllocations::RESERVED_END_OF_INDEX; ++i)
		sections.push_back(index[i]);
	xSemaphoreGive(this->index_mutex);
	return std::move(sections);
}

/**
 * Enqueue an immediate flush of the full EEPROM.
 *
 * @param completion_cb A callback to call upon completion.  (Triggers priority inheritance if not NULL.)
 */
void PersistentStorage::flush(std::function<void(void)> completion_cb) {
	this->logtree.log("Requesting full storage flush", LogTree::LOG_DIAGNOSTIC);
	this->flush(0, this->eeprom.size, completion_cb);
}

/**
 * Enqueue an immediate flush of a subset of EEPROM.
 *
 * @param start A pointer to the start of the range to be flushed.
 * @param len The length in bytes of the range to be flushed.
 * @param completion_cb A callback to call upon completion.  (Triggers priority inheritance if not NULL.)
 */
void PersistentStorage::flush(void *start, size_t len, std::function<void(void)> completion_cb) {
	configASSERT(start >= this->data && start <= this->data + this->eeprom.size - len);
	u32 start_addr = reinterpret_cast<u8*>(start) - this->data;
	u32 end_addr = start_addr + len;
	this->logtree.log(stdsprintf("Requesting flush of range [%u, %u)", start_addr, end_addr), LogTree::LOG_DIAGNOSTIC);
	xSemaphoreTake(this->flushq_mutex, portMAX_DELAY);
	FlushRequest req(start_addr, end_addr, completion_cb);
	if (!this->flushq.empty() && this->flushq.top().index_flush && !!req.complete) {
		// The current top task is an index flush, and we're blocking on a callback.  Make sure it has inherited our priority.
		FlushRequest index_req = this->flushq.top();
		this->flushq.pop();
		if (index_req.process_priority < req.process_priority)
			index_req.process_priority = req.process_priority;
		this->flushq.push(index_req);
	}
	this->flushq.push(req);
	if (completion_cb) {
		uint32_t myprio = uxTaskPriorityGet(NULL);
		uint32_t flushprio = uxTaskPriorityGet(this->flushtask);
		if (myprio > flushprio)
			vTaskPrioritySet(this->flushtask, myprio);
	}
	xTaskNotifyGive(this->flushtask);
	xSemaphoreGive(this->flushq_mutex);
}

/**
 * Put in a priority flush request for the index.
 *
 * \note This will take the index_mutex to auto-calculate the index length.
 */
void PersistentStorage::flush_index() {
	xSemaphoreTake(this->index_mutex, portMAX_DELAY);
	struct PersistentStorageIndexRecord *index = reinterpret_cast<struct PersistentStorageIndexRecord *>(this->data + sizeof(struct PersistentStorageHeader));
	int i;
	for (i = 0; index[i].id != 0; ++i)
		;
	++i; // We want i to be the length of index, but we bailed from the loop on, not after, the terminator.
	u32 index_length = sizeof(PersistentStorageHeader) + i*sizeof(PersistentStorageIndexRecord);
	xSemaphoreGive(this->index_mutex);

	this->logtree.log(stdsprintf("Requesting flush of index (length %u)", index_length), LogTree::LOG_DIAGNOSTIC);

	xSemaphoreTake(this->flushq_mutex, portMAX_DELAY);
	if (!this->flushq.empty() && this->flushq.top().index_flush) {
		// We already have an index refresh on top, just check the bounds.
		FlushRequest existing = this->flushq.top();
		this->flushq.pop();
		existing.end = index_length;
		this->flushq.push(existing);
	}
	else {
		// We don't have an index refresh yet, we need to add one.
		FlushRequest req(0, index_length, NULL, true);
		if (!this->flushq.empty()) {
			// Inherit priority, if relevant, so we don't deprioritize anyone's job.
			if (this->flushq.top().process_priority > req.process_priority)
				req.process_priority = this->flushq.top().process_priority;
		}
		this->flushq.push(req);
	}
	// We didn't do any priority update of our own on the thread, as we don't mind this being background until pushed by a later flush.
	xTaskNotifyGive(this->flushtask);
	xSemaphoreGive(this->flushq_mutex);
}

/**
 * Instantiate a FlushRequest record
 *
 * @param start Start of range
 * @param end   End of range
 * @param complete_cb Completion callback
 * @param index_flush Indicate whether this is an index flush (and thus max priority).
 */
PersistentStorage::FlushRequest::FlushRequest(u32 start, u32 end, std::function<void(void)> complete_cb, bool index_flush)
	: start(start), end(end), process_priority(uxTaskPriorityGet(NULL)), complete(complete_cb), requested_at(get_tick64()), index_flush(index_flush) {
	// Done!
}

/// A comparison used to manage the priority queue.
bool PersistentStorage::FlushRequest::operator <(const FlushRequest &other) const {
	u32 mine = 0;
	u32 theirs = 0;

	// Index flushes always have priority.
	if (this->index_flush)
		mine |= 0x80000000;
	if (other.index_flush)
		theirs |= 0x80000000;

	// Interactive flushes have priority based on process priority, for inheritance.
	if (this->complete)
		mine |= 0x40000000 | this->process_priority;
	if (other.complete)
		theirs |= 0x40000000 | this->process_priority;

	if (mine < theirs)
		return true;
	else if (mine == theirs)
		return other.requested_at < this->requested_at; // Older request is higher priority.
	else // (theirs < mine)
		return false;
}

void PersistentStorage::run_flush_thread() {
	this->logtree.log("Loading persistent storage.", LogTree::LOG_INFO);
	this->eeprom.read(0, this->cache, this->eeprom.size);
	this->logtree.log("Loaded persistent storage.", LogTree::LOG_INFO);
	memcpy(this->data, this->cache, this->eeprom.size);

	struct PersistentStorageHeader *hdr = reinterpret_cast<struct PersistentStorageHeader*>(this->data);
	if (hdr->version == 0 || hdr->version == 0xffff) {
		// Uninitialized
		hdr->version = 1;
		struct PersistentStorageIndexRecord *secrec1 = reinterpret_cast<struct PersistentStorageIndexRecord*>(this->data + sizeof(struct PersistentStorageHeader));
		secrec1->id = PersistentStorageAllocations::RESERVED_END_OF_INDEX; // Initialize the EOL marker.
		this->logtree.log("Persistent storage first use initialization complete.", LogTree::LOG_NOTICE);
	}
	else if (hdr->version != 1) {
		this->logtree.log(stdsprintf("Persistent storage version %hhu not recognized, persistent storage REFORMATTED.", hdr->version), LogTree::LOG_CRITICAL);
		hdr->version = 1;
		struct PersistentStorageIndexRecord *secrec1 = reinterpret_cast<struct PersistentStorageIndexRecord*>(this->data + sizeof(struct PersistentStorageHeader));
		secrec1->id = PersistentStorageAllocations::RESERVED_END_OF_INDEX; // Initialize the EOL marker.
	}
	xEventGroupSetBits(this->storage_loaded, 1);
	vTaskPrioritySet(NULL, TASK_PRIORITY_BACKGROUND); // Now background.

	AbsoluteTimeout next_bg_flush(get_tick64() + flush_ticks);
	while (true) {
		if (!ulTaskNotifyTake(pdTRUE, next_bg_flush.get_timeout())) {
			// Nothing new.  Let's enqueue a full flush.
			xSemaphoreTake(this->flushq_mutex, portMAX_DELAY);
			// NULL callback means our current priority is irrelevant.
			this->flushq.emplace(0, this->eeprom.size, std::function<void(void)>());
			next_bg_flush.timeout64 = get_tick64() + this->flush_ticks;
			xSemaphoreGive(this->flushq_mutex);
		}

		bool changed = false;
		bool done = false;
		while (!done) {
			if (this->wdt)
				this->wdt->service_slot(this->wdt_slot); // This will get hit by our regularly enqueued full flush, in any case.
			// Step 1: Check the Canary.
			if (0x1234dead != NVREG32(this->cache, this->eeprom.size)) {
				this->logtree.log("Canary INVALID.  There has been a buffer overrun in the vicinity of the persistent storage system. EEPROM flushes are PERMANENTLY DISABLED.", LogTree::LOG_CRITICAL);
				configASSERT(0); // We're done.  We can't trust our cache or comparisons.
			}

			xSemaphoreTake(this->flushq_mutex, portMAX_DELAY);
			FlushRequest request = this->flushq.top();
			this->flushq.pop();
			if (!!request.complete || request.index_flush) // Index flushes inherit priority but don't have completion callbacks.
				vTaskPrioritySet(NULL, request.process_priority); // Someone's blocking, therefore inherit.
			else
				vTaskPrioritySet(NULL, TASK_PRIORITY_BACKGROUND); // Noone's blocking, therefore disinherit.
			xSemaphoreGive(this->flushq_mutex);

			if (this->do_flush_range(request.start, request.end))
				changed = true;
			if (!!request.complete)
				request.complete(); // Notify

			xSemaphoreTake(this->flushq_mutex, portMAX_DELAY);
			if (this->flushq.empty()) {
				vTaskPrioritySet(NULL, TASK_PRIORITY_BACKGROUND); // Disinherit before waiting.
				done = true;
			}
			xSemaphoreGive(this->flushq_mutex);
		}
		if (changed)
			this->logtree.log("Changes to persistent storage have been flushed to EEPROM.", LogTree::LOG_INFO);
	}
}

/**
 * Flush a given range of the raw persistent storage.
 *
 * @param start The first byte of the range
 * @param end The first byte after the range
 * @return true if changes were flushed, else false
 */
bool PersistentStorage::do_flush_range(u32 start, u32 end) {
	this->logtree.log(stdsprintf("Flushing range [%u, %u)", start, end), LogTree::LOG_DIAGNOSTIC);
	start -= start % this->eeprom.page_size; // Round start down to page boundary.
	if (end % this->eeprom.page_size)
		end += this->eeprom.page_size - (end % this->eeprom.page_size);

	bool changed = false;
	for (u32 pgaddr = start; pgaddr < end; pgaddr += this->eeprom.page_size) {
		bool differ = false;
		for (u32 i = pgaddr; i < pgaddr+this->eeprom.page_size; ++i) {
			if (this->data[i] != this->cache[i]) {
				differ = true;
				break;
			}
		}
		if (!differ)
			continue; // Already clean.
		this->logtree.log(stdsprintf("Difference found at 0x%lx", pgaddr), LogTree::LOG_TRACE);
		TRACE.log(this->logtree.path.c_str(), this->logtree.path.size(), LogTree::LOG_TRACE, reinterpret_cast<char*>(this->cache+pgaddr), this->eeprom.page_size, true);
		TRACE.log(this->logtree.path.c_str(), this->logtree.path.size(), LogTree::LOG_TRACE, reinterpret_cast<char*>(this->data+pgaddr), this->eeprom.page_size, true);

		if (this->eeprom.write(pgaddr, this->data+pgaddr, this->eeprom.page_size) != this->eeprom.page_size) {
			this->logtree.log(stdsprintf("EEPROM write failed during flush in Persistent Storage service at 0x%04lx", pgaddr), LogTree::LOG_ERROR);
		}
		else {
			memcpy(this->cache + pgaddr, this->data + pgaddr, this->eeprom.page_size); // Update cache mirror to match EEPROM
			changed = true;
		}
	}
	return changed;
}

namespace {
/// A "list_sections" console command.
class ConsoleCommand_list_sections : public CommandParser::Command {
public:
	PersistentStorage &storage; ///< The associated storage for this command.

	/// Instantiate
	ConsoleCommand_list_sections(PersistentStorage &storage) : storage(storage) { };

	virtual std::string get_helptext(const std::string &command) const {
		return stdsprintf(
				"%s\n"
				"\n"
				"Lists all persistent storage sections.\n", command.c_str());
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		std::string out;
		std::vector<struct PersistentStorage::PersistentStorageIndexRecord> sections = this->storage.list_sections();
		for (auto it = sections.begin(), eit = sections.end(); it != eit; ++it) {
			std::string name = "UNKNOWN!  MISSING ALLOCATION!";
			if (PersistentStorageAllocations::id_to_name->count(it->id))
				name = PersistentStorageAllocations::id_to_name->at(it->id);
			out += stdsprintf("Section 0x%04hx (ver %hu), at pages 0x%04hx-0x%04hx: %s\n", it->id, it->version, it->pgoff, it->pgoff + it->pgcount - 1, name.c_str());
		}
		console->write(out);
	}

	//virtual std::vector<std::string> complete(const CommandParser::CommandParameters &parameters) const { };
};

/// A "read" console command.
class ConsoleCommand_read : public CommandParser::Command {
public:
	PersistentStorage &storage; ///< The associated storage for this command.

	/// Instantiate
	ConsoleCommand_read(PersistentStorage &storage) : storage(storage) { };

	virtual std::string get_helptext(const std::string &command) const {
		return stdsprintf(
				"%s $section $start $length\n"
				"\n"
				"Read from a given section.\n", command.c_str());
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		u16 sect_id = 0;
		std::string sect_name;
		u32 start;
		u32 length;
		if (!parameters.parse_parameters(1, true, &sect_id, &start, &length)) {
			// Fine, parse $section as a string.
			if (!parameters.parse_parameters(1, true, &sect_name, &start, &length)) {
				print("Invalid parameters.  See help.\n");
				return;
			}
			if (PersistentStorageAllocations::name_to_id->count(sect_name))
				sect_id = PersistentStorageAllocations::name_to_id->at(sect_name);
		}

		if (!sect_id) {
			print("Invalid section.\n");
			return;
		}

		// We have to look the section up to get its version and size so we can request it.
		std::vector<struct PersistentStorage::PersistentStorageIndexRecord> sections = this->storage.list_sections();
		for (auto it = sections.begin(), eit = sections.end(); it != eit; ++it) {
			if (it->id == sect_id) {
				u8 *buf = (u8*)this->storage.get_section(it->id, it->version, it->pgcount*this->storage.eeprom.page_size);
				if (!buf) {
					print("Failed to fetch section!\n");
					return;
				}
				std::string out;
				for (u32 i = start; i < start+length && i < (it->pgcount*this->storage.eeprom.page_size); ++i)
					out += stdsprintf(" %2hhx", buf[i]);
				console->write(stdsprintf("0x%04hx[0x%04lx:0x%04lx]:", sect_id, start, start+length) + out + "\n");
				return;
			}
		}
		print("Section not found.\n");
	}

	virtual std::vector<std::string> complete(const CommandParser::CommandParameters &parameters) const {
		if (parameters.cursor_parameter != 1)
			return std::vector<std::string>(); // Sorry, can't help.

		std::vector<std::string> ret;
		for (auto it = PersistentStorageAllocations::name_to_id->begin(), eit = PersistentStorageAllocations::name_to_id->end(); it != eit; ++it)
			ret.push_back(it->first);
		return ret;
	};
};

/// A "write" console command.
class ConsoleCommand_write : public CommandParser::Command {
public:
	PersistentStorage &storage; ///< The associated storage for this command.

	/// Instantiate
	ConsoleCommand_write(PersistentStorage &storage) : storage(storage) { };

	virtual std::string get_helptext(const std::string &command) const {
		return stdsprintf(
				"%s $section $start $byte [...]\n"
				"\n"
				"Write to a given section.\n", command.c_str());
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		u16 sect_id = 0;
		std::string sect_name;
		u32 start;
		if (!parameters.parse_parameters(1, false, &sect_id, &start)) {
			// Fine, parse $section as a string.
			if (!parameters.parse_parameters(1, false, &sect_name, &start)) {
				print("Invalid parameters.  See help.\n");
				return;
			}
			if (PersistentStorageAllocations::name_to_id->count(sect_name))
				sect_id = PersistentStorageAllocations::name_to_id->at(sect_name);
		}

		if (!sect_id) {
			print("Invalid section.\n");
			return;
		}

		const CommandParser::CommandParameters::size_type writebytecount = parameters.nargs()-3;
		u8 writebytes[writebytecount];
		for (CommandParser::CommandParameters::size_type i = 0; i < parameters.nargs()-3; ++i) {
			if (!parameters.parse_parameters(i+3, false, &writebytes[i])) {
				print("Unable to parse input bytes.\n");
				return;
			}
		}

		// We have to look the section up to get its version and size so we can request it.
		std::vector<struct PersistentStorage::PersistentStorageIndexRecord> sections = this->storage.list_sections();
		for (auto it = sections.begin(), eit = sections.end(); it != eit; ++it) {
			if (it->id == sect_id) {
				u8 *buf = (u8*)this->storage.get_section(it->id, it->version, it->pgcount*storage.eeprom.page_size);
				if (!buf) {
					print("Failed to fetch section!\n");
					return;
				}
				std::string out;
				if (start + writebytecount > (it->pgcount*this->storage.eeprom.page_size)) {
					print("This write would overflow the region.  Cancelled.\n");
					return;
				}
				for (u32 i = 0; i < writebytecount; ++i)
					buf[start+i] = writebytes[i];
				console->write(stdsprintf("0x%04hx[0x%04lx:0x%04lx] written.\n", sect_id, start, start+writebytecount));
				this->storage.flush(buf+start, writebytecount);
				return;
			}
		}
		print("Section not found.\n");
	}

	virtual std::vector<std::string> complete(const CommandParser::CommandParameters &parameters) const {
		if (parameters.cursor_parameter != 1)
			return std::vector<std::string>(); // Sorry, can't help.

		std::vector<std::string> ret;
		for (auto it = PersistentStorageAllocations::name_to_id->begin(), eit = PersistentStorageAllocations::name_to_id->end(); it != eit; ++it)
			ret.push_back(it->first);
		return ret;
	};
};

/// A "set_section_version" console command.
class ConsoleCommand_set_section_version : public CommandParser::Command {
public:
	PersistentStorage &storage; ///< The associated storage for this command.

	/// Instantiate
	ConsoleCommand_set_section_version(PersistentStorage &storage) : storage(storage) { };

	virtual std::string get_helptext(const std::string &command) const {
		return stdsprintf(
				"%s $section $version [$size]\n"
				"\n"
				"Sets the version number of a given section, automatically creating it if a size is specified.\n", command.c_str());
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		u16 sect_id = 0;
		std::string sect_name;
		u16 version;
		u16 size = 0;

		if (parameters.parse_parameters(1, true, &sect_id, &version, &size)) {
			// Okay!
		}
		else if (parameters.parse_parameters(1, true, &sect_id, &version)) {
			// Still okay!
		}
		else if (parameters.parse_parameters(1, true, &sect_name, &version, &size)) {
			// Fine, parse $section as a string.
			if (PersistentStorageAllocations::name_to_id->count(sect_name))
				sect_id = PersistentStorageAllocations::name_to_id->at(sect_name);
		}
		else if (parameters.parse_parameters(1, true, &sect_name, &version)) {
			// Fine, parse $section as a string, and there's no size specified.
			if (PersistentStorageAllocations::name_to_id->count(sect_name))
				sect_id = PersistentStorageAllocations::name_to_id->at(sect_name);
		}
		else {
			print("Invalid parameters.\n");
			return;
		}

		if (!sect_id) {
			print("Invalid section.\n");
			return;
		}

		if (size) {
			if (this->storage.get_section(sect_id, version, size)) {
				print("Section created or already correct.\n");
				return;
			}
			else if (this->storage.get_section(sect_id, this->storage.get_section_version(sect_id), size)) {
				// Section exists.  Version will be updated below.
			}
			else {
				print("Section exists with a different size.  Aborting.\n");
				return;
			}
		}
		this->storage.set_section_version(sect_id, version);
	}

	virtual std::vector<std::string> complete(const CommandParser::CommandParameters &parameters) const {
		if (parameters.cursor_parameter != 1)
			return std::vector<std::string>(); // Sorry, can't help.

		std::vector<std::string> ret;
		for (auto it = PersistentStorageAllocations::name_to_id->begin(), eit = PersistentStorageAllocations::name_to_id->end(); it != eit; ++it)
			ret.push_back(it->first);
		return ret;
	};
};

/// A "delete_section" console command.
class ConsoleCommand_delete_section : public CommandParser::Command {
public:
	PersistentStorage &storage; ///< The associated storage for this command.

	/// Instantiate
	ConsoleCommand_delete_section(PersistentStorage &storage) : storage(storage) { };

	virtual std::string get_helptext(const std::string &command) const {
		return stdsprintf(
				"%s $section\n"
				"\n"
				"Deletes a given storage section.\n"
				"\n"
				"*************************************************\n"
				"* DO NOT DO THIS IF IT IS IN USE BY OTHER CODE. *\n"
				"*************************************************\n", command.c_str());
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		u16 sect_id = 0;
		std::string sect_name;

		if (parameters.parse_parameters(1, true, &sect_id)) {
			// Okay!
		}
		else if (parameters.parse_parameters(1, true, &sect_name)) {
			// Fine, parse $section as a string, and there's no size specified.
			if (PersistentStorageAllocations::name_to_id->count(sect_name))
				sect_id = PersistentStorageAllocations::name_to_id->at(sect_name);
		}
		else {
			print("Invalid parameters.\n");
			return;
		}

		if (!sect_id) {
			print("Invalid section.\n");
			return;
		}

		storage.delete_section(sect_id);
	}

	// What? No. I'm not helping you **** this up. You'd better mean it.
	//virtual std::vector<std::string> complete(const CommandParser::CommandParameters &parameters) const { };
};
} // anonymous namespace

/**
 * Register console commands related to this storage.
 * @param parser The command parser to register to.
 * @param prefix A prefix for the registered commands.
 */
void PersistentStorage::register_console_commands(CommandParser &parser, const std::string &prefix) {
	parser.register_command(prefix + "list_sections", std::make_shared<ConsoleCommand_list_sections>(*this));
	parser.register_command(prefix + "read", std::make_shared<ConsoleCommand_read>(*this));
	parser.register_command(prefix + "write", std::make_shared<ConsoleCommand_write>(*this));
	parser.register_command(prefix + "set_section_version", std::make_shared<ConsoleCommand_set_section_version>(*this));
	parser.register_command(prefix + "delete_section", std::make_shared<ConsoleCommand_delete_section>(*this));
}

/**
 * Unregister console commands related to this storage.
 * @param parser The command parser to unregister from.
 * @param prefix A prefix for the registered commands.
 */
void PersistentStorage::deregister_console_commands(CommandParser &parser, const std::string &prefix) {
	parser.register_command(prefix + "list_sections", NULL);
	parser.register_command(prefix + "read", NULL);
	parser.register_command(prefix + "write", NULL);
	parser.register_command(prefix + "set_section_version", NULL);
	parser.register_command(prefix + "delete_section", NULL);
}

std::map<u16, std::string> *PersistentStorageAllocations::id_to_name;
std::map<std::string, u16> *PersistentStorageAllocations::name_to_id;
