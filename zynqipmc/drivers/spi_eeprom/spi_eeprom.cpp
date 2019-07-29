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

#include "spi_eeprom.h"
#include <libs/printf.h>
#include <libs/except.h>

SPIEEPROM::SPIEEPROM(SPIMaster &spibus, size_t chip_select, size_t size, size_t page_size) :
EEPROM(size, page_size), spibus(spibus), chip_select(chip_select) {
	this->mutex = xSemaphoreCreateMutex();
	configASSERT(this->mutex);
}

SPIEEPROM::~SPIEEPROM() {
	xSemaphoreTake(this->mutex, portMAX_DELAY);
	vSemaphoreDelete(this->mutex);
}

size_t SPIEEPROM::read(size_t address, uint8_t *buf, size_t bytes) {
	if (address + bytes > this->kTotalSize) {
		throw std::out_of_range(stdsprintf("Tried to access addresses [%u, %u) of an EEPROM with size %u.", address, address+bytes, this->kTotalSize));
	}

	const uint8_t hdr_len = (this->kTotalSize > 256 ? 3 : 2);

	// Heap allocation to avoid stack overflow on large reads (like PersistentStorage startup)
	uint8_t *txbuf = (uint8_t*)malloc(hdr_len + bytes);
	txbuf[0] = 3; // CMD_READ

	if (hdr_len == 3) {
		// Two byte address.
		txbuf[1] = address >> 8;
		txbuf[2] = address & 0xff;
	} else {
		// One byte address.
		configASSERT(address <= 0xff);
		txbuf[1] = address & 0xff;
	}

	MutexGuard<false> lock(this->mutex, true);
	// TODO: Atomic instead of this, this code could take some improvements
	if (!this->spibus.transfer(this->chip_select, txbuf, txbuf, hdr_len+bytes)) {
		lock.release();
		vPortFree(txbuf);
		return 0;
	}
	lock.release();

	memcpy(buf, txbuf+hdr_len, bytes);
	free(txbuf);

	return bytes;
}

size_t SPIEEPROM::write(size_t address, const uint8_t *buf, size_t bytes) {
	if (address + bytes > this->kTotalSize) {
		throw std::out_of_range(stdsprintf("Tried to access addresses [%u, %u) of an EEPROM with size %u.", address, address+bytes, this->kTotalSize));
	}

	MutexGuard<false> lock(this->mutex, true);

	/* Unlike reads, writes DEFINITELY care about page boundaries.  We'll need
	 * to split transactions correctly.
	 */
	const uint8_t hdr_len = (this->kTotalSize > 256 ? 3 : 2);
	uint8_t txbuf[hdr_len+this->kPageSize];
	size_t bytes_written = 0;
	while (bytes_written < bytes) {
		txbuf[0] = 6; // CMD_WRITE_ENABLE
		if (!this->spibus.transfer(this->chip_select, txbuf, nullptr, 1))
				break;

		txbuf[0] = 2; // CMD_WRITE
		if (hdr_len == 3) {
			// Two byte address.
			txbuf[1] = address >> 8;
			txbuf[2] = address & 0xff;
		}
		else {
			// One byte address.
			configASSERT(address <= 0xff);
			txbuf[1] = address & 0xff;
		}

		size_t to_write = this->kPageSize - (address % this->kPageSize);
		if (to_write > (bytes - bytes_written))
			to_write = bytes - bytes_written;

		// Fill as much as it will be written (up to one page)
		memcpy(txbuf+hdr_len, buf+bytes_written, to_write);

		if (!this->spibus.transfer(this->chip_select, txbuf, nullptr, hdr_len+to_write))
			break;

		do {
			if (txbuf[0] == 5) // Not our first RDSR
				vTaskDelay(1); // Sleep the shortest possible duration before rechecking.
			// Now to wait until the write operation is actually complete.
			txbuf[0] = 5; // CMD_RDSR
			txbuf[1] = 1; // So the `continue;` below will in fact always loop.
			if (!this->spibus.transfer(this->chip_select, txbuf, txbuf, 2))
				continue;
		} while (txbuf[1] & 1); // Write In Progress.

		bytes_written += to_write;
		address += to_write;
	}
	return bytes_written;
}
