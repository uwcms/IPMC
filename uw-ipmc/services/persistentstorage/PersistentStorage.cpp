/*
 * PersistentStorage.cpp
 *
 *  Created on: Jan 2, 2018
 *      Author: jtikalsky
 */

#include <services/persistentstorage/PersistentStorage.h>
#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>
#include <event_groups.h>
#include <IPMC.h>
#include <drivers/spi_eeprom/SPIEEPROM.h>
#include <libs/SkyRoad.h>
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
 */
PersistentStorage::PersistentStorage(EEPROM &eeprom, LogTree &logtree)
	: eeprom(eeprom), logtree(logtree) {
	configASSERT((eeprom.size / eeprom.page_size) <= UINT16_MAX); // Ensure that the EEPROM will not overflow our u16 fields.
	this->cache = (u8*)pvPortMalloc(this->eeprom.size*2 + 4);
	NVREG32(this->cache, this->eeprom.size) = 0x1234dead; // Set Canary
	this->data = this->cache + this->eeprom.size + 4;
	this->logtree.log("Persistent storage task starting.", LogTree::LOG_INFO);
	this->storage_loaded = xEventGroupCreate();
	this->flushwait[0] = new WaitList();
	this->flushwait[1] = new WaitList();
	this->index_mutex = xSemaphoreCreateMutex();
	this->prio_mutex = xSemaphoreCreateMutex();
	// We are a driver task until the initial load is complete, then will change to a background task.
	configASSERT(xTaskCreate(run_persistentstorage_thread, "PersistentFlush", configMINIMAL_STACK_SIZE+256, this, TASK_PRIORITY_DRIVER, &this->flushtask));
}

PersistentStorage::~PersistentStorage() {
	configASSERT(0); // Unsupported, no way to safely shutdown run_flush_thread at the moment.
	NVREG32(this->cache, this->eeprom.size) = 0; // Clear the canary for good measure.
	vSemaphoreDelete(this->prio_mutex);
	vSemaphoreDelete(this->index_mutex);
	delete this->flushwait[1];
	delete this->flushwait[0];
	vEventGroupDelete(this->storage_loaded);
	vPortFree(this->data);
}

/**
 * The global header for the persistent storage space.
 */
struct PersistentStorageHeader {
	u16 version; ///< The version of this persistent storage format.
};

/**
 * An entry in the persistent storage section index.
 */
struct PersistentStorageIndexRecord {
	u16 id;      ///< The ID of the section.
	u16 pgoff;   ///< The page number of the section start.
	u16 pgcount; ///< The length in pages of the section.
	u16 version; ///< The version of the section.
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
	for (int i = 0; index[i].id != PersistentStorageAllocations::RESERVED_END_OF_INDEX; ++i)
		if (index[i].id == section_id)
			index[i].version = section_version;
	xSemaphoreGive(this->index_mutex);
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
				this->logtree.log(stdsprintf("Version mismatch retrieving persistent storage section %hu: %hu requested, %hu present.", section_id, section_version, index[i].version), LogTree::LOG_ERROR);
				xSemaphoreGive(this->index_mutex);
				return NULL;
			}
			if (index[i].pgcount != section_pgcount) {
				this->logtree.log(stdsprintf("Size mismatch retrieving persistent storage section %hu: %hu pages requested, %hu pages present.", section_id, section_pgcount, index[i].pgcount), LogTree::LOG_ERROR);
				xSemaphoreGive(this->index_mutex);
				return NULL;
			}
			xSemaphoreGive(this->index_mutex);
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
		this->logtree.log(stdsprintf("Unable to allocate %hu contiguous pages for persistent storage section %hu.", section_pgcount, section_id), LogTree::LOG_ERROR);
		xSemaphoreGive(this->index_mutex);
		return NULL;
	}
	else {
		// Record the allocation.
		index[i].id = section_id;
		index[i].pgoff = allocpg;;
		index[i].pgcount = section_pgcount;
		index[i].version = section_version;

		// Mark the new terminator.
		index[i+1].id = PersistentStorageAllocations::RESERVED_END_OF_INDEX;

		xSemaphoreGive(this->index_mutex);
		return this->data + (index[i].pgoff * this->eeprom.page_size); // The flush order will ensure we are flushed before any data in the new section.
	}
}

/**
 * Delete all instances of the specified persistent storage section.
 *
 * \note This will perform a blocking flush.
 *
 * @param section_id The ID of the section to delete.
 */
void PersistentStorage::delete_section(u16 section_id) {
	xEventGroupWaitBits(this->storage_loaded, 1, 0, pdTRUE, portMAX_DELAY);
	struct PersistentStorageIndexRecord *index = reinterpret_cast<struct PersistentStorageIndexRecord *>(this->data + sizeof(struct PersistentStorageHeader));
	xSemaphoreTake(this->index_mutex, portMAX_DELAY);
	for (int i = 0; index[i].id != PersistentStorageAllocations::RESERVED_END_OF_INDEX; ++i) {
		if (index[i].id == section_id) {
			this->logtree.log(stdsprintf("Deleting persistent storage allocation for section %hu (version %hu) at %hu, freeing %hu pages.", index[i].id, index[i].version, index[i].pgoff, index[i].pgcount), LogTree::LOG_NOTICE);
			for (int x = i; index[x].id != PersistentStorageAllocations::RESERVED_END_OF_INDEX; ++x)
				index[x] = index[x+1];
			i--; // Recheck this index, it's not the same record anymore.
		}
	}
	xSemaphoreGive(this->index_mutex);
	this->flush(portMAX_DELAY); // We have to flush, to ensure that the index is consistent.
}

