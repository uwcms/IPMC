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

#ifndef SRC_COMMON_ZYNQIPMC_DRIVERS_GENERICS_FLASH_H_
#define SRC_COMMON_ZYNQIPMC_DRIVERS_GENERICS_FLASH_H_

#include <libs/vfs/vfs.h>
#include <libs/logtree/logtree.h>
#include <libs/printf.h>
#include <libs/xilinx_image/xilinx_image.h>
#include <libs/bootconfig/bootconfig.h>
#include <stdint.h>
#include <zynqipmc_config.h>
#ifdef ENABLE_IPMI
#include <services/ipmi/m_state_machine.h>
extern MStateMachine *mstatemachine;
#endif
extern LogTree LOG; // We can't include <core.h> here, because it will create a loop since core.h requires class Flash.

/**
 * Abstract flash interface.
 * Before any operation takes place the flash needs to be initialized
 * by calling isInitialized when appropriate and internally this will
 * detect the flash type and size.
 * Only then read/write operations are allowed and only JEDEC compatible
 * flashes are supported.
 */
class Flash {
public:
	Flash() : initialized(false), parameters({0}) {};
	virtual ~Flash() {};

	//! true if the flash has already been initialized, false otherwise.
	inline bool isInitialized() { return this->initialized; };

	/**
	 * Initialize the flash by checking the JEDEC registers.
	 * Multiple calls will only trigger a single initialization.
	 * @return true if initialization successful (JEDEC compatible flash detected), false otherwise.
	 */
	virtual bool initialize() {
		if (this->isInitialized()) return true;

		if (!this->getJEDECInfo()) return false;
		if (!this->getManufacturerID()) return false;

		this->initialized = true;
		return true;
	};

	/**
	 * Read a set of bytes to a provided pre-allocated buffer.
	 * @param address Start address in the flash.
	 * @param buffer Pre-allocated buffer to store read data.
	 * @param bytes Number of bytes to read from flash.
	 * @return true if read was successful, false otherwise.
	 * @warning The sum of address and bytes cannot exceed flash total size, this will cause read to fail.
	 */
	virtual bool read(uint32_t address, uint8_t *buffer, size_t bytes) = 0;

	/**
	 * Write a set of bytes to the flash.
	 * @param address Start address in the flash.
	 * @param buffer Buffer with data to be written to flash.
	 * @param bytes Number of bytes to write to flash.
	 * @return true if write was successful, false otherwise.
	 * @warning The sum of address and bytes cannot exceed flash total size, this will cause write to fail.
	 */
	virtual bool write(uint32_t address, const uint8_t *buffer, size_t bytes) = 0;

	//! Returns the flash size in bytes or zero if flash wasn't been initialized or is incompatible.
	inline size_t getTotalSize() {
		if (this->parameters.memoryDensityExp) return (1 << (this->parameters.memoryDensity >> 3));
		else return ((this->parameters.memoryDensity+1) >> 3);
	}

	//! Returns the flash sector size in bytes or zero if flash wasn't been initialized yet.
	inline size_t getSectorSize(uint8_t sector = 0) {
		if ((sector > 4) || (this->parameters.sectors[sector].size == 0)) return 0;

		return (1 << (this->parameters.sectors[sector].size));
	}

