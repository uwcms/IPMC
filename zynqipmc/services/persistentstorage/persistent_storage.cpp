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

// This must precede the PersistentStorage.h include to fill the static data.
#define PERSISTENT_STORAGE_ALLOCATE(id, name) \
	const uint16_t name = id; \
	static void _ps_reg_section_ ## name() __attribute__((constructor(1000))); \
	static void _ps_reg_section_ ## name() { \
		if (!name_to_id) \
			name_to_id = new std::map<std::string, uint16_t>(); \
		if (!id_to_name) \
			id_to_name = new std::map<uint16_t, std::string>(); \
		(*name_to_id)[#name] = id; \
		(*id_to_name)[id] = #name; \
	}

#include "persistent_storage.h"
#include <string>
#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>
#include <event_groups.h>
#include <core.h>
#include <drivers/tracebuffer/tracebuffer.h>
#include <services/console/consolesvc.h>
#include <libs/printf.h>

#define NVREG32(baseptr, offset) (*((uint32_t*)((baseptr)+(offset))))

std::map<uint16_t, std::string> *PersistentStorageAllocations::id_to_name;
std::map<std::string, uint16_t> *PersistentStorageAllocations::name_to_id;

static inline uint16_t pageCount(uint16_t size, uint16_t page_size) {
	return (size / page_size) + (size % page_size ? 1 : 0);
}

PersistentStorage::PersistentStorage(EEPROM &eeprom, LogTree &logtree, PSWDT *watchdog)
	: eeprom(eeprom), logtree(logtree), wdt(watchdog) {
	if ((eeprom.getTotalSize() / eeprom.getPageSize()) > UINT16_MAX) // Ensure that the EEPROM will not overflow our uint16_t fields.
		throw std::domain_error("Only EEPROMs up to UINT16_MAX in length are supported.");
	this->cache = (uint8_t*)malloc(this->eeprom.getTotalSize()*2 + 4);
	NVREG32(this->cache, this->eeprom.getTotalSize()) = 0x1234dead; // Set Canary
	this->data = this->cache + this->eeprom.getTotalSize() + 4;
	this->logtree.log("Persistent storage task starting.", LogTree::LOG_INFO);
	this->storage_loaded = xEventGroupCreate();
	this->index_mutex = xSemaphoreCreateMutex();
	this->flushq_mutex = xSemaphoreCreateMutex();
	// We are a driver task until the initial load is complete, then will change to a background task.
	if (this->wdt) {
		this->wdt_slot = this->wdt->register_slot(this->flush_ticks * 10); // We're background, but we should get service EVENTUALLY.
		this->wdt->activate_slot(this->wdt_slot);
	}
	this->flushtask = runTask("PersistentFlush", TASK_PRIORITY_DRIVER, [this]() -> void { this->flushThread(); });
}

PersistentStorage::~PersistentStorage() {
	configASSERT(0); // Unsupported, no way to safely shutdown flushThread at the moment.
	NVREG32(this->cache, this->eeprom.getTotalSize()) = 0; // Clear the canary for good measure.
	vSemaphoreDelete(this->flushq_mutex);
	vSemaphoreDelete(this->index_mutex);
	vEventGroupDelete(this->storage_loaded);
	free(this->cache);
}

//! The global header for the persistent storage space.
struct PersistentStorageHeader {
	uint16_t version; ///< The version of this persistent storage format.
};

uint16_t PersistentStorage::getSectionVersion(uint16_t section_id) {
	xEventGroupWaitBits(this->storage_loaded, 1, 0, pdTRUE, portMAX_DELAY);
	MutexGuard<false> lock(this->index_mutex, true);
	struct PersistentStorageIndexRecord *index = reinterpret_cast<struct PersistentStorageIndexRecord *>(this->data + sizeof(struct PersistentStorageHeader));
	for (int i = 0; index[i].id != PersistentStorageAllocations::RESERVED_END_OF_INDEX; ++i)
		if (index[i].id == section_id)
			return index[i].version;

	return 0;
}

