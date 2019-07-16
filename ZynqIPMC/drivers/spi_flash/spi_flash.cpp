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

#include "spi_flash.h"
#include <memory>
#include <libs/Utils.h>
#include <libs/printf.h>

//! Returns the flash bank number from an address.
inline static uint32_t bankFromAddress(uint32_t address) {
	return address >> 24;
}

SPIFlash::SPIFlash(SPIMaster& spi, size_t chip_select, LogTree &log)
: spi(spi), kChipSelect(chip_select), log(log), quadModeEnabled(false) {
}

bool SPIFlash::initialize() {
	if (!Flash::initialize()) return false;

	if (this->parameters.supports114FastRead) {
		// Micron flashes don't have Quad Bit
		if (this->manufacturer != ManufacturerID::MICRON) {
			if (!this->enableQuadBit()) {
				this->initialized = false;
				return false;
			}
		}

		this->quadModeEnabled = true;
	}

	return true;
}

bool SPIFlash::read(uint32_t address, uint8_t *buffer, size_t bytes) {
	if (!this->isInitialized()) return false;

	if ((address + bytes) > this->getTotalSize()) return false;
	if (bankFromAddress(address) != bankFromAddress(address + bytes - 1)) return false; // TODO: For now don't allow reads spanning multiple banks

	size_t dummyBytes = (this->quadModeEnabled)? ((this->parameters.fastRead114NumberOfWaits+1)/8) : 0;

	if (!this->selectBank(bankFromAddress(address))) return false;

	return this->spi.atomic<bool>(this->kChipSelect, [=]() {
		uint8_t command[4 + dummyBytes] = {0};

		// Form the command to execute a read
		command[0] = (this->quadModeEnabled)? this->parameters.fastRead114OpCode : 0x03;
		command[1] = address >> 16;
		command[2] = address >> 8;
		command[3] = address;

		if (!this->spi.transfer_unsafe(command, NULL, sizeof(command))) {
			return false; // Failed
		}

		if (!this->spi.transfer_unsafe(buffer, buffer, bytes)) {
			return false; // Failed
		}

		return true;
	});
}