	/**
	 * Generates an VFS file linked to the Flash that can be added to the
	 * virtual file system, allowing flash programming via ethernet or console.
	 *
	 * The address is determined by a logical boot target specifier, mapped to
	 * a physical partition using the specified partition size and the BootConfig
	 * mapping functions.
	 *
	 * @param bootconf The BootConfig instance handling mapping.
	 * @param lbt The logical boot target specifier.
	 * @param bytes Number of bytes per partition.
	 * @return VFS::File that can be used with VFS::addFile.
	 * @throw std::runtime_error if address and size checks fail.
	 */
	VFS::File createBootImageFile(BootConfig &bootconf, enum BootConfig::LogicalBootTarget lbt, size_t bytes, std::function<void(Flash*)> finish_cb = nullptr) {
		if (bytes == 0) throw std::runtime_error("Flash size cannot be zero");
		if (!this->initialized) throw std::runtime_error("Flash device is not initialized");
		if ((bytes % 256) != 0) throw std::runtime_error("Start address is not page aligned");
		if ((lbt+1) * bytes > this->getTotalSize()) throw std::runtime_error("File size exceeds flash total size");

		return VFS::File(
			[this, &bootconf, lbt, bytes, finish_cb](uint8_t *buffer, size_t size) -> size_t {
				// Read
				if (size > bytes) return 0; // Read request too large
				if (!this->read(bootconf.mapLogicalToPhysicalBootTarget(lbt) * size, buffer, size)) return 0; // Failed to read
				if (finish_cb) finish_cb(this);
				return size;
			},
			[this, &bootconf, lbt, bytes, finish_cb](uint8_t *buffer, size_t size) -> size_t {
				// Write
				if (size > bytes) return 0; // Write request too large

				if (lbt == BootConfig::LBT_BACKUP)
					throw std::runtime_error("You cannot modify the backup image, use update.bin to upgrade.");

				enum BootConfig::PhysicalBootTarget pbt = bootconf.mapLogicalToPhysicalBootTarget(lbt);
				// If we're looking at an A/B partition, we're writing to the OTHER one.  We'll switch later.
				if (pbt == BootConfig::PBT_A)
					pbt = BootConfig::PBT_B;
				else if (pbt == BootConfig::PBT_B)
					pbt = BootConfig::PBT_A;

				std::string message;
				std::shared_ptr<const VersionInfo> bin_version = NULL;
				BootFileValidationReturn r = validateBootFile(buffer, size, message, bin_version, &bootconf);
				if (r != 0) {
					LOG["flash_upgrade"].log(stdsprintf("Uploaded QSPI image is INVALID: %s", message.c_str()), LogTree::LOG_CRITICAL);
					// The FTP server will convert this to a 450 error.
					throw std::runtime_error(stdsprintf("Uploaded QSPI image is INVALID: %s", message.c_str()));
				} else {
					LOG["flash_upgrade"].log(stdsprintf("Uploaded QSPI image is VALID: %s", message.c_str()), LogTree::LOG_NOTICE);
				}

#ifdef ENABLE_IPMI
				if (!mstatemachine->setUpdateLock()) {
					LOG["flash_upgrade"].log("It is only possible to perform updates or boot/image reconfiguration while in M1.", LogTree::LOG_CRITICAL);
					// The FTP server will convert this to a 450 error.
					throw std::runtime_error("It is only possible to perform updates or boot/image reconfiguration while in M1.");
					return 0;
				}
				LOG["flash_upgrade"].log("Update lock set. It is not possible to go to M4 without restarting.", LogTree::LOG_CRITICAL);
#endif

				if (!this->write(pbt * bytes, buffer, size)) return 0; // Failed to write
				LOG["flash_upgrade"].log("Flash upgrade complete.", LogTree::LOG_NOTICE);

				if (lbt == BootConfig::LBT_PRIMARY) {
					// We're doing A/B update.  Switch over.
					bootconf.switchPrimaryImage();
					bootconf.flushBootTarget();
					LOG["flash_upgrade"].log("Updated primary boot image.", LogTree::LOG_NOTICE);
					if (bootconf.getImageTagLock() == "?" && (bin_version->version.tag.substr(0,9) != "fallback-") && (bin_version->version.tag != "fallback")) {
						bootconf.setImageTagLock(bin_version->version.tag);
						LOG["flash_upgrade"].log(stdsprintf("Set uninitialized image tag lock to \"%s\". Change it with the image_tag_lock command.", bin_version->version.tag.c_str()), LogTree::LOG_NOTICE);
					}
				}
				if (finish_cb) finish_cb(this);
				return size;
			}, bytes);
	}

protected:
	/**
	 * Fill the JEDEC parameters structure from flash.
	 * @return false if flash is not JEDEC compliant of if there is a communication issue, true otherwise.
	 */
	virtual bool getJEDECInfo() = 0;

	/**
	 * Retrieve the manufacturer ID from flash.
	 * @return true if successful.
	 */
	virtual bool getManufacturerID() = 0;

	//! Disables write protections.
	virtual bool disableWriteProtections() = 0;

	//! Enables writing to the flash. Automatically de-asserted after a write operation.
	virtual bool enableWriting() = 0;

	//! Disable writing to the flash.
	virtual bool disableWriting() = 0;

	//! Wait for a write operation to complete.
	virtual bool waitForWriteComplete() = 0;

	//! Set the bank in flash (for flashes with > 16MiB).
	virtual bool selectBank(uint8_t bank) = 0;

	//! Get the currently selected bank.
	virtual bool getSelectedBank(uint8_t &bank) = 0;

	/**
	 * Write a single page to the flash.
	 * @param address Page address.
	 * @param buffer Buffer with data to write with exactly 256 bytes.
	 * @param bytes Number of bytes to write, must be 256.
	 * @return false if arguments are invalid or if write failed, true otherwise.
	 */
	virtual bool writePage(uint32_t address, const uint8_t *buffer, size_t bytes) = 0;

