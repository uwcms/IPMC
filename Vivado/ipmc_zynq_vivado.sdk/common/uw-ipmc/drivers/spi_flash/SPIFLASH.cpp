/*
 * SPIFLASH.cpp
 *
 *  Created on: Aug 20, 2018
 *      Author: mpv
 */

#include "SPIFLASH.h"
#include <memory>

SPIFlash::SPIFlash(SPIMaster& spi, uint8_t cs)
: spi(spi), cs(cs) {

}

bool SPIFlash::read(uint32_t address, uint8_t *buffer, size_t bytes) {
	if (!this->isInitialized()) return false;

	if ((address + bytes) > this->getTotalSize()) return false;

	return this->spi.atomic<bool>(this->cs, [=]() {
		uint8_t command[4];

		// Form the command to execute a read
		command[0] = 0x03;
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

	size_t sectorSize = this->getSectorSize(0);
	size_t rem = bytes % sectorSize;
	std::unique_ptr<uint8_t> tmpbuf = nullptr;

	if (rem > 0) {
		// Before erasing we need to read the last sector the will get partially erased
		tmpbuf = std::unique_ptr<uint8_t>(new uint8_t[sectorSize]);
		if (!tmpbuf) return false; // Out of memory

		size_t bufferOffset = bytes - rem;
		size_t lastSectorAddress = address + bufferOffset;
		if (!this->read(lastSectorAddress, tmpbuf.get(), sectorSize)) {
			return false; // Failed
		}

		// Prefill with buffer data
		memcpy(tmpbuf.get(), buffer + bufferOffset, rem);
	}

	// Disable protections, if any
	if (!this->disableWriteProtections()) {
		return false; // Unable to disable protections
	}

	// Erase the sectors
	size_t eraseRange = address + (bytes - rem) + (rem > 0?sectorSize:0);
	if (!this->eraseSectors(address, eraseRange)) {
		return false; // Failed
	}

	// Write pages
	size_t pageCount = (bytes - rem) / 256;
	for (size_t page = 0; page < pageCount; page++) {
		size_t bufferOffset = page * 256;
		if (!this->writePage(address + bufferOffset, buffer + bufferOffset, 256)) {
			return false;
		}
	}

	// Write the last page
	if (rem > 0) {
		size_t pageCount = sectorSize / 256;
		for (size_t page = 0; page < pageCount; page++) {
			size_t bufferOffset = page * 256;
			if (!this->writePage(address + (bytes - rem) + bufferOffset, tmpbuf.get() + bufferOffset, 256)) {
				return false;
			}
		}
	}

	return true;
}

bool SPIFlash::getJEDECInfo() {
	// Based on JESD216
	return this->spi.atomic<bool>(this->cs, [=]() {
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
				if (tableentry[i].major_revision != 0x01 || tableentry[i].length_words != 9) {
					return false; // Only support rev 1
				}

				// Do the offset
				size_t offset = tableentry[i].pointer - (sizeof(SFDP_First_Header) + sizeof(SFDP_Table_Entry) * (sfdpheader.num_headers + 1));
				std::unique_ptr<uint8_t> dummy(new uint8_t[offset]);
				if (!dummy.get()) return false; // Not enough memory

				if (!this->spi.transfer_unsafe(dummy.get(), dummy.get(), offset)) {
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

		if (!this->spi.transfer(this->cs, command, NULL, sizeof(command))) {
			return false; // Failed
		}

		if (!this->waitForWriteComplete()) {
			return false;
		}
	}

	if (!this->enableWriting()) {
		return false; // Failed
	}

	uint8_t command[2] = {0x01, 0};
	if (!this->spi.transfer(this->cs, command, NULL, sizeof(command))) {
		return false; // Failed
	}

	if (!this->waitForWriteComplete()) {
		return false;
	}

	StatusRegister status = this->getStatusRegister();

	if (status.blockProtect0 == 1 || status.blockProtect1 == 1) {
		return false; // Failed
	}

	return true;
}

bool SPIFlash::enableWriting() {
	if (!this->isInitialized()) return false;

	uint8_t command[1] = {0x06};
	if (!this->spi.transfer(this->cs, command, NULL, sizeof(command))) {
		return false; // Failed
	}

	return true;
}

bool SPIFlash::disableWriting() {
	if (!this->isInitialized()) return false;

	uint8_t command[1] = {0x04};
	if (!this->spi.transfer(this->cs, command, NULL, sizeof(command))) {
		return false; // Failed
	}

	return true;
}

bool SPIFlash::waitForWriteComplete() {
	if (!this->isInitialized()) return false;

	unsigned int attempts = 0;

	// If more than 100 attempts (100 * 50ms = 5sec) then abort
	while (attempts < 20) {
		StatusRegister status = this->getStatusRegister();

		if (status.writeInProgress) {
			vTaskDelay(pdMS_TO_TICKS(50));
			attempts++;
		} else {
			return true;
		}
	}

	return false;
}

bool SPIFlash::writePage(uint32_t address, const uint8_t *buffer, size_t bytes) {
	if (!this->isInitialized()) return false;

	if (bytes != 256) return false;
	if ((address % 256) != 0) return false;

	printf("Writing page 0x%08X", address);

	if (!this->enableWriting()) {
		return false;
	}

	if (!this->spi.atomic<bool>(this->cs, [=]() {
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

bool SPIFlash::eraseSectors(uint32_t address, size_t bytes) {
	if (!this->isInitialized()) return false;

	if (this->getSectorSize(0) == 0) return false; // Erase not supported??
	if (address + bytes > this->getTotalSize()) return false;
	if ((address % this->getSectorSize(0)) != 0) return false;

	if (address == 0 && bytes == this->getTotalSize()) {
		printf("Erasing chip");

		if (!this->enableWriting()) {
			return false;
		}

		// Erase the whole chip
		uint8_t command[1] = {0x60};
		if (!this->spi.transfer(this->cs, command, NULL, sizeof(command))) {
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

			printf("Erasing sector 0x%08X", sectorAddress);

			if (!this->enableWriting()) {
				return false;
			}

			uint8_t command[4];
			command[0] = this->parameters.sectors[0].opcode;
			command[1] = sectorAddress >> 16;
			command[2] = sectorAddress >> 8;
			command[3] = sectorAddress;

			if (!this->spi.transfer(this->cs, command, NULL, sizeof(command))) {
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

SPIFlash::StatusRegister SPIFlash::getStatusRegister() {
	StatusRegister status;
	status._raw = 0;
	uint8_t command[2] = {0x05, 0};

	if (!this->spi.transfer(this->cs, command, command, sizeof(command))) {
		return status; // Failed
	}

	status._raw = command[1];

	return status;
}