bool SPIFlash::write(uint32_t address, const uint8_t *buffer, size_t bytes) {
	if (!this->isInitialized()) return false;

	// Magic about to happen
	if (bytes == 0) return true;
	if ((address % this->getSectorSize(0)) != 0) return false; // TODO: For now we only sector aligned start addresses

	std::unique_ptr<uint8_t> tmppage = std::unique_ptr<uint8_t>(new uint8_t[256]);
	if (!tmppage) return false; // Out of memory

	size_t sectorSize = this->getSectorSize(0);
	size_t sectorRem = bytes % sectorSize;
	size_t sectorCount = bytes / sectorSize + (sectorRem? 1 : 0);

	size_t pagesPerSector = sectorSize / 256;

	std::unique_ptr<uint8_t> tmpbuf = nullptr;

	if (sectorRem > 0) {
		// Before erasing we need to read the last sector the will get partially erased
		tmpbuf = std::unique_ptr<uint8_t>(new uint8_t[sectorSize]);
		if (!tmpbuf) return false; // Out of memory

		size_t bufferOffset = bytes - sectorRem;
		size_t lastSectorAddress = address + bufferOffset;
		if (!this->read(lastSectorAddress, tmpbuf.get(), sectorSize)) {
			return false; // Failed
		}

		// Prefill with buffer data
		memcpy(&*tmpbuf, buffer + bufferOffset, sectorRem);
	}

	// Disable protections, if any
	if (!this->disableWriteProtections()) {
		return false; // Unable to disable protections
	}

	for (size_t sector = 0; sector < sectorCount; sector++) {
		size_t sectorOffset = sector * sectorSize;
		size_t sectorAddress = address + sectorOffset;
		uint8_t *pdata = nullptr;
		float progress = ((sector+1) * 1.0f) / (sectorCount * 1.0f) * 100.0f;

		log.log(stdsprintf("(%.0f%%) Programming sector 0x%08x", progress, sectorAddress), LogTree::LOG_NOTICE);

		if (!this->selectBank(bankFromAddress(sectorAddress))) return false;

		if (!this->eraseSector(sectorAddress)) {
			log.log(stdsprintf("Failed to erase sector 0x%08x", sectorAddress), LogTree::LOG_ERROR);
			return false;
		}

		if (sectorRem && (sector == (sectorCount-1))) {
			pdata = &*tmpbuf; // Last sector
		} else {
			pdata = (uint8_t*)buffer + sectorOffset;
		}

		// Minimize verbosity
		//log.log(stdsprintf("(%.0f%%) Writing %d pages at address 0x%08x", progress, pagesPerSector, sectorAddress), LogTree::LOG_NOTICE);


		for (size_t page = 0; page < pagesPerSector; page++) {
			size_t pageOffset = page * 256;

			if (!this->writePage(sectorAddress + pageOffset, pdata + pageOffset, 256)) {
				log.log(stdsprintf("Failed to write page 0x%08x", sectorAddress + pageOffset), LogTree::LOG_ERROR);
				return false;
			}

			// Verify
			if (!this->read(sectorAddress + pageOffset, &*tmppage, 256)) {
				log.log(stdsprintf("Failed to read back page 0x%08x", sectorAddress + pageOffset), LogTree::LOG_ERROR);
				return false;
			}

			if (memcmp(pdata + pageOffset, &*tmppage, 256) != 0) {
				log.log(stdsprintf("Mismatch in written page 0x%08x", sectorAddress + pageOffset), LogTree::LOG_ERROR);
//				log.log(formatedHexString(pdata + pageOffset, 256), LogTree::LOG_ERROR);
//				log.log(formatedHexString(&*tmppage, 256), LogTree::LOG_ERROR);
				return false;
			}
		}
	}

	return true;
}

bool SPIFlash::getManufacturerID()
{
	uint8_t command[4];
	command[0] = 0x9F;

	if (!this->spi.transfer(this->kChipSelect, command, command, sizeof(command))) {
		return false; // Failed
	}

	this->manufacturer = command[1];

	return true;
}

bool SPIFlash::getJEDECInfo() {
	// Based on JESD216
	return this->spi.atomic<bool>(this->kChipSelect, [=]() {
		uint8_t command[5] = {0};

		// Form the command to execute a read
		command[0] = 0x5A;

		if (!this->spi.transfer_unsafe(command, NULL, sizeof(command))) {
			return false; // Failed
		}

		// Start by getting the JEDEC main header
		struct SFDP_First_Header sfdpheader;

		if (!this->spi.transfer_unsafe((uint8_t*)&sfdpheader, (uint8_t*)&sfdpheader, sizeof(SFDP_First_Header))) {
			return false; // Failed
		}

		if (sfdpheader.signature != 0x50444653 || sfdpheader.major_revision != 0x01 || sfdpheader.num_headers > 3) {
			return false; // Something odd with first header
		}

		struct SFDP_Table_Entry tableentry[sfdpheader.num_headers+1];
		if (!this->spi.transfer_unsafe((uint8_t*)tableentry, (uint8_t*)tableentry, sizeof(SFDP_Table_Entry) * (sfdpheader.num_headers + 1))) {
			return false; // Failed
		}

		for (int i = 0; i <= sfdpheader.num_headers; i++) {
			if (tableentry[i].id_number == 0x00) {
				// Main JEDEC header - this is where the gold is!
				if (tableentry[i].major_revision != 0x01 /*|| tableentry[i].length_words != 9*/) {
					return false; // Only support rev 1
				}

				// Do the offset
				size_t offset = tableentry[i].pointer - (sizeof(SFDP_First_Header) + sizeof(SFDP_Table_Entry) * (sfdpheader.num_headers + 1));
				std::unique_ptr<uint8_t> dummy(new uint8_t[offset]);
				if (!dummy.get()) return false; // Not enough memory

				if (!this->spi.transfer_unsafe(dummy.get(), NULL, offset)) {
					return false; // Failed
				}

				if (!this->spi.transfer_unsafe((uint8_t*)&this->parameters, (uint8_t*)&this->parameters, sizeof(JEDECFlashParameters))) {
					return false; // Failed
				}

				return true;
			}
		}

		return false;
	});
}

