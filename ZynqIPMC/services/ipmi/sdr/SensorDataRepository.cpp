/*
 * SensorDataRepository.cpp
 *
 *  Created on: Sep 6, 2018
 *      Author: jtikalsky
 */

#include <libs/threading.h>
#include <services/ipmi/sdr/SensorDataRepository.h>
#include <services/ipmi/IPMI_MSG.h> // for ipmi_checksum()

SensorDataRepository::SensorDataRepository()
	: reservation(0), last_update_ts(0) {
	this->mutex = xSemaphoreCreateRecursiveMutex();
	configASSERT(this->mutex);
}

SensorDataRepository::~SensorDataRepository() {
	vSemaphoreDelete(this->mutex);
}

/**
 * Add a Sensor Data Record to this repository.  If another record with the same
 * type and key bytes already exists, it will be replaced.
 *
 * @param record The record to add
 * @param reservation The current reservation (or 0 to oneshot reserve for this
 *                    operation).  If this does not match the current
 *                    reservation, the operation will not complete.
 * @return The record ID of the inserted record.
 * @throw reservation_cancelled_error
 * @throw sdr_invalid_error
 */
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
					this->last_update_ts = time(NULL);
				this->records[i] = interpreted;
				replaced = true;
				break;
			}
		}
		if (!replaced) {
			interpreted->record_id(this->records.size());
			this->records.push_back(interpreted);
			this->last_update_ts = time(NULL);
		}
		return interpreted->record_id();
	}
	else {
		throw sdr_invalid_error("The provided SDR could not be interpreted.");
	}
}

/**
 * Add all records in the specified Sensor Data Repository to this repository.
 *
 * \note SDRs which could not be interpreted will not be added.
 *
 * @param sdrepository The record to add
 * @param reservation The current reservation (or 0 to oneshot reserve for this
 *                    operation).  If this does not match the current
 *                    reservation, the operation will not complete.
 * @throw reservation_cancelled_error
 */
void SensorDataRepository::add(const SensorDataRepository &sdrepository, reservation_t reservation) {
	MutexGuard<true> lock(this->mutex, true);
	this->assert_reservation(reservation);
	for (auto it = sdrepository.records.begin(), eit = sdrepository.records.end(); it != eit; ++it) {
		try {
			this->add(**it, reservation);
		}
		catch (sdr_invalid_error) {
			// Skip record.
		}
	}
}

/**
 * Remove the specified Sensor Data Record from this repository.
 *
 * @param id The record id of the record to remove
 * @param reservation The current reservation (or 0 to oneshot reserve for this
 *                    operation).  If this does not match the current
 *                    reservation, the operation will not complete.
 * @return true if a record was removed else false
 * @throw reservation_cancelled_error
 */
bool SensorDataRepository::remove(uint16_t id, reservation_t reservation) {
	MutexGuard<true> lock(this->mutex, true);
	this->assert_reservation(reservation);
	bool removed = false;
	if (id <= this->records.size()) {
		this->records.erase(std::next(this->records.begin(), id));
		removed = true;
	}
	this->renumber();
	this->last_update_ts = time(NULL);
	return removed;
}

/**
 * Remove all records with the same type and key bytes as the provided SDR from
 * the repository.
 *
 * @param record The SDR to match for removal
 * @param reservation The current reservation (or 0 to oneshot reserve for this
 *                    operation).  If this does not match the current
 *                    reservation, the operation will not complete.
 * @return true if at least one record was removed else false
 * @throw reservation_cancelled_error
 */
bool SensorDataRepository::remove(const SensorDataRecord &record, reservation_t reservation) {
	MutexGuard<true> lock(this->mutex, true);
	this->assert_reservation(reservation);
	bool removed = false;
	for (auto it = this->records.begin(); it != this->records.end(); /* below */) {
		if (**it == record) {
			it = this->records.erase(it);
			removed = true;
		}
		else {
			++it;
		}
	}
	this->renumber();
	this->last_update_ts = time(NULL);
	return removed;
}

