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

#include <libs/threading.h>
#include <services/ipmi/IPMI_MSG.h> // for ipmi_checksum()
#include <services/ipmi/sdr/sensor_data_repository.h>

SensorDataRepository::SensorDataRepository()
	: reservation(0), last_update_ts(0) {
	this->mutex = xSemaphoreCreateRecursiveMutex();
	configASSERT(this->mutex);
}

SensorDataRepository::~SensorDataRepository() {
	vSemaphoreDelete(this->mutex);
}

uint16_t SensorDataRepository::add(const SensorDataRecord &record, reservation_t reservation) {
	MutexGuard<true> lock(this->mutex, true);
	this->assert_reservation(reservation);

	/* Ensure we have our own copy, and that it is valid.
	 * Retrievals are const, and we want modifications to be done by replacement.
	 */
	std::shared_ptr<SensorDataRecord> interpreted = record.interpret();
	if (interpreted) {
		bool replaced = false;
		for (size_type i = 0; i < this->records.size(); ++i) {
			if (*this->records[i] == *interpreted) {
				interpreted->record_id(i);
				if (!this->records[i]->identical_content(*interpreted, true))
					this->last_update_ts = time(nullptr);
				this->records[i] = interpreted;
				replaced = true;
				break;
			}
		}

		if (!replaced) {
			interpreted->record_id(this->records.size());
			this->records.push_back(interpreted);
			this->last_update_ts = time(nullptr);
		}

		return interpreted->record_id();
	} else {
		throw sdr_invalid_error("The provided SDR could not be interpreted.");
	}
}

void SensorDataRepository::add(const SensorDataRepository &sdrepository, reservation_t reservation) {
	MutexGuard<true> lock(this->mutex, true);
	this->assert_reservation(reservation);
	for (auto it = sdrepository.records.begin(), eit = sdrepository.records.end(); it != eit; ++it) {
		try {
			this->add(**it, reservation);
		} catch (sdr_invalid_error) {
			// Skip record.
		}
	}
}

bool SensorDataRepository::remove(uint16_t id, reservation_t reservation) {
	MutexGuard<true> lock(this->mutex, true);
	this->assert_reservation(reservation);
	bool removed = false;

	if (id <= this->records.size()) {
		this->records.erase(std::next(this->records.begin(), id));
		removed = true;
	}

	this->renumber();
	this->last_update_ts = time(nullptr);
	return removed;
}

bool SensorDataRepository::remove(const SensorDataRecord &record, reservation_t reservation) {
	MutexGuard<true> lock(this->mutex, true);
	this->assert_reservation(reservation);
	bool removed = false;

	for (auto it = this->records.begin(); it != this->records.end(); /* below */) {
		if (**it == record) {
			it = this->records.erase(it);
			removed = true;
		} else {
			++it;
		}
	}

	this->renumber();
	this->last_update_ts = time(nullptr);
	return removed;
}

void SensorDataRepository::clear(reservation_t reservation) {
	MutexGuard<true> lock(this->mutex, true);
	this->assert_reservation(reservation);
	this->records.clear();
	this->last_update_ts = time(nullptr);
}

time_t SensorDataRepository::lastUpdateTimestamp() {
	MutexGuard<true> lock(this->mutex, true);

	// If possible & necessary, update a relative-to-boot timestamp to "now".
	if (this->last_update_ts <= 0x20000000) {
		time_t now = time(nullptr);
		if (now > 0x20000000) {
			this->last_update_ts = now;
		}
	}
	return this->last_update_ts;
}

std::shared_ptr<const SensorDataRecord> SensorDataRepository::get(uint16_t id, reservation_t reservation) const {
	MutexGuard<true> lock(this->mutex, true);
	if (reservation && reservation != this->reservation) {
		throw reservation_cancelled_error("The provided reservation is not valid");
	}

	if (id >= this->records.size()) {
		return nullptr;
	}

	return this->records.at(id);
}

std::shared_ptr<const SensorDataRecord> SensorDataRepository::find(const std::vector<uint8_t> &key, reservation_t reservation) const {
	MutexGuard<true> lock(this->mutex, true);
	if (reservation && reservation != this->reservation) {
		throw reservation_cancelled_error("The provided reservation is not valid");
	}

	for (auto it = this->records.begin(), eit = this->records.end(); it != eit; ++it) {
		if ((*it)->record_key() == key) {
			return *it;
		}
	}

	return nullptr;
}