void PersistentStorage::setSectionVersion(uint16_t section_id, uint16_t section_version) {
	xEventGroupWaitBits(this->storage_loaded, 1, 0, pdTRUE, portMAX_DELAY);

	MutexGuard<false> lock(this->index_mutex, true);
	struct PersistentStorageIndexRecord *index = reinterpret_cast<struct PersistentStorageIndexRecord *>(this->data + sizeof(struct PersistentStorageHeader));
	for (int i = 0; index[i].id != PersistentStorageAllocations::RESERVED_END_OF_INDEX; ++i) {
		if (index[i].id == section_id) {
			index[i].version = section_version;
			this->logtree.log(stdsprintf("PersistentStorage set section[%04hx].version = %04hx", section_id, section_version), LogTree::LOG_INFO);
		}
	}

	lock.release();
	this->flushIndex();
}

/**
 * Calculate whether two ranges overlap.
 *
 * @param baseA The base of the first range
 * @param sizeA The size of the first range
 * @param baseB The base of the second range
 * @param sizeB The size of the second range
 * @return true if the ranges overlap, else false
 */
static inline bool rangesOverlap(uint16_t baseA, uint16_t sizeA, uint16_t baseB, uint16_t sizeB) {
	if (baseB >= (baseA+sizeA))
		return false; // Range B is entirely above Range A
	if (baseA >= (baseB+sizeB))
		return false; // Range A is entirely above Range B
	return true; // Neither range is entirely above the other.
}

