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

#include "bootconfig.h"

static inline uint8_t eepconf_read(EEPROM &eeprom) {
	uint8_t eepconfig;
	configASSERT(eeprom.read(2, &eepconfig, 1) == 1);
	return eepconfig;
}

static inline void eepconf_write(EEPROM &eeprom, uint8_t eepconfig) {
	uint8_t oldconfig;
	configASSERT(eeprom.read(2, &oldconfig, 1) == 1);
	if (oldconfig != eepconfig)
		configASSERT(eeprom.write(2, &eepconfig, 1) == 1);
}

BootConfig::BootConfig(EEPROM &eeprom, PersistentStorage &persistent_storage) :
	eeprom(eeprom), lock_config_store(VariablePersistentAllocation(persistent_storage, PersistentStorageAllocations::WISC_IMAGE_TAG_LOCK)) {
	configASSERT(this->mutex = xSemaphoreCreateRecursiveMutex());
	this->eepconfig = eepconf_read(eeprom);
	if (this->eepconfig & 0x80) {
		 /* Handle potentially uninitialized EEPROM detected with bit (bit 7, 0x80).
		  * Don't mess with the Primary/Secondary switch bit. (bit 3, 0x08).
		  *
		  * This will reset the boot target to fallback, but not save this
		  * unless the boot target is set or an update is applied.
		  *
		  * This will avoid changing the primary image in flash unexpectedly.
		  */
		this->eepconfig &= 0x08;
	}
}

BootConfig::~BootConfig() {
	vSemaphoreDelete(this->mutex);
}

/**
 * Return the current physical boot target.
 * @return The current physical boot target.
 */
enum BootConfig::PhysicalBootTarget BootConfig::getPhysicalBootTarget() const {
	MutexGuard<true> lock(this->mutex);
	if (this->eepconfig & 0x04)
		return PBT_TEST;
	return static_cast<enum PhysicalBootTarget>(this->eepconfig & 0x03);
}

/**
 * Read and return the current logical boot target.
 * @return The current logical boot target.
 */
enum BootConfig::LogicalBootTarget BootConfig::getLogicalBootTarget() const {
	MutexGuard<true> lock(this->mutex);
	if (this->eepconfig & 0x04)
		return LBT_TEST;
	switch (this->eepconfig & 0x03) {
	case 0: return LBT_FALLBACK;
	case 1: return (this->eepconfig & 0x08) ? LBT_BACKUP : LBT_PRIMARY;
	case 2: return (this->eepconfig & 0x08) ? LBT_PRIMARY : LBT_BACKUP;
	case 3: return LBT_TEST;
	default: return LBT_FALLBACK; // wtf?
	}
}

/**
 * Switch which image is currently the primary.
 */
void BootConfig::switchPrimaryImage() {
	MutexGuard<true> lock(this->mutex);
	this->eepconfig ^= 0x08; // Swap designation.
	if ((this->eepconfig & 0x03) == 1 || (this->eepconfig & 0x03) == 2) // If a A/B image is active...
		this->eepconfig = (this->eepconfig & 0xfc) | ((this->eepconfig & 0x08) ? 2 : 1); // Set the current primary image to be active.
}

/**
 * Update the current logical boot target.
 * @param lbt The new logical boot target.
 */
void BootConfig::setLogicalBootTarget(enum BootConfig::LogicalBootTarget lbt) {
	MutexGuard<true> lock(this->mutex);
	switch (lbt) {
	case LBT_FALLBACK: this->eepconfig = (this->eepconfig & 0xf8) | 0; break;
	case LBT_PRIMARY:  this->eepconfig = (this->eepconfig & 0xf8) | ((this->eepconfig & 0x08) ? 2 : 1); break;
	case LBT_BACKUP:   this->eepconfig = (this->eepconfig & 0xf8) | ((this->eepconfig & 0x08) ? 1 : 2); break;
	case LBT_TEST:     this->eepconfig =  this->eepconfig         | 0x04; break;
	}
}

/**
 * Map a specified logical boot target to the corresponding physical boot target.
 * @param lbt The logical boot target to map.
 * @return The corresponding physical boot target
 */
enum BootConfig::PhysicalBootTarget BootConfig::mapLogicalToPhysicalBootTarget(enum LogicalBootTarget lbt) const {
	MutexGuard<true> lock(this->mutex);
	switch (lbt) {
	case 0: return PBT_FALLBACK;
	case 1: return (this->eepconfig & 0x08) ? PBT_B : PBT_A;
	case 2: return (this->eepconfig & 0x08) ? PBT_A : PBT_B;
	case 3: return PBT_TEST;
	default: return static_cast<enum PhysicalBootTarget>(lbt); // wtf?
	}
}

void BootConfig::flushBootTarget() {
	eepconf_write(this->eeprom, this->eepconfig); // "unmodified" optimization handled by eepconf_write()
}

/**
 * Get the current carrier lock value.
 * @return The current carrier lock value.
 */
std::string BootConfig::getCarrierLock() const {
	MutexGuard<true> lock(this->mutex);
	std::vector<uint8_t> storeData = this->lock_config_store.getData();
	if (storeData.size())
		return std::string(storeData.begin(), storeData.end());
	else
		return "?";
}

/**
 * Update the current carrier lock value.
 * @param lock The new carrier lock value.
 */
void BootConfig::setCarrierLock(std::string lock) {
	MutexGuard<true> mutexlock(this->mutex);
	this->lock_config_store.setData(std::vector<uint8_t>(lock.begin(), lock.end()));
}

/**
 * Test a value against the current carrier lock setting.
 * If the setting is not programmed, it is read as "?".
 *
 * The following always match:
 *   lock == "?"
 *   lock == "*"
 *   match == "fallback"
 *   match startswith "fallback-"
 *
 * @param match_value The carrier lock value to test.
 * @return true if matched, else false
 */
bool BootConfig::testCarrierLock(std::string match_value) const {
	MutexGuard<true> mutexlock(this->mutex);
	std::string lock = this->getCarrierLock();
	return (
			!lock.size() ||
			lock == "?" ||
			lock == "*" ||
			lock == match_value ||
			match_value == "fallback" ||
			match_value.substr(0, 9) == "fallback-");
}