SensorDataRepository::size_type SensorDataRepository::size() const {
	MutexGuard<true> lock(this->mutex, true);
	return this->records.size();
}

SensorDataRepository::operator std::vector< std::shared_ptr<const SensorDataRecord> >() const {
	MutexGuard<true> lock(this->mutex, true);
	std::vector< std::shared_ptr<const SensorDataRecord> > ret;

	// We're doing a shallow copy so we need to const-ify our output records.
	for (auto it = this->records.begin(), eit = this->records.end(); it != eit; ++it) {
		ret.push_back(*it);
	}

	return ret;
}

std::vector<uint8_t> SensorDataRepository::u8export() const {
	MutexGuard<true> lock(this->mutex, true);
	std::vector<uint8_t> output;

	// Export Update Timestamp
	time_t last_update_ts = this->last_update_ts;
	if (last_update_ts <= 0x20000000) {
		last_update_ts = 0; // We don't want to persist a boot-relative TS.  It'd be a lie soon after next boot.
	}

	for (size_t i = 0; i < sizeof(time_t); ++i) {
		output.push_back((this->last_update_ts >> (8*i)) & 0xFF);
	}

	// Export Records
	for (auto it = this->records.begin(), eit = this->records.end(); it != eit; ++it) {
		SensorDataRecord &record = **it;
		if (record.sdr_data.size() == 0 || record.sdr_data.size() > 255) {
			continue; // We can't store this record.
		}

		output.push_back(record.sdr_data.size());
		output.insert(output.end(), record.sdr_data.begin(), record.sdr_data.end());
	}

	output.insert(output.begin(), ipmi_checksum(output));
	return output;
}

bool SensorDataRepository::u8import(const std::vector<uint8_t> &data, reservation_t reservation) {
	MutexGuard<true> lock(this->mutex, true);
	this->assert_reservation(reservation);

	if (data.size() < 2) return false; // No data here.
	if (ipmi_checksum(data)) return false; // Invalid checksum.

	// Retrieve Update Timestamp
	if (data.size() < 1+sizeof(time_t)) return false;
	std::vector<uint8_t>::size_type cur = 1; // Checksum is byte 0.
	time_t last_update_ts = 0;
	for (; cur-1 < sizeof(time_t); ++cur) {
		last_update_ts |= data[cur] << ((cur-1)*8);
	}

	if (last_update_ts > this->last_update_ts) {
		this->last_update_ts = last_update_ts;
	}

	SensorDataRepository tmprepo;

	// Import Records
	while (cur < data.size()) {
		uint8_t record_length = data[cur];
		if (record_length == 0) return false; // Synchronization invalid
		if (cur+1+record_length > data.size()) return false; // Synchronization invalid

		std::vector<uint8_t> sdrdata(std::next(data.begin(), cur+1), std::next(data.begin(), cur+1+record_length));
		cur += 1+record_length;
		std::shared_ptr<SensorDataRecord> sdr = SensorDataRecord::interpret(sdrdata);

		if (sdr) {
			tmprepo.add(*sdr, 0);
		}
	}

	this->add(tmprepo, reservation);
	return true;
}

SensorDataRepository::reservation_t SensorDataRepository::getCurrentReservation() const {
	MutexGuard<true> lock(this->mutex, true);
	return this->reservation;
}

SensorDataRepository::reservation_t SensorDataRepository::reserve() {
	MutexGuard<true> lock(this->mutex, true);
	this->reservation += 1; // Auto-wrap from uint16_t.
	if (this->reservation == 0) {
		this->reservation += 1; // We don't want to give out reservation 0.
	}
	return this->reservation;
}

void SensorDataRepository::renumber() {
	MutexGuard<true> lock(this->mutex, true);
	for (std::vector< std::shared_ptr<SensorDataRecord> >::size_type i = 0; i < this->records.size(); ++i) {
		this->records.at(i)->record_id(i);
	}
}

void SensorDataRepository::assert_reservation(reservation_t &reservation) {
	MutexGuard<true> lock(this->mutex, true);
	if (!reservation) {
		reservation = this->reserve();
	}

	if (reservation != this->reservation) {
		throw reservation_cancelled_error("The provided reservation is not valid");
	}
}