bool SPIFlash::disableWriteProtections() {
	if (!this->isInitialized()) return false;

	if (this->parameters.writeEnableRequiredToWriteToStatusRegister) {
		uint8_t command[1];

		if (this->parameters.writeEnableOpcodeSelectToWriteToStatusRegister) {
			command[0] = 0x06;
		} else {
			command[0] = 0x50;
		}

		if (!this->spi.transfer(this->kChipSelect, command, NULL, sizeof(command))) {
			return false; // Failed
		}

		if (!this->waitForWriteComplete()) {
			return false;
		}
	}

	StatusRegister status;
	if (!this->getStatusRegister(status)) return false;

	if (status.blockProtect0 == 1 || status.blockProtect1 == 1 ||
		status.blockProtect2 == 1 || status.blockProtect3 == 1) {
		return false; // Failed
	}

	return true;
}

bool SPIFlash::enableQuadBit() {
	if (!this->isInitialized()) return false;

	StatusRegister status;
	if (!this->getStatusRegister(status)) return false;

	if (!status.quadEnable) {
		status.quadEnable = 1;

		if (!this->enableWriting()) {
			return false; // Failed
		}

		uint8_t command[2] = {0x01, status._raw};
		if (!this->spi.transfer(this->kChipSelect, command, NULL, sizeof(command))) {
			return false; // Failed
		}
	}

	return true;
}

bool SPIFlash::enableWriting() {
	if (!this->isInitialized()) return false;

	uint8_t command[1] = {0x06};
	if (!this->spi.transfer(this->kChipSelect, command, NULL, sizeof(command))) {
		return false; // Failed
	}

	return true;
}

bool SPIFlash::disableWriting() {
	if (!this->isInitialized()) return false;

	uint8_t command[1] = {0x04};
	if (!this->spi.transfer(this->kChipSelect, command, NULL, sizeof(command))) {
		return false; // Failed
	}

	return true;
}

bool SPIFlash::waitForWriteComplete() {
	if (!this->isInitialized()) return false;

	unsigned int attempts = 0;

	// Programming a page shouldn't take more than 1.5ms, so a big
	// delay from FreeRTOS will have a huge speed impact, so don't use it.
	while (attempts < 20000) {
		StatusRegister status;
		if (!this->getStatusRegister(status)) return false;

		if (status.writeInProgress) {
			//vTaskDelay(pdMS_TO_TICKS(50));
			attempts++;
		} else {
			return true;
		}
	}

	return false;
}

bool SPIFlash::selectBank(uint8_t bank) {
	// This is manufacturer specific and we only support Micron and Macronix

	uint8_t current;
	if (!this->getSelectedBank(current)) return false;

	if (current != bank) {
		// Bank change required
		uint8_t command[2] = {0xC5, bank};

		if (!this->enableWriting()) {
			return false;
		}

		if (!this->spi.transfer(this->kChipSelect, command, NULL, sizeof(command))) {
			return false; // Failed
		}

		if (!this->getSelectedBank(current)) return false;
		if (current != bank) {
			log.log(stdsprintf("Failed to change to bank %d", bank), LogTree::LOG_ERROR);
			return false;
		}
	}

	return true;
}

bool SPIFlash::getSelectedBank(uint8_t &bank) {
	// This is manufacturer specific and we only support Micron and Macronix
	uint8_t command[2];
	command[0] = 0xC8;

	if (!this->spi.transfer(this->kChipSelect, command, command, sizeof(command))) {
		return false; // Failed
	}

	bank = command[1];

	return true;
}

