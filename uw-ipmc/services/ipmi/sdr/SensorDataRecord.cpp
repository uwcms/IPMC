/*
 * SensorDataRecord.cpp
 *
 *  Created on: Aug 3, 2018
 *      Author: jtikalsky
 */

#include <services/ipmi/sdr/SensorDataRecord.h>
#include <IPMC.h>

#include <services/ipmi/sdr/SensorDataRecord01.h>
#include <services/ipmi/sdr/SensorDataRecord02.h>
#include <services/ipmi/sdr/SensorDataRecord03.h>
#include <services/ipmi/sdr/SensorDataRecord12.h>

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
bool SensorDataRecord::validate() const {
	if (this->sdr_data.size() < 5)
		return false;
	if (this->sdr_data.size() < (5U + this->sdr_data[4]))
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
std::shared_ptr<SensorDataRecord> SensorDataRecord::interpret() const {
	if (!this->validate())
		return NULL;
	SensorDataRecord *rec = NULL;
	switch (this->record_type()) {
	case 0x01: rec = new SensorDataRecord01(this->sdr_data); break;
	case 0x02: rec = new SensorDataRecord02(this->sdr_data); break;
	case 0x03: rec = new SensorDataRecord03(this->sdr_data); break;
	case 0x12: rec = new SensorDataRecord12(this->sdr_data); break;
	default:
		/* If interpretation fails, return NULL, rather than a copy of *this, so
		 * that the user knows not to call get_record_key().
		 */
		rec = NULL;
	}
	return std::shared_ptr<SensorDataRecord>(rec);
}

uint16_t SensorDataRecord::record_id() const {
	configASSERT(this->sdr_data.size() >= 2);
	return (this->sdr_data[0] << 8) | this->sdr_data[1];
}

void SensorDataRecord::record_id(uint16_t record_id) {
	configASSERT(this->sdr_data.size() >= 2);
	this->sdr_data[0] = record_id >> 8;
	this->sdr_data[1] = record_id & 0xff;
}

uint8_t SensorDataRecord::record_version() const {
	configASSERT(this->sdr_data.size() >= 3);
	return this->sdr_data[2];
}

uint8_t SensorDataRecord::record_type() const {
	configASSERT(this->sdr_data.size() >= 4);
	return this->sdr_data[3];
}

std::vector<uint8_t> SensorDataRecord::record_key() const {
	configASSERT(0); // Supported only by derived classes.
	return std::vector<uint8_t>();
}