/**
 * Erase the contents of this repository.
 *
 * @param reservation The current reservation (or 0 to oneshot reserve for this
 *                    operation).  If this does not match the current
 *                    reservation, the operation will not complete.
 * @throw reservation_cancelled_error
 */
void SensorDataRepository::clear(reservation_t reservation) {
	MutexGuard<true> lock(this->mutex, true);
	this->assert_reservation(reservation);
	this->records.clear();
	this->last_update_ts = time(NULL);
}

/**
 * Retrieve the last update timestamp for the repository.
 *
 * @return the last update timestamp for the repository
 */
time_t SensorDataRepository::last_update_timestamp() {
	MutexGuard<true> lock(this->mutex, true);

	// If possible & necessary, update a relative-to-boot timestamp to "now".
	if (this->last_update_ts <= 0x20000000) {
		time_t now = time(NULL);
		if (now > 0x20000000)
			this->last_update_ts = now;
	}
	return this->last_update_ts;
}

/**
 * Retrieve the specified Sensor Data Record from this repository.
 *
 * @param id The record id of the record to retrieve
 * @return A std::shared_ptr<SensorDataRecord> containing a specialized SDR subclass.
 * @throw reservation_cancelled_error
 */
std::shared_ptr<const SensorDataRecord> SensorDataRepository::get(uint16_t id, reservation_t reservation) const {
	MutexGuard<true> lock(this->mutex, true);
	if (reservation && reservation != this->reservation)
		throw reservation_cancelled_error("The provided reservation is not valid");
	if (id >= this->records.size())
		return NULL;
	return this->records.at(id);
}

/**
 * Retrieve the specified Sensor Data Record from this repository.
 *
 * @param key The record key bytes of the record to retrieve.
 * @return A std::shared_ptr<SensorDataRecord> containing a specialized SDR subclass.
 * @throw reservation_cancelled_error
 */
std::shared_ptr<const SensorDataRecord> SensorDataRepository::find(const std::vector<uint8_t> &key, reservation_t reservation) const {
	MutexGuard<true> lock(this->mutex, true);
	if (reservation && reservation != this->reservation)
		throw reservation_cancelled_error("The provided reservation is not valid");
	for (auto it = this->records.begin(), eit = this->records.end(); it != eit; ++it)
		if ((*it)->record_key() == key)
			return *it;
	return NULL;
}

/**
 * Return the number of records in this container.
 *
 * @return The number of records in this container
 */
SensorDataRepository::size_type SensorDataRepository::size() const {
	MutexGuard<true> lock(this->mutex, true);
	return this->records.size();
}

/**
 * Export our database for external iteration or analysis.
 *
 * @return A copy of the internal database.
 */
SensorDataRepository::operator std::vector< std::shared_ptr<const SensorDataRecord> >() const {
	MutexGuard<true> lock(this->mutex, true);
	std::vector< std::shared_ptr<const SensorDataRecord> > ret;
	// We're doing a shallow copy so we need to const-ify our output records.
	for (auto it = this->records.begin(), eit = this->records.end(); it != eit; ++it)
		ret.push_back(*it);
	return ret;
}

/**
 * Export the data from this SDR repository as a u8 vector.
 *
 * \warning This will potentially discard invalid SDR records from the repository.
 *
 * @return The SDR repository contents in binary form
 */
std::vector<uint8_t> SensorDataRepository::u8export() const {
	MutexGuard<true> lock(this->mutex, true);
	std::vector<uint8_t> output;

	// Export Update Timestamp
	time_t last_update_ts = this->last_update_ts;
	if (last_update_ts <= 0x20000000)
		last_update_ts = 0; // We don't want to persist a boot-relative TS.  It'd be a lie soon after next boot.
	for (size_t i = 0; i < sizeof(time_t); ++i)
		output.push_back((this->last_update_ts >> (8*i)) & 0xFF);

	// Export Records
	for (auto it = this->records.begin(), eit = this->records.end(); it != eit; ++it) {
		SensorDataRecord &record = **it;
		if (record.sdr_data.size() == 0 || record.sdr_data.size() > 255)
			continue; // We can't store this record.
		output.push_back(record.sdr_data.size());
		output.insert(output.end(), record.sdr_data.begin(), record.sdr_data.end());
	}
	output.insert(output.begin(), ipmi_checksum(output));
	return output;
}