bool SPIFlash::writePage(uint32_t address, const uint8_t *buffer, size_t bytes) {
	if (!this->isInitialized()) return false;

	if (bytes != 256) return false;
	if ((address % 256) != 0) return false;

	if (!this->enableWriting()) {
		return false;
	}

	if (!this->spi.atomic<bool>(this->kChipSelect, [=]() {
		uint8_t command[4];

		// Form the command to execute a read
		command[0] = 0x02;
		command[1] = address >> 16;
		command[2] = address >> 8;
		command[3] = address;

		if (!this->spi.transfer_unsafe(command, NULL, sizeof(command))) {
			return false; // Failed
		}

		if (!this->spi.transfer_unsafe(buffer, NULL, 256)) {
			return false; // Failed
		}

		return true;
	})) return false;

	// Write enable will be cleared automatically
	return this->waitForWriteComplete();
}

bool SPIFlash::eraseSector(uint32_t address) {
	if (!this->isInitialized()) return false;

	if (this->getSectorSize(0) == 0) return false; // Erase not supported??
	if ((address % this->getSectorSize(0)) != 0) return false;

	if (!this->enableWriting()) {
		return false;
	}

	uint8_t command[4];
	command[0] = this->parameters.sectors[0].opcode;
	command[1] = address >> 16;
	command[2] = address >> 8;
	command[3] = address;

	if (!this->spi.transfer(this->kChipSelect, command, NULL, sizeof(command))) {
		return false; // Failed
	}

	return this->waitForWriteComplete();
}

bool SPIFlash::eraseSectors(uint32_t address, size_t bytes) {
	if (!this->isInitialized()) return false;

	if (this->getSectorSize(0) == 0) return false; // Erase not supported??
	if (address + bytes > this->getTotalSize()) return false;
	if ((address % this->getSectorSize(0)) != 0) return false;

	if (address == 0 && bytes == this->getTotalSize()) {
		log.log("Erasing the whole chip", LogTree::LOG_NOTICE);

		if (!this->enableWriting()) {
			return false;
		}

		// Erase the whole chip
		uint8_t command[1] = {0x60};
		if (!this->spi.transfer(this->kChipSelect, command, NULL, sizeof(command))) {
			return false; // Failed
		}

		return this->waitForWriteComplete();
	} else {
		// Do partial erases
		// This section can be optimized by starting to erase large blocks and then smaller, but considering
		// how many times this code will run it won't be really a performance issue.
		/*size_t numOfSectorSizes = 0;
		for (int i = 0; i < 4; i++) {
			if (this->getSectorSize(0) != 0) numOfSectorSizes++;
			else break;
		}
		for (int i = numOfSectorSizes; i > 0; i--) {
			etc..
		}*/

		size_t sectorsToErase = bytes / this->getSectorSize(0);
		for (size_t i = 0; i < sectorsToErase; i++) {
			uint32_t sectorAddress = address + i * this->getSectorSize(0);

			log.log(stdsprintf("Erasing sector 0x%08lX", sectorAddress), LogTree::LOG_NOTICE);

			if (!this->enableWriting()) {
				return false;
			}

			uint8_t command[4];
			command[0] = this->parameters.sectors[0].opcode;
			command[1] = sectorAddress >> 16;
			command[2] = sectorAddress >> 8;
			command[3] = sectorAddress;

			if (!this->spi.transfer(this->kChipSelect, command, NULL, sizeof(command))) {
				return false; // Failed
			}

			if (!this->waitForWriteComplete()) {
				return false; // Write failed
			}

			// Write enable will be cleared automatically
		}
	}

	return true;
}

bool SPIFlash::getStatusRegister(Flash::StatusRegister &status) {
	uint8_t command[2] = {0x05, 0};

	if (!this->spi.transfer(this->kChipSelect, command, command, sizeof(command))) {
		return false; // Failed
	}

	status._raw = command[1];

	return true;
}
