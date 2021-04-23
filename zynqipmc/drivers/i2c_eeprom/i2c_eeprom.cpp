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

#include "i2c_eeprom.h"
#include <libs/except.h>
#include <libs/printf.h>
#include <list>

I2CEEPROM::I2CEEPROM(I2C &i2cbus, uint8_t address, size_t size, size_t page_size)
    : EEPROM(size, page_size), i2cbus(i2cbus), address(address) {
	this->mutex = xSemaphoreCreateMutex();
	configASSERT(this->mutex);
}

I2CEEPROM::~I2CEEPROM() {
	xSemaphoreTake(this->mutex, portMAX_DELAY);
	vSemaphoreDelete(this->mutex);
}

size_t I2CEEPROM::read(size_t address, uint8_t *buf, size_t bytes) {
	if (address + bytes > this->kTotalSize) {
		throw std::out_of_range(stdsprintf("Tried to access addresses [%u, %u) of an EEPROM with size %u.", address, address + bytes, this->kTotalSize));
	}

	const uint8_t hdr_len = (this->kTotalSize > 256 ? 2 : 1);

	uint8_t addr_buf[hdr_len];
	if (hdr_len == 2) {
		// Two byte address.
		addr_buf[0] = address >> 8;
		addr_buf[1] = address & 0xff;
	}
	else {
		// One byte address.
		configASSERT(address <= 0xff);
		addr_buf[0] = address & 0xff;
	}

	MutexGuard<false> lock(this->mutex, true);
	return this->i2cbus.atomic<size_t>([&]() -> size_t {
		if (this->i2cbus.write(this->address, addr_buf, hdr_len) != hdr_len)
			return 0; // Couldn't set read address.
		return this->i2cbus.read(this->address, buf, bytes);
	});
}

size_t I2CEEPROM::write(size_t address, const uint8_t *buf, size_t bytes) {
	if (address + bytes > this->kTotalSize) {
		throw std::out_of_range(stdsprintf("Tried to access addresses [%u, %u) of an EEPROM with size %u.", address, address + bytes, this->kTotalSize));
	}

	/* Unlike reads, writes DEFINITELY care about page boundaries.  We'll need
	 * to split transactions correctly.
	 */

	const uint8_t hdr_len = (this->kTotalSize > 256 ? 2 : 1);
	struct transaction {
		std::shared_ptr<uint8_t> buf;
		uint8_t len;
	};
	std::list<struct transaction> transactions;

	size_t bytes_written = 0;
	while (bytes_written < bytes) {
		size_t to_write = this->kPageSize - (address % this->kPageSize);
		if (to_write > (bytes - bytes_written))
			to_write = bytes - bytes_written;

		// Create a transaction containing as much as it will be written (up to one page)
		struct transaction write_tx;
		write_tx.buf = std::shared_ptr<uint8_t>(new uint8_t[hdr_len + to_write], std::default_delete<uint8_t[]>());
		if (hdr_len == 2) {
			write_tx.buf.get()[0] = address >> 8;
			write_tx.buf.get()[1] = address & 0xff;
		}
		else {
			configASSERT(address <= 0xff);
			write_tx.buf.get()[0] = address;
		}
		memcpy(write_tx.buf.get() + hdr_len, buf + bytes_written, to_write);
		write_tx.len = hdr_len + to_write;
		transactions.push_back(write_tx);

		bytes_written += to_write;
	}

	// We now have a list of individual page-write transactions to execute.
	MutexGuard<false> lock(this->mutex, true);
	return this->i2cbus.atomic<size_t>([&]() -> size_t {
		size_t written = 0;
		for (struct transaction tx : transactions) {
			size_t written_now = this->i2cbus.write(this->address, tx.buf.get(), tx.len);
			if (written_now != tx.len)
				return written + written_now - hdr_len;
			written += written_now - hdr_len;
			/* Now we need to wait for the write to physically complete.
			 *
			 * The datasheet says writes will have their addresses NAK'd, and
			 * since a addr-only write only sets the internal address pointer
			 * for the next read...
			 */
			while (this->i2cbus.write(this->address, tx.buf.get(), hdr_len) != hdr_len)
				vTaskDelay(1);
		}
		return written;
	});
}
