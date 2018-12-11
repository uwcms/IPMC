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
 */
void SensorDataRecord::validate() const {
	if (this->sdr_data.size() < 5)
		throw invalid_sdr_error("This SDR is shorter than the required headers.");
	if ((255 - this->sdr_data[4]) < 5)
		throw invalid_sdr_error("This SDR claims to be too long.");
	if (this->sdr_data.size() < (5U + this->sdr_data[4]))
		throw invalid_sdr_error("This SDR is shorter than specified in the header");
}

/**
 * Return a shared_ptr to a the appropriate generic SensorDataRecord subclass
 * for parsing and managing this record.
 *
 * \note This will return a NULL shared_ptr if the record cannot be handled by
 *       an appropriate subclass.
 *
 * @param The SDR data to interpret
 * @return A shared_ptr to a subclass if possible, false otherwise
 */
std::shared_ptr<SensorDataRecord> SensorDataRecord::interpret(const std::vector<uint8_t> &data) {
	if (data.size() < 5 || data.size() < (5U + data[4]))
		return NULL; // Invalid
	SensorDataRecord *rec = NULL;
	switch (data[3] /* Record Type */) {
	case 0x01: rec = new SensorDataRecord01(data); break;
	case 0x02: rec = new SensorDataRecord02(data); break;
	case 0x03: rec = new SensorDataRecord03(data); break;
	case 0x12: rec = new SensorDataRecord12(data); break;
	default:
		/* If interpretation fails, return NULL, rather than a copy of *this, so
		 * that the user knows not to call get_record_key().
		 */
		rec = NULL;
	}
	if (rec) {
		try {
			rec->validate();
		}
		catch (invalid_sdr_error &e) {
			return NULL;
		}
	}
	return std::shared_ptr<SensorDataRecord>(rec);
}

uint16_t SensorDataRecord::record_id() const {
	if (this->sdr_data.size() < 2)
		throw invalid_sdr_error("Truncated SDR");
	return (this->sdr_data[1] << 8) | this->sdr_data[0];
}

void SensorDataRecord::record_id(uint16_t record_id) {
	if (this->sdr_data.size() < 2)
		throw invalid_sdr_error("Truncated SDR");
	this->sdr_data[1] = record_id >> 8;
	this->sdr_data[0] = record_id & 0xff;
}

uint8_t SensorDataRecord::record_version() const {
	if (this->sdr_data.size() < 3)
		throw invalid_sdr_error("Truncated SDR");
	return this->sdr_data[2];
}

uint8_t SensorDataRecord::record_type() const {
	if (this->sdr_data.size() < 4)
		throw invalid_sdr_error("Truncated SDR");
	return this->sdr_data[3];
}

uint8_t SensorDataRecord::record_length() const {
	this->validate();
	return 5U + this->sdr_data[4];
}

/**
 * Check whether two SensorDataRecords have identical content (but not
 * necessarily identical internal extra fields (use a == on .sdr_data for that).
 *
 * @param b the record to compare to
 * @param compare_record_id true if record_ids must also match
 * @return true if the records are identical, else false
 */
bool SensorDataRecord::identical_content(const SensorDataRecord &b, bool compare_record_id) const {
	this->validate();
	b.validate();
	uint8_t rl = this->record_length();
	if (rl != b.record_length())
		return false;

	// Copy all record body bytes
	std::vector<uint8_t> adata = std::vector<uint8_t>(this->sdr_data.begin(), std::next(this->sdr_data.begin(), rl));
	std::vector<uint8_t> bdata = std::vector<uint8_t>(b.sdr_data.begin(), std::next(b.sdr_data.begin(), rl));

	if (!compare_record_id) {
		// Nullify both record IDs.
		adata[0] = 0;
		adata[1] = 0;
		bdata[0] = 0;
		bdata[1] = 0;
	}
	return adata == bdata;
}