	/**
	 * Erase a group of sectors, from address to (address + bytes).
	 * @param address First sector address, must be aligned to the sector size.
	 * @param bytes Number of bytes to erase.
	 * @return true if success, false otherwise.
	 * @warning If address is not aligned nor the total number of bytes is a multiple
	 * of the sector size then the operation will fail.
	 */
	virtual bool eraseSectors(uint32_t address, size_t bytes) = 0;

	//! Discoverable parameter header definition
	struct SFDP_First_Header {
		uint32_t signature : 32;
		uint8_t minor_revision : 8;
		uint8_t major_revision : 8;
		uint8_t num_headers : 8;
		uint8_t : 8;
	};

	//! Discoverable parameter table entry definition
	struct SFDP_Table_Entry {
		uint8_t id_number : 8;
		uint8_t minor_revision : 8;
		uint8_t major_revision : 8;
		uint8_t length_words : 8;
		uint32_t pointer : 24;
		uint8_t : 8;
	};

	//! JEDEC parameters definition
	struct JEDECFlashParameters {
		// Word 1
		uint32_t blockSectorEraseSize : 2;
		uint32_t writeGranularity : 1;
		uint32_t writeEnableRequiredToWriteToStatusRegister : 1;
		uint32_t writeEnableOpcodeSelectToWriteToStatusRegister : 1;
		uint32_t : 3;
		uint32_t eraseOpcode : 8;
		uint32_t supports112FastRead : 1;
		uint32_t addressBytes : 2;
		uint32_t supportsDoubleTransferRate : 1;
		uint32_t supports122FastRead : 1;
		uint32_t supports144FastRead : 1;
		uint32_t supports114FastRead : 1;
		uint32_t : 9;
		// Word 2
		uint32_t memoryDensity : 31;
		uint32_t memoryDensityExp : 1;
		// Word 3
		uint32_t fastRead144NumberOfWaits : 5;
		uint32_t fastRead144NumberOfModeBits : 3;
		uint32_t fastRead144OpCode : 8;
		uint32_t fastRead114NumberOfWaits : 5;
		uint32_t fastRead114NumberOfModeBits : 3;
		uint32_t fastRead114OpCode : 8;
		// Word 4
		uint32_t fastRead112NumberOfWaits : 5;
		uint32_t fastRead112NumberOfModeBits : 3;
		uint32_t fastRead112OpCode : 8;
		uint32_t fastRead122NumberOfWaits : 5;
		uint32_t fastRead122NumberOfModeBits : 3;
		uint32_t fastRead122OpCode : 8;
		// Word 5
		uint32_t supports222FastRead : 1;
		uint32_t : 3;
		uint32_t supports444FastRead : 1;
		uint32_t : 27;
		// Word 6
		uint32_t : 16;
		uint32_t fastRead222NumberOfWaits : 5;
		uint32_t fastRead222NumberOfModeBits : 3;
		uint32_t fastRead222OpCode : 8;
		// Word 7
		uint32_t : 16;
		uint32_t fastRead444NumberOfWaits : 5;
		uint32_t fastRead444NumberOfModeBits : 3;
		uint32_t fastRead444OpCode : 8;
		// Word 8 & 9
		struct Sector {uint8_t size; uint8_t opcode;} sectors[4];
	};

	//! Flash internal status register definition
	struct StatusRegister {
		union {
			struct {
				uint8_t writeInProgress : 1;
				uint8_t writeEnableLatch : 1;
				uint8_t blockProtect0 : 1;
				uint8_t blockProtect1 : 1;
				uint8_t blockProtect2 : 1;
				uint8_t blockProtect3 : 1;
				uint8_t quadEnable : 1;				///< Flash dependent
				uint8_t statusRegWriteDisable : 1;	///< Flash dependent
			};
			uint8_t _raw;
		};
	};

	//! Get the status register from the flash.
	virtual bool getStatusRegister(StatusRegister &status) = 0;

	bool initialized;					///< Indicates if initialized and if parameters is valid.
	JEDECFlashParameters parameters;	///< JEDEC parameters of the flash.
	uint8_t manufacturer;				///< Manufacturer ID of the flash.

	//! List of flash manufacturers and their IDs.
	enum ManufacturerID : uint8_t {
		MICRON = 0x20,
		MACRONIX = 0xC2
	};

};

#endif /* SRC_COMMON_ZYNQIPMC_DRIVERS_GENERICS_FLASH_H_ */