void *PersistentStorage::getSection(uint16_t section_id, uint16_t section_version, uint16_t section_size) {
	if (section_id == PersistentStorageAllocations::RESERVED_END_OF_INDEX)
		std::domain_error("It is not possible to request the section \"RESERVED_END_OF_INDEX\".");
	xEventGroupWaitBits(this->storage_loaded, 1, 0, pdTRUE, portMAX_DELAY);
	MutexGuard<false> lock(this->index_mutex, true);
	struct PersistentStorageIndexRecord *index = reinterpret_cast<struct PersistentStorageIndexRecord *>(this->data + sizeof(struct PersistentStorageHeader));

	uint16_t section_pgcount = pageCount(section_size, this->eeprom.getPageSize());
	int i;
	for (i = 0; index[i].id != PersistentStorageAllocations::RESERVED_END_OF_INDEX; ++i) {
		if (index[i].id == section_id) {
			if (index[i].version != section_version) {
				this->logtree.log(stdsprintf("Version mismatch retrieving persistent storage section 0x%04hx: %hu requested, %hu present.", section_id, section_version, index[i].version), LogTree::LOG_ERROR);
				return nullptr;
			}

			if (index[i].pgcount < section_pgcount) {
				this->logtree.log(stdsprintf("Size mismatch retrieving persistent storage section 0x%04hx: %hu pages requested, %hu pages present.", section_id, section_pgcount, index[i].pgcount), LogTree::LOG_ERROR);
				return nullptr;
			}

			lock.release();
			this->logtree.log(stdsprintf("Persistent storage section[%04hx] (version = %04hx) retrieved.", section_id, section_version), LogTree::LOG_DIAGNOSTIC);
			return this->data + (index[i].pgoff * this->eeprom.getPageSize());
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
	uint16_t minimum_address = sizeof(struct PersistentStorageHeader) + (i+2)*sizeof(struct PersistentStorageIndexRecord);
	uint16_t minimum_page = pageCount(minimum_address, this->eeprom.getPageSize());

	// We start allocations from the end of EEPROM.
	uint16_t allocpg = (this->eeprom.getTotalSize()/this->eeprom.getPageSize())-section_pgcount;
	bool potential_overlap = true;
	while (allocpg >= minimum_page && potential_overlap) {
		// Ok, find an overlap with this allocation.
		potential_overlap = false; // Unless we find something below, we know there is no overlap.
		for (int x = 0; index[x].id != PersistentStorageAllocations::RESERVED_END_OF_INDEX; ++x) {
			if (rangesOverlap(allocpg, section_pgcount, index[x].pgoff, index[x].pgcount)) {
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
		return nullptr;
	} else {
		// Record the allocation.
		index[i].id = section_id;
		index[i].pgoff = allocpg;
		index[i].pgcount = section_pgcount;
		index[i].version = section_version;

		// Mark the new terminator.
		index[i+1].id = PersistentStorageAllocations::RESERVED_END_OF_INDEX;

		this->logtree.log(stdsprintf("Persistent storage section[0x%04hx] (version = %hu) allocated at 0x%04hx for %hu pages.", section_id, section_version, index[i].pgoff, index[i].pgcount), LogTree::LOG_DIAGNOSTIC);
		lock.release();
		this->flushIndex();
		return this->data + (index[i].pgoff * this->eeprom.getPageSize());
	}
}

void PersistentStorage::deleteSection(uint16_t section_id) {
	xEventGroupWaitBits(this->storage_loaded, 1, 0, pdTRUE, portMAX_DELAY);
	struct PersistentStorageIndexRecord *index = reinterpret_cast<struct PersistentStorageIndexRecord *>(this->data + sizeof(struct PersistentStorageHeader));
	MutexGuard<false> lock(this->index_mutex, true);

	for (int i = 0; index[i].id != PersistentStorageAllocations::RESERVED_END_OF_INDEX; ++i) {
		if (index[i].id == section_id) {
			this->logtree.log(stdsprintf("Deleting persistent storage allocation for section 0x%04hx (version %hu) at 0x%04hx, freeing %hu pages.", index[i].id, index[i].version, index[i].pgoff, index[i].pgcount), LogTree::LOG_NOTICE);
			for (int x = i; index[x].id != PersistentStorageAllocations::RESERVED_END_OF_INDEX; ++x)
				memcpy(&(index[x]), &(index[x+1]), sizeof(struct PersistentStorageIndexRecord));
			i--; // Recheck this index, it's not the same record anymore.
		}
	}

	lock.release();
	this->flushIndex();
}

std::vector<struct PersistentStorage::PersistentStorageIndexRecord> PersistentStorage::listSections() {
	xEventGroupWaitBits(this->storage_loaded, 1, 0, pdTRUE, portMAX_DELAY);
	struct PersistentStorageIndexRecord *index = reinterpret_cast<struct PersistentStorageIndexRecord *>(this->data + sizeof(struct PersistentStorageHeader));
	MutexGuard<false> lock(this->index_mutex, true);
	std::vector<struct PersistentStorageIndexRecord> sections;

	for (int i = 0; index[i].id != PersistentStorageAllocations::RESERVED_END_OF_INDEX; ++i)
		sections.push_back(index[i]);

	lock.release();
	return std::move(sections);
}

void PersistentStorage::deinitializeStorage() {
	xEventGroupWaitBits(this->storage_loaded, 1, 0, pdTRUE, portMAX_DELAY);
	struct PersistentStorageHeader *header = reinterpret_cast<struct PersistentStorageHeader *>(this->data);
	MutexGuard<false> lock(this->index_mutex, true);
	this->logtree.log("Deinitialized persistent storage.", LogTree::LOG_NOTICE);
	header->version = 0xffff; // Flag as uninitialized EEPROM
	lock.release();
	this->flushIndex();
}

void PersistentStorage::flush(std::function<void(void)> completion_cb) {
	this->logtree.log("Requesting full storage flush", LogTree::LOG_DIAGNOSTIC);
	this->flush(this->data, this->eeprom.getTotalSize(), completion_cb);
}

void PersistentStorage::flush(void *start, size_t len, std::function<void(void)> completion_cb) {
	if (!(start >= this->data && start <= this->data + this->eeprom.getTotalSize() - len))
		throw std::domain_error("The requested flush range exceeds the storage memory space.");
	uint32_t start_addr = reinterpret_cast<uint8_t*>(start) - this->data;
	uint32_t end_addr = start_addr + len;
	this->logtree.log(stdsprintf("Requesting flush of range [%lu, %lu)", start_addr, end_addr), LogTree::LOG_DIAGNOSTIC);
	MutexGuard<false> lock(this->index_mutex, true);
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
		uint32_t myprio = uxTaskPriorityGet(nullptr);
		uint32_t flushprio = uxTaskPriorityGet(this->flushtask);
		if (myprio > flushprio)
			vTaskPrioritySet(this->flushtask, myprio);
	}
	xTaskNotifyGive(this->flushtask);
}

void PersistentStorage::flushIndex() {
	MutexGuard<false> indexlock(this->index_mutex, true);
	struct PersistentStorageIndexRecord *index = reinterpret_cast<struct PersistentStorageIndexRecord *>(this->data + sizeof(struct PersistentStorageHeader));
	int i;
	for (i = 0; index[i].id != 0; ++i)
		;
	++i; // We want i to be the length of index, but we bailed from the loop on, not after, the terminator.
	uint32_t index_length = sizeof(PersistentStorageHeader) + i*sizeof(PersistentStorageIndexRecord);
	indexlock.release();

	this->logtree.log(stdsprintf("Requesting flush of index (length %lu)", index_length), LogTree::LOG_DIAGNOSTIC);
	this->traceIndex();

	MutexGuard<false> flushlock(this->flushq_mutex, true);
	if (!this->flushq.empty() && this->flushq.top().index_flush) {
		// We already have an index refresh on top, just check the bounds.
		FlushRequest existing = this->flushq.top();
		this->flushq.pop();
		existing.end = index_length;
		this->flushq.push(existing);
	}
	else {
		// We don't have an index refresh yet, we need to add one.
		FlushRequest req(0, index_length, nullptr, true);
		if (!this->flushq.empty()) {
			// Inherit priority, if relevant, so we don't deprioritize anyone's job.
			if (this->flushq.top().process_priority > req.process_priority)
				req.process_priority = this->flushq.top().process_priority;
		}
		this->flushq.push(req);
	}
	// We didn't do any priority update of our own on the thread, as we don't mind this being background until pushed by a later flush.
	xTaskNotifyGive(this->flushtask);
}

PersistentStorage::FlushRequest::FlushRequest(uint32_t start, uint32_t end, std::function<void(void)> complete_cb, bool index_flush)
	: start(start), end(end), process_priority(uxTaskPriorityGet(nullptr)), complete(complete_cb), requested_at(get_tick64()), index_flush(index_flush) {
	// Done!
}

bool PersistentStorage::FlushRequest::operator <(const FlushRequest &other) const {
	uint32_t mine = 0;
	uint32_t theirs = 0;

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

void PersistentStorage::flushThread() {
	this->logtree.log("Loading persistent storage.", LogTree::LOG_INFO);
	this->eeprom.read(0, this->cache, this->eeprom.getTotalSize());
	this->logtree.log("Loaded persistent storage.", LogTree::LOG_INFO);
	memcpy(this->data, this->cache, this->eeprom.getTotalSize());

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
	this->traceIndex();
	vTaskPrioritySet(nullptr, TASK_PRIORITY_BACKGROUND); // Now background.

	AbsoluteTimeout next_bg_flush(get_tick64() + flush_ticks);
	while (true) {
		if (!ulTaskNotifyTake(pdTRUE, next_bg_flush.getTimeout())) {
			// Nothing new.  Let's enqueue a full flush.
			MutexGuard<false> flushlock(this->flushq_mutex, true);
			// nullptr callback means our current priority is irrelevant.
			this->flushq.emplace(0, this->eeprom.getTotalSize(), std::function<void(void)>());
			next_bg_flush.setAbsTimeout(get_tick64() + this->flush_ticks);
		}

		bool changed = false;
		bool done = false;
		while (!done) {
			if (this->wdt)
				this->wdt->service_slot(this->wdt_slot); // This will get hit by our regularly enqueued full flush, in any case.

			// Step 1: Check the Canary.
			if (0x1234dead != NVREG32(this->cache, this->eeprom.getTotalSize())) {
				this->logtree.log("Canary INVALID.  There has been a buffer overrun in the vicinity of the persistent storage system. EEPROM flushes are PERMANENTLY DISABLED.", LogTree::LOG_CRITICAL);
				configASSERT(0); // We're done.  We can't trust our cache or comparisons.
			}

			MutexGuard<false> flushlock(this->flushq_mutex, true);
			FlushRequest request = this->flushq.top();
			this->flushq.pop();
			if (!!request.complete || request.index_flush) { // Index flushes inherit priority but don't have completion callbacks.
				vTaskPrioritySet(nullptr, request.process_priority); // Someone's blocking, therefore inherit.
			} else {
				vTaskPrioritySet(nullptr, TASK_PRIORITY_BACKGROUND); // Noone's blocking, therefore disinherit.
			}
			flushlock.release();

			if (this->doFlushRange(request.start, request.end))
				changed = true;

			if (!!request.complete)
				request.complete(); // Notify

			flushlock.acquire();
			if (this->flushq.empty()) {
				vTaskPrioritySet(nullptr, TASK_PRIORITY_BACKGROUND); // Disinherit before waiting.
				done = true;
			}
			flushlock.release();
		}

		if (changed)
			this->logtree.log("Changes to persistent storage have been flushed to EEPROM.", LogTree::LOG_INFO);
	}
}

bool PersistentStorage::doFlushRange(uint32_t start, uint32_t end) {
	this->logtree.log(stdsprintf("Flushing range [%lu, %lu)", start, end), LogTree::LOG_DIAGNOSTIC);
	start -= start % this->eeprom.getPageSize(); // Round start down to page boundary.
	if (end % this->eeprom.getPageSize())
		end += this->eeprom.getPageSize() - (end % this->eeprom.getPageSize());

	bool changed = false;
	for (uint32_t pgaddr = start; pgaddr < end; pgaddr += this->eeprom.getPageSize()) {
		bool differ = false;
		for (uint32_t i = pgaddr; i < pgaddr+this->eeprom.getPageSize(); ++i) {
			if (this->data[i] != this->cache[i]) {
				differ = true;
				break;
			}
		}
		if (!differ)
			continue; // Already clean.
		this->logtree.log(stdsprintf("Difference found at 0x%lx", pgaddr), LogTree::LOG_TRACE);
		TRACE.log(this->logtree.getPath().c_str(), this->logtree.getPath().size(), LogTree::LOG_TRACE, reinterpret_cast<char*>(this->cache+pgaddr), this->eeprom.getPageSize(), true);
		TRACE.log(this->logtree.getPath().c_str(), this->logtree.getPath().size(), LogTree::LOG_TRACE, reinterpret_cast<char*>(this->data+pgaddr), this->eeprom.getPageSize(), true);

		if (this->eeprom.write(pgaddr, this->data+pgaddr, this->eeprom.getPageSize()) != this->eeprom.getPageSize()) {
			this->logtree.log(stdsprintf("EEPROM write failed during flush in Persistent Storage service at 0x%04lx", pgaddr), LogTree::LOG_ERROR);
		}
		else {
			memcpy(this->cache + pgaddr, this->data + pgaddr, this->eeprom.getPageSize()); // Update cache mirror to match EEPROM
			changed = true;
		}
	}
	return changed;
}

void PersistentStorage::traceIndex() {
	MutexGuard<false> indexlock(this->index_mutex, true);
	struct PersistentStorageIndexRecord *index = reinterpret_cast<struct PersistentStorageIndexRecord *>(this->data + sizeof(struct PersistentStorageHeader));
	std::string out;
	int i;
	for (i = 0; index[i].id != 0; ++i)
		out += stdsprintf("\t<section id=\"0x%04hx\" offset=\"0x%04hx\" end=\"0x%04hx\" pagecount=\"%hu\" version=\"%hu\"/>\n", index[i].id, index[i].pgoff, index[i].pgoff + index[i].pgcount - 1, index[i].pgcount, index[i].version);
	this->logtree.log(stdsprintf("\n<index length=\"%d\">\n", i) + out + "</index>", LogTree::LOG_TRACE);
}

std::vector<uint8_t> VariablePersistentAllocation::getData() const {
	MutexGuard<false> lock(this->mutex, true);
	uint16_t version = this->storage.getSectionVersion(this->id);
	if (version == 0)
		return std::vector<uint8_t>(); // No storage.
	if (version != 1)
		throw std::runtime_error(stdsprintf("We support only record version 1, not record version %hu (found).", version)); // Unsupported!  We have no valid response to return, and this wasn't written by us.

	// Retrieve two byte length header.
	uint16_t *data_size = reinterpret_cast<uint16_t*>(this->storage.getSection(this->id, 1, 2));
	if (!data_size)
		throw std::runtime_error("The storage record is corrupt."); // So there's data, but we can't retrieve our header?

	uint8_t *data = reinterpret_cast<uint8_t*>(this->storage.getSection(this->id, 1, 2+*data_size));
	return std::vector<uint8_t>(data+2, data+2+*data_size);
}

bool VariablePersistentAllocation::setData(const std::vector<uint8_t> &data, std::function<void(void)> flush_completion_cb) {
	MutexGuard<false> lock(this->mutex, true);
	uint16_t version = this->storage.getSectionVersion(this->id);

	if (version == 0 || version == 1) {
		uint8_t *pdata = nullptr;
		if (version == 1) {
			// Content exists, let's reuse the allocation if it's the right size.
			// Retrieve data.  If this fails, it's because our new data is too big and we needta reallocate anyway.
			pdata = reinterpret_cast<uint8_t*>(this->storage.getSection(this->id, 1, 2 + data.size()));
			if (pdata) {
				// An allocation exists that is at least large enough, do we need to shrink it?
				uint16_t *data_size = reinterpret_cast<uint16_t*>(pdata);
				if (pageCount(*data_size, this->storage.eeprom.getPageSize()) != pageCount(data.size(), this->storage.eeprom.getPageSize()))
					pdata = nullptr; // Yep, different pagecount from desired.  Reallocate.
			}
		}

		if (!pdata) {
			// We need to (re)allocate the persistent storage space.
			this->storage.deleteSection(this->id);
			pdata = reinterpret_cast<uint8_t*>(this->storage.getSection(this->id, 1, 2 + data.size()));
			if (!pdata)
				return false; // Allocation failed?
		}

		// Time to actually write the data!
		*reinterpret_cast<uint16_t*>(pdata) = data.size();
		uint8_t *pdataptr = pdata + 2;
		for (auto it = data.begin(), eit = data.end(); it != eit; ++it)
			*(pdataptr++) = *it;
		this->storage.flush(pdata, 2+data.size(), flush_completion_cb);
		return true;
	} else {
		return false; // This isn't ours.
	}
}

/// A "listSections" console command.
class PersistentStorage::ListSectionsCommand : public CommandParser::Command {
public:
	ListSectionsCommand(PersistentStorage &storage) : storage(storage) { };

	virtual std::string getHelpText(const std::string &command) const {
		return command + "\n\n"
				"Lists all persistent storage sections.\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		std::string out;
		std::vector<struct PersistentStorage::PersistentStorageIndexRecord> sections = this->storage.listSections();
		for (auto it = sections.begin(), eit = sections.end(); it != eit; ++it) {
			std::string name = "UNKNOWN!  MISSING ALLOCATION!";
			if (PersistentStorageAllocations::id_to_name->count(it->id))
				name = PersistentStorageAllocations::id_to_name->at(it->id);
			out += stdsprintf("Section 0x%04hx (ver %hu), at pages 0x%04hx-0x%04hx: %s\n", it->id, it->version, it->pgoff, it->pgoff + it->pgcount - 1, name.c_str());
		}
		console->write(out);
	}

private:
	PersistentStorage &storage;
};

/// A "read" console command.
class PersistentStorage::ReadCommand : public CommandParser::Command {
public:
	ReadCommand(PersistentStorage &storage) : storage(storage) { };

	virtual std::string getHelpText(const std::string &command) const {
		return command + " $section $start $length\n\n"
				"Read from a given section.\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		uint16_t sect_id = 0;
		std::string sect_name;
		uint32_t start;
		uint32_t length;

		if (!parameters.parseParameters(1, true, &sect_id, &start, &length)) {
			// Fine, parse $section as a string.
			if (!parameters.parseParameters(1, true, &sect_name, &start, &length)) {
				console->write("Invalid parameters.  See help.\n");
				return;
			}

			if (PersistentStorageAllocations::name_to_id->count(sect_name))
				sect_id = PersistentStorageAllocations::name_to_id->at(sect_name);
		}

		if (!sect_id) {
			console->write("Invalid section.\n");
			return;
		}

		// We have to look the section up to get its version and size so we can request it.
		std::vector<struct PersistentStorage::PersistentStorageIndexRecord> sections = this->storage.listSections();
		for (auto it = sections.begin(), eit = sections.end(); it != eit; ++it) {
			if (it->id == sect_id) {
				uint8_t *buf = (uint8_t*)this->storage.getSection(it->id, it->version, it->pgcount*this->storage.eeprom.getPageSize());
				if (!buf) {
					console->write("Failed to fetch section!\n");
					return;
				}

				std::string out;
				for (uint32_t i = start; i < start+length && i < (it->pgcount*this->storage.eeprom.getPageSize()); ++i)
					out += stdsprintf(" %02hhx", buf[i]);
				console->write(stdsprintf("0x%04hx[0x%04lx:0x%04lx]:", sect_id, start, start+length) + out + "\n");
				return;
			}
		}
		console->write("Section not found.\n");
	}

	virtual std::vector<std::string> complete(const CommandParser::CommandParameters &parameters) const {
		if (parameters.cursor_parameter != 1)
			return std::vector<std::string>(); // Sorry, can't help.

		std::vector<std::string> ret;
		for (auto it = PersistentStorageAllocations::name_to_id->begin(), eit = PersistentStorageAllocations::name_to_id->end(); it != eit; ++it)
			ret.push_back(it->first);
		return ret;
	};

private:
	PersistentStorage &storage;
};

/// A "write" console command.
class PersistentStorage::WriteCommand : public CommandParser::Command {
public:
	WriteCommand(PersistentStorage &storage) : storage(storage) { };

	virtual std::string getHelpText(const std::string &command) const {
		return command + " $section $start $byte [...]\n\n"
				"Write to a given section.\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		uint16_t sect_id = 0;
		std::string sect_name;
		uint32_t start;

		if (!parameters.parseParameters(1, false, &sect_id, &start)) {
			// Fine, parse $section as a string.
			if (!parameters.parseParameters(1, false, &sect_name, &start)) {
				console->write("Invalid parameters.  See help.\n");
				return;
			}

			if (PersistentStorageAllocations::name_to_id->count(sect_name))
				sect_id = PersistentStorageAllocations::name_to_id->at(sect_name);
		}

		if (!sect_id) {
			console->write("Invalid section.\n");
			return;
		}

		const CommandParser::CommandParameters::size_type writebytecount = parameters.nargs()-3;
		uint8_t writebytes[writebytecount];
		for (CommandParser::CommandParameters::size_type i = 0; i < parameters.nargs()-3; ++i) {
			if (!parameters.parseParameters(i+3, false, &writebytes[i])) {
				console->write("Unable to parse input bytes.\n");
				return;
			}
		}

		// We have to look the section up to get its version and size so we can request it.
		std::vector<struct PersistentStorage::PersistentStorageIndexRecord> sections = this->storage.listSections();
		for (auto it = sections.begin(), eit = sections.end(); it != eit; ++it) {
			if (it->id == sect_id) {
				uint8_t *buf = (uint8_t*)this->storage.getSection(it->id, it->version, it->pgcount*storage.eeprom.getPageSize());
				if (!buf) {
					console->write("Failed to fetch section!\n");
					return;
				}

				std::string out;

				if (start + writebytecount > (it->pgcount*this->storage.eeprom.getPageSize())) {
					console->write("This write would overflow the region.  Cancelled.\n");
					return;
				}

				for (uint32_t i = 0; i < writebytecount; ++i)
					buf[start+i] = writebytes[i];
				console->write(stdsprintf("0x%04hx[0x%04lx:0x%04lx] written.\n", sect_id, start, start+writebytecount));
				this->storage.flush(buf+start, writebytecount);
				return;
			}
		}
		console->write("Section not found.\n");
	}

	virtual std::vector<std::string> complete(const CommandParser::CommandParameters &parameters) const {
		if (parameters.cursor_parameter != 1)
			return std::vector<std::string>(); // Sorry, can't help.

		std::vector<std::string> ret;
		for (auto it = PersistentStorageAllocations::name_to_id->begin(), eit = PersistentStorageAllocations::name_to_id->end(); it != eit; ++it)
			ret.push_back(it->first);
		return ret;
	};

private:
	PersistentStorage &storage;
};

/// A "set_section_version" console command.
class PersistentStorage::SetSectionVersionCommand : public CommandParser::Command {
public:
	SetSectionVersionCommand(PersistentStorage &storage) : storage(storage) { };

	virtual std::string getHelpText(const std::string &command) const {
		return command + " $section $version [$size]\n\n"
				"Sets the version number of a given section, automatically creating it if a size is specified.\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		uint16_t sect_id = 0;
		std::string sect_name;
		uint16_t version;
		uint16_t size = 0;

		if (parameters.parseParameters(1, true, &sect_id, &version, &size)) {
			// Okay!
		} else if (parameters.parseParameters(1, true, &sect_id, &version)) {
			// Still okay!
		} else if (parameters.parseParameters(1, true, &sect_name, &version, &size)) {
			// Fine, parse $section as a string.
			if (PersistentStorageAllocations::name_to_id->count(sect_name))
				sect_id = PersistentStorageAllocations::name_to_id->at(sect_name);
		} else if (parameters.parseParameters(1, true, &sect_name, &version)) {
			// Fine, parse $section as a string, and there's no size specified.
			if (PersistentStorageAllocations::name_to_id->count(sect_name))
				sect_id = PersistentStorageAllocations::name_to_id->at(sect_name);
		} else {
			console->write("Invalid parameters.\n");
			return;
		}

		if (!sect_id) {
			console->write("Invalid section.\n");
			return;
		}

		if (size) {
			if (this->storage.getSection(sect_id, version, size)) {
				console->write("Section created or already correct.\n");
				return;
			} else if (this->storage.getSection(sect_id, this->storage.getSectionVersion(sect_id), size)) {
				// Section exists.  Version will be updated below.
			} else {
				console->write("Section exists with a different size.  Aborting.\n");
				return;
			}
		}
		this->storage.setSectionVersion(sect_id, version);
	}

	virtual std::vector<std::string> complete(const CommandParser::CommandParameters &parameters) const {
		if (parameters.cursor_parameter != 1)
			return std::vector<std::string>(); // Sorry, can't help.

		std::vector<std::string> ret;
		for (auto it = PersistentStorageAllocations::name_to_id->begin(), eit = PersistentStorageAllocations::name_to_id->end(); it != eit; ++it)
			ret.push_back(it->first);
		return ret;
	};

private:
	PersistentStorage &storage;
};

/// A "deleteSection" console command.
class PersistentStorage::DeleteSectionCommand : public CommandParser::Command {
public:
	DeleteSectionCommand(PersistentStorage &storage) : storage(storage) { };

	virtual std::string getHelpText(const std::string &command) const {
		return command + " $section\n\n"
				"Deletes a given storage section.\n"
				"\n"
				"*************************************************\n"
				"* DO NOT DO THIS IF IT IS IN USE BY OTHER CODE. *\n"
				"*************************************************\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		uint16_t sect_id = 0;
		std::string sect_name;

		if (parameters.parseParameters(1, true, &sect_id)) {
			// Okay!
		} else if (parameters.parseParameters(1, true, &sect_name)) {
			// Fine, parse $section as a string, and there's no size specified.
			if (PersistentStorageAllocations::name_to_id->count(sect_name))
				sect_id = PersistentStorageAllocations::name_to_id->at(sect_name);
		} else {
			console->write("Invalid parameters.\n");
			return;
		}

		if (!sect_id) {
			console->write("Invalid section.\n");
			return;
		}

		storage.deleteSection(sect_id);
	}

private:
	PersistentStorage &storage;
};


/// A "reinitialize_storage" console command.
class PersistentStorage::ReinitializeStorageCommand : public CommandParser::Command {
public:
	ReinitializeStorageCommand(PersistentStorage &storage) : storage(storage) { };

	virtual std::string getHelpText(const std::string &command) const {
		return command + "\n\n"
				"Mark the entire persistent storage as uninitialized.\n"
				"\n"
				"The storage will be automatically reinitialized at next boot.\n"
				"\n"
				"This is not a secure erase.\n"; // Don't want to waste write cycles.
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		storage.deinitializeStorage();
		console->write("Persistent Storage has been marked for reinitialization on next boot.\n");
	}

private:
	PersistentStorage &storage;
};

void PersistentStorage::registerConsoleCommands(CommandParser &parser, const std::string &prefix) {
	parser.registerCommand(prefix + "list_sections", std::make_shared<PersistentStorage::ListSectionsCommand>(*this));
	parser.registerCommand(prefix + "read", std::make_shared<PersistentStorage::ReadCommand>(*this));
	parser.registerCommand(prefix + "write", std::make_shared<PersistentStorage::WriteCommand>(*this));
	parser.registerCommand(prefix + "set_section_version", std::make_shared<PersistentStorage::SetSectionVersionCommand>(*this));
	parser.registerCommand(prefix + "deleteSection", std::make_shared<PersistentStorage::DeleteSectionCommand>(*this));
	parser.registerCommand(prefix + "reinitialize_storage", std::make_shared<PersistentStorage::ReinitializeStorageCommand>(*this));
}

void PersistentStorage::deregisterConsoleCommands(CommandParser &parser, const std::string &prefix) {
	parser.registerCommand(prefix + "list_sections", nullptr);
	parser.registerCommand(prefix + "read", nullptr);
	parser.registerCommand(prefix + "write", nullptr);
	parser.registerCommand(prefix + "set_section_version", nullptr);
	parser.registerCommand(prefix + "deleteSection", nullptr);
	parser.registerCommand(prefix + "reinitialize_storage", nullptr);
}

