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

#include <services/ipmi/sdr/sensor_data_record.h>
#include <services/ipmi/sdr/sensor_data_record_01.h>
#include <services/ipmi/sdr/sensor_data_record_02.h>
#include <services/ipmi/sdr/sensor_data_record_03.h>
#include <services/ipmi/sdr/sensor_data_record_12.h>

void SensorDataRecord::validate() const {
	if (this->sdr_data.size() < 5) {
		throw invalid_sdr_error("This SDR is shorter than the required headers.");
	}

	if ((255 - this->sdr_data[4]) < 5) {
		throw invalid_sdr_error("This SDR claims to be too long.");
	}

	if (this->sdr_data.size() < (5U + this->sdr_data[4])) {
		throw invalid_sdr_error("This SDR is shorter than specified in the header");
	}
}

std::shared_ptr<SensorDataRecord> SensorDataRecord::interpret(const std::vector<uint8_t> &data) {
	if (data.size() < 5 || data.size() < (5U + data[4])) {
		return nullptr; // Invalid
	}

	SensorDataRecord *rec = nullptr;
	switch (data[3] /* Record Type */) {
		case 0x01: rec = new SensorDataRecord01(data); break;
		case 0x02: rec = new SensorDataRecord02(data); break;
		case 0x03: rec = new SensorDataRecord03(data); break;
		case 0x12: rec = new SensorDataRecord12(data); break;
		default: {
			/* If interpretation fails, return nullptr, rather than a copy of *this, so
			 * that the user knows not to call get_record_key().
			 */
			rec = nullptr;
		} break;
	}

	if (rec) {
		try {
			rec->validate();
		} catch (invalid_sdr_error &e) {
			return nullptr;
		}
	}

	return std::shared_ptr<SensorDataRecord>(rec);
}

uint16_t SensorDataRecord::recordId() const {
	if (this->sdr_data.size() < 2) {
		throw invalid_sdr_error("Truncated SDR");
	}

	return ((uint16_t)this->sdr_data[1] << 8) | this->sdr_data[0];
}

void SensorDataRecord::recordId(uint16_t record_id) {
	if (this->sdr_data.size() < 2) {
		throw invalid_sdr_error("Truncated SDR");
	}

	this->sdr_data[1] = record_id >> 8;
	this->sdr_data[0] = record_id & 0xff;
}

uint8_t SensorDataRecord::recordVersion() const {
	if (this->sdr_data.size() < 3) {
		throw invalid_sdr_error("Truncated SDR");
	}

	return this->sdr_data[2];
}

uint8_t SensorDataRecord::recordType() const {
	if (this->sdr_data.size() < 4) {
		throw invalid_sdr_error("Truncated SDR");
	}

	return this->sdr_data[3];
}

uint8_t SensorDataRecord::recordLength() const {
	this->validate();

	return 5U + this->sdr_data[4];
}

std::vector<uint8_t> SensorDataRecord::u8export(uint8_t self_ipmb_addr, uint8_t self_ipmb_channel) const {
	this->validate();

	std::vector<uint8_t> out = std::vector<uint8_t>(this->sdr_data.begin(), std::next(this->sdr_data.begin(), this->recordLength()));

	return out;
}

bool SensorDataRecord::identicalContent(const SensorDataRecord &b, bool compare_record_id) const {
	this->validate();
	b.validate();

	uint8_t rl = this->recordLength();
	if (rl != b.recordLength()) return false;

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