/**
 * Flush cached EEPROM writes immediately.  If an optional timeout is specified,
 * this call will wait until the flush has been completed, and the flush thread
 * will inherit the priority of the calling task.
 *
 * \note Priority inheritance will persist for one flush cycle even if you time
 *       out before it completes, however the EEPROM writes themselves will
 *       still be asynchronous and interrupt based due to the driver.
 *
 * @param timeout How long to wait for a synchronous flush.
 * @return false if wait timed out, else true
 */
bool PersistentStorage::flush(TickType_t timeout) {
	WaitList *wl = this->flushwait[0];
	WaitList::Subscription sub;
	this->logtree.log("Requesting explicit flush of persistent storage.", LogTree::LOG_DIAGNOSTIC);
	uint32_t prio = 1 << TASK_PRIORITY_BACKGROUND;
	uint32_t my_priority = uxTaskPriorityGet(NULL);
	if (timeout) {
		sub = wl->join();
		prio = 1 << my_priority;
		xSemaphoreTake(this->prio_mutex, portMAX_DELAY);
	}
	/* We will notify the flush task, and supply it with "you should run one
	 * cycle with at least X priority".
	 */
	xTaskNotify(this->flushtask, prio, eSetBits);
	if (timeout) {
		/* We will now ensure that the flush task inherits at least up to our
		 * priority, so that it can receive the notification.
		 */
		if (my_priority > uxTaskPriorityGet(this->flushtask))
			vTaskPrioritySet(this->flushtask, my_priority);
		xSemaphoreGive(this->prio_mutex);
		return sub.wait(timeout);
	}
	return true;
}

void PersistentStorage::run_flush_thread() {
	this->logtree.log("Loading persistent storage.", LogTree::LOG_INFO);
	this->eeprom.read(0, this->cache, this->eeprom.size);
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
	while (true) {
		xSemaphoreTake(this->prio_mutex, portMAX_DELAY);
		/* We must now check if our priority should remain elevated by a new
		 * notification, or whether there are none pending and we should
		 * disinherit.
		 */
		uint32_t notify_value = 0;
		xTaskNotifyWait(0, 0xffffffff,  &notify_value, 0);
		if (notify_value) {
			/* We have something pending.
			 *
			 * Inherit the highest priority level of our pending notifications.
			 */
			vTaskPrioritySet(NULL, 31 - __builtin_clz(notify_value));
			xSemaphoreGive(this->prio_mutex);
		}
		else {
			// Nothing was pending.  Disinherit and wait for a notification.
			vTaskPrioritySet(NULL, TASK_PRIORITY_BACKGROUND);
			xSemaphoreGive(this->prio_mutex);
			// Wait for notification, or periodic 10hz background flush.
			notify_value = 0;
			xTaskNotifyWait(0, 0xffffffff, &notify_value, configTICK_RATE_HZ/10);
			if (notify_value)
				vTaskPrioritySet(NULL, 31 - __builtin_clz(notify_value)); // There's a priority to inherit.
		}

		// We have received notification but done no work yet, swap waitlists.
		WaitList *current_wl = this->flushwait[0];
		this->flushwait[0] = this->flushwait[1]; // New subscriptions will trigger, and wait for, another pass.
		this->flushwait[1] = current_wl;

		// Step 1: Check the Canary.
		if (0x1234dead != NVREG32(this->cache, this->eeprom.size)) {
			this->logtree.log("Canary INVALID.  There has been a buffer overrun in the vicinity of the persistent storage system. EEPROM flushes are PERMANENTLY DISABLED.", LogTree::LOG_CRITICAL);
			configASSERT(0); // We're done.  We can't trust our cache or comparisons.
		}

		bool changed = false;

		// We'll flush pages at a time.
		for (unsigned int addr = 0; addr < this->eeprom.size; addr += this->eeprom.page_size) {
			if (0 == memcmp(this->cache + addr, this->data + addr, this->eeprom.page_size))
				continue; // Already clean.

			if (this->eeprom.write(addr, this->data+addr, this->eeprom.page_size) != this->eeprom.page_size) {
				this->logtree.log(stdsprintf("EEPROM write failed during flush in Persistent Storage service at 0x%04x", addr), LogTree::LOG_ERROR);
			}
			else {
				memcpy(this->cache + addr, this->data + addr, this->eeprom.page_size); // Update cache mirror to match EEPROM
				changed = true;
			}
		}
		if (changed)
			this->logtree.log("Changes to persistent storage have been flushed to EEPROM.", LogTree::LOG_INFO);
		current_wl->wake();
	}
}