/**
 * Import the data from the supplied u8 vector, merging with this SDR repository.
 *
 * \note Invalid records are silently discarded, but synchronization, format
 *       or checksum errors are detectable.  If false is returned, no changes to
 *       the repository will have been made.
 *
 * @param data The SDR repository contents in binary form
 * @param reservation The current reservation (or 0 to oneshot reserve for this
 *                    operation).  If this does not match the current
 *                    reservation, the operation will not complete.
 * @return true if the operation was successful, false if data was invalid
 * @throw reservation_cancelled_error
 */
bool SensorDataRepository::u8import(const std::vector<uint8_t> &data, reservation_t reservation) {
	MutexGuard<true> lock(this->mutex, true);
	this->assert_reservation(reservation);

	if (data.size() < 2)
		return false; // No data here.
	if (ipmi_checksum(data))
		return false; // Invalid checksum.

	// Retrieve Update Timestamp
	if (data.size() < 1+sizeof(time_t))
		return false;
	std::vector<uint8_t>::size_type cur = 1; // Checksum is byte 0.
	time_t last_update_ts = 0;
	for (; cur-1 < sizeof(time_t); ++cur)
		last_update_ts |= data[cur] << ((cur-1)*8);
	if (last_update_ts > this->last_update_ts)
		this->last_update_ts = last_update_ts;

	SensorDataRepository tmprepo;

	// Import Records
	while (cur < data.size()) {
		uint8_t record_length = data[cur];
		if (record_length == 0)
			return false; // Synchronization invalid
		if (cur+1+record_length > data.size())
			return false; // Synchronization invalid
		std::vector<uint8_t> sdrdata(std::next(data.begin(), cur+1), std::next(data.begin(), cur+1+record_length));
		cur += 1+record_length;
		std::shared_ptr<SensorDataRecord> sdr = SensorDataRecord::interpret(sdrdata);
		if (sdr)
			tmprepo.add(*sdr, 0);
	}
	this->add(tmprepo, reservation);
	return true;
}

/**
 * Returns the ID of the current reservation.
 * @return the current reservation id
 */
SensorDataRepository::reservation_t SensorDataRepository::get_current_reservation() const {
	MutexGuard<true> lock(this->mutex, true);
	return this->reservation;
}

/**
 * Reserves the repository, returning the new reservation id.
 * @return the new reservation id
 */
SensorDataRepository::reservation_t SensorDataRepository::reserve() {
	MutexGuard<true> lock(this->mutex, true);
	this->reservation += 1; // Auto-wrap from uint16_t.
	if (this->reservation == 0)
		this->reservation += 1; // We don't want to give out reservation 0.
	return this->reservation;
}

/**
 * Renumber the SDRs in the repository after changes
 */
void SensorDataRepository::renumber() {
	MutexGuard<true> lock(this->mutex, true);
	for (std::vector< std::shared_ptr<SensorDataRecord> >::size_type i = 0; i < this->records.size(); ++i)
		this->records.at(i)->record_id(i);
}

/**
 * Acquire or verify a supplied reservation
 * @param reservation The reservation to acquire or verify
 * @throw reservation_cancelled_error
 */
void SensorDataRepository::assert_reservation(reservation_t &reservation) {
	MutexGuard<true> lock(this->mutex, true);
	if (!reservation)
		reservation = this->reserve();
	if (reservation != this->reservation)
		throw reservation_cancelled_error("The provided reservation is not valid");
}
