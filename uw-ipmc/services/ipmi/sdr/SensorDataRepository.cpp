/*
 * SensorDataRepository.cpp
 *
 *  Created on: Sep 6, 2018
 *      Author: jtikalsky
 */

#include <services/ipmi/sdr/SensorDataRepository.h>

/**
 * Add a Sensor Data Record to this repository.
 *
 * \note This will invalidate all reservations on this repository.
 *
 * @param record The record to add
 */
void SensorDataRepository::add(const SensorDataRecord &record) {
	std::shared_ptr<SensorDataRecord> interpreted = record.interpret();
	if (interpreted)
		this->records.push_back(interpreted);
	else
		this->records.push_back(std::make_shared<SensorDataRecord>(record));
	this->renumber();
}

/**
 * Add all records in the specified Sensor Data Repository to this repository.
 *
 * \note This will invalidate all reservations on this repository.
 *
 * @param sdrepository The record to add
 */
void SensorDataRepository::add(const SensorDataRepository &sdrepository) {
	auto reservation = this->reservation;
	for (auto it = sdrepository.records.begin(), eit = sdrepository.records.end(); it != eit; ++it)
		this->add(**it);
	this->reservation = reservation;
	this->reserve(); // Increment reservation only once.
}

/**
 * Remove the specified Sensor Data Record from this repository.
 *
 * \note This will invalidate all reservations on this repository.
 *
 * @param id The record id of the record to remove
 */
void SensorDataRepository::remove(uint16_t id) {
	if (id <= this->records.size())
		this->records.erase(std::next(this->records.begin(), id));
	this->renumber();
}

/**
 * Erase the contents of this repository.
 */
void SensorDataRepository::clear() {
	this->records.clear();
	this->reserve();
}

/**
 * Retrieve the specified Sensor Data Record from this repository.
 *
 * \note This will invalidate all reservations on this repository.
 *
 * @param id The record id of the record to remove
 * @return A std::shared_ptr<SensorDataRecord> containing a specialized SDR subclass.
 */
std::shared_ptr<const SensorDataRecord> SensorDataRepository::get(uint16_t id) const {
	if (id >= this->records.size())
		return NULL;
	return this->records.at(id);
}

/**
 * Return the number of records in this container.
 *
 * @return The number of records in this container
 */
std::vector< std::shared_ptr<SensorDataRecord> >::size_type SensorDataRepository::size() const {
	return this->records.size();
}

/**
 * Export the data from this SDR repository as a u8 vector.
 *
 * \warning This will potentially discard invalid SDR records from the repository.
 *
 * @return The SDR repository contents in binary form
 */
std::vector<uint8_t> SensorDataRepository::u8export() const {
	std::vector<uint8_t> output;
	for (auto it = this->records.begin(), eit = this->records.end(); it != eit; ++it) {
		SensorDataRecord &record = **it;
		if (record.sdr_data.size() < 5U || record.sdr_data.size() != 5U + record.sdr_data[4])
			continue; // Skip invalid record.
		output.insert(output.end(), record.sdr_data.begin(), record.sdr_data.end());
	}
	return output;
}

/**
 * Import the data from the supplied u8 vector, overwriting this SDR repository.
 *
 * \warning This will potentially discard invalid SDR records, and any later record.
 *
 * @param data The SDR repository contents in binary form
 */
void SensorDataRepository::u8import(const std::vector<uint8_t> &data) {
	auto reservation = this->reservation;
	this->records.clear();
	std::vector<uint8_t>::size_type cur = 0;
	while (cur < data.size()) {
		if (cur+5 < data.size())
			break; // No remaining record headers.
		if (cur+5+data[cur+4] < data.size())
			break; // No remaining complete records.
		std::vector<uint8_t> sdr(std::next(data.begin(), cur), std::next(data.begin(), cur+5+data[cur+4]));
		std::shared_ptr<SensorDataRecord> record = SensorDataRecord(sdr).interpret();
		if (!record)
			continue; // Can't be parsed.  We'll at least try for later ones.
		this->records.push_back(record);
	}
	this->reservation = reservation;
	this->reserve(); // Increment reservation only once.
}

/**
 * Reserves the repository, returning the new reservation id.
 * @return the new reservation id
 */
uint8_t SensorDataRepository::reserve() {
	this->reservation = ((this->reservation-1) % 0xff) + 1;
	return this->reservation;
}

/**
 * Renumber the SDRs in the repository after changes
 */
void SensorDataRepository::renumber() {
	this->reserve(); // Cancel any existing reservation.
	for (std::vector< std::shared_ptr<SensorDataRecord> >::size_type i = 0; i < this->records.size(); ++i)
		this->records.at(i)->record_id(i);
}
