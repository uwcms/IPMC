#include <IPMC.h>
#include <services/ipmi/IPMIFormats.h>
#include <services/ipmi/sdr/SensorDataRecordSensor.h>
#include <iterator>

std::vector<uint8_t> SensorDataRecordSensor::record_key() const {
	this->validate();
	return std::vector<uint8_t>(std::next(this->sdr_data.begin(), 5), std::next(this->sdr_data.begin(), 8));
}

void SensorDataRecordSensor::validate() const {
	SensorDataRecord::validate();
	// Check that the ID field's Type/Length specification is valid.
	if (this->sdr_data.size() < this->_get_id_string_offset()+1U /* One byte of ID content */ +1U /* Change offset of last, to size of container */)
		throw invalid_sdr_error("Truncated SDR");
	unsigned idlen = ipmi_type_length_field_get_length(std::vector<uint8_t>(std::next(this->sdr_data.begin(), this->_get_id_string_offset()), this->sdr_data.end()));
	if (idlen > 17) // IDString TypeLengthCode (= 1) + IDString Bytes (<= 16)
		throw invalid_sdr_error("Invalid ID string");
	if (this->_get_id_string_offset()+idlen < this->sdr_data.size())
		throw invalid_sdr_error("Truncated ID string");
}

void SensorDataRecordSensor::initialize_blank(std::string name) {
	if (name.size() > 16) // Specification limit
		throw std::domain_error("Sensor names must be <= 16 characters.");
	this->sdr_data.resize(0); // Clear
	this->sdr_data.resize(this->_get_id_string_offset(), 0); // Initialize Blank Fields
	this->sdr_data.push_back(0xC0 | name.size()); // Type/Length code: Raw ASCII/Unicode, with length.
	for (auto it = name.begin(), eit = name.end(); it != eit; ++it)
		this->sdr_data.push_back(static_cast<uint8_t>(*it));
	this->sdr_data[2] = 0x51; // Set SDR version
	this->sdr_data[3] = this->parsed_record_type(); // Set record type
	this->sdr_data[4] = this->sdr_data.size() - 5; // Number of remaining bytes.
}

/// Create a bitmask with the lower `nbits` bits set.
#define LOWBITS(nbits) (0xff >> (8-(nbits)))

/// Define a `type` type SDR_FIELD from byte `byte`[a:b].
#define SDR_FIELD(name, type, byte, a, b, attributes) \
	type SensorDataRecordSensor::name() const attributes { \
		this->validate(); \
		return static_cast<type>((this->sdr_data[byte] >> (b)) & LOWBITS((a)-(b)+1)); \
	} \
	void SensorDataRecordSensor::name(type val) attributes { \
		if ((static_cast<uint8_t>(val) & LOWBITS((a)-(b)+1)) != static_cast<uint8_t>(val)) \
			throw std::domain_error("The supplied value does not fit correctly in the field."); \
		this->validate(); \
		this->sdr_data[byte] &= ~(LOWBITS((a)-(b)+1)<<(b)); /* Erase old value */ \
		this->sdr_data[byte] |= static_cast<uint8_t>(val)<<(b); /* Set new value */ \
	}

SDR_FIELD(sensor_owner_id, uint8_t, 5, 7, 0, )
SDR_FIELD(sensor_owner_channel, uint8_t, 6, 7, 4, )
SDR_FIELD(sensor_owner_lun, uint8_t, 6, 2, 0, )
SDR_FIELD(sensor_number, uint8_t, 7, 7, 0, )

SDR_FIELD(entity_id, uint8_t, 8, 7, 0, )
SDR_FIELD(entity_instance_is_container, bool, 9, 7, 7, )
SDR_FIELD(entity_instance, uint8_t, 9, 6, 0, )

std::string SensorDataRecordSensor::id_string() const {
	this->validate();
	return render_ipmi_type_length_field(std::vector<uint8_t>(std::next(this->sdr_data.begin(), this->_get_id_string_offset()), this->sdr_data.end()));
}
void SensorDataRecordSensor::id_string(std::string val) {
	this->validate();
	if (val.size() > 16) // Specified.
		throw std::domain_error("Sensor names must be <= 16 characters.");

	// Capture extended state data.
	std::vector<uint8_t> ext_data(std::next(this->sdr_data.begin(), this->_get_ext_data_offset()), this->sdr_data.end());

	this->sdr_data.resize(this->_get_id_string_offset()); // Downsize.
	this->sdr_data.push_back(0xC0 | val.size()); // Type/Length code: Raw ASCII/Unicode, with length.
	for (auto it = val.begin(), eit = val.end(); it != eit; ++it)
		this->sdr_data.push_back(static_cast<uint8_t>(*it));

	// Restore extended state data.
	this->sdr_data.insert(this->sdr_data.end(), ext_data.begin(), ext_data.end());
}

std::vector<uint8_t> SensorDataRecordSensor::u8export(uint8_t self_ipmb_addr, uint8_t self_ipmb_channel) {
	std::vector<uint8_t> out = SensorDataRecord::u8export(self_ipmb_addr, self_ipmb_channel);
	if (out[5] == 0)
		out[5] = self_ipmb_addr;
	if ((out[6] & 0xf0) == 0)
		out[6] |= self_ipmb_channel << 4;
	return out;
}

uint8_t SensorDataRecordSensor::_get_ext_data_offset() const {
	return this->_get_id_string_offset() + ipmi_type_length_field_get_length(std::vector<uint8_t>(std::next(this->sdr_data.begin(), this->_get_id_string_offset()), this->sdr_data.end()));
}
