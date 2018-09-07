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
	for (auto it = sdrepository.records.begin(), eit = sdrepository.records.end(); it != eit; ++it)
		this->add(**it);
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
