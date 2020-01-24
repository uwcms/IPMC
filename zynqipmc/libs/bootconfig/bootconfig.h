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

#ifndef SRC_COMMON_ZYNQIPMC_LIBS_BOOTCONFIG_BOOTCONFIG_H_
#define SRC_COMMON_ZYNQIPMC_LIBS_BOOTCONFIG_BOOTCONFIG_H_

#include <drivers/generics/eeprom.h>
#include <libs/threading.h>
#include <services/persistentstorage/persistent_storage.h>

/**
 * This class manages all boot-image related configuration, including the
 * current/next boot target, the current/next update target, and the carrier
 * tag lock data.
 *
 * It allocates byte 2 of the supplied (non-persistent storage) EEPROM.
 * The format for this byte is:
 *   [7:3] Reserved
 *     [2] 0b = Image A is current.  1b = Image B is current.
 *   [1:0] Boot target (flash partition) (read by FSBL)
 */
class BootConfig {
public:
	enum PhysicalBootTarget {
		PBT_FALLBACK = 0,
		PBT_A        = 1,
		PBT_B        = 2,
		PBT_TEST     = 3,
	};
	enum LogicalBootTarget {
		LBT_FALLBACK = 0,
		LBT_PRIMARY  = 1,
		LBT_BACKUP   = 2,
		LBT_TEST     = 3,
	};

	BootConfig(EEPROM &eeprom, PersistentStorage &persistent_storage);
	virtual ~BootConfig();

	enum PhysicalBootTarget getPhysicalBootTarget() const;
	enum PhysicalBootTarget getPrimaryImage() const;
	enum LogicalBootTarget getLogicalBootTarget() const;
	enum PhysicalBootTarget mapLogicalToPhysicalBootTarget(enum LogicalBootTarget lbt) const;
	void switchPrimaryImage();
	void setLogicalBootTarget(enum LogicalBootTarget lbt);

	void flushBootTarget();

	std::string getImageTagLock() const;
	void setImageTagLock(std::string lock);
	bool testImageTagLock(std::string match_value) const;

protected:
	EEPROM &eeprom; ///< The hardware config EEPROM.
	uint8_t eepconfig; ///< An editable cache of the config EEPROM
	VariablePersistentAllocation lock_config_store; ///< The storage for the image tag lock tag.
	SemaphoreHandle_t mutex; ///< A mutex to prevent concurrent modification.
};

#endif /* SRC_COMMON_ZYNQIPMC_LIBS_BOOTCONFIG_BOOTCONFIG_H_ */
