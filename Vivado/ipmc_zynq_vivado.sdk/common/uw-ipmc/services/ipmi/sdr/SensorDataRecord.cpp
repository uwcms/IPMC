/*
 * SensorDataRecord.cpp
 *
 *  Created on: Aug 3, 2018
 *      Author: jtikalsky
 */

#include <services/ipmi/sdr/SensorDataRecord.h>
#include <IPMC.h>

/**
 * Validate the current Sensor Data Record.
 *
 * If this function returns true, it is safe to read all relevant fields from
 * the record.  It only tests if the data is well-formed for the
 * SensorDataRecord subclass it is being interpreted by.  It may not be valid
 * with any other parser.
 *
 * \warning Do not call any accessors on a record that is not valid.
 *
 * @return true if the record is well-formed, else false
 */
bool SensorDataRecord::validate() {
	if (this->sdr_data.size() < 4)
		return false;
	if (this->sdr_data.size() != (4U + this->sdr_data[3]))
		return false;
	return true;
}

/**
 * Return a shared_ptr to a the appropriate generic SensorDataRecord subclass
 * for parsing and managing this record.
 *
 * \note This will return a NULL shared_ptr if the record cannot be handled by
 *       an appropriate subclass.
 *
 * @return A shared_ptr to a subclass if possible, false otherwise.
 */
std::shared_ptr<SensorDataRecord> SensorDataRecord::interpret() {
	if (!this->validate())
		return NULL;
	switch (this->get_record_type()) {
	default:
		/* If interpretation fails, return NULL, rather than a copy of *this, so
		 * that the user knows not to call get_record_key().
		 */
		return NULL;
	}
}

uint16_t SensorDataRecord::get_record_id() {
	configASSERT(this->sdr_data.size() >= 2);
	return (this->sdr_data[0] << 8) | this->sdr_data[1];
}

void SensorDataRecord::set_record_id(uint16_t record_id) {
	configASSERT(this->sdr_data.size() >= 2);
	this->sdr_data[0] = record_id >> 8;
	this->sdr_data[1] = record_id & 0xff;
}

uint8_t SensorDataRecord::get_record_version() {
	configASSERT(this->sdr_data.size() >= 3);
	return this->sdr_data[2];
}

uint8_t SensorDataRecord::get_record_type() {
	configASSERT(this->sdr_data.size() >= 4);
	return this->sdr_data[3];
}

std::vector<uint8_t> SensorDataRecord::get_record_key() {
	configASSERT(0); // Supported only by derived classes.
	return std::vector<uint8_t>();
}
