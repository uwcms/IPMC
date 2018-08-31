#include <IPMC.h>
#include <services/ipmi/IPMIFormats.h>
#include <services/ipmi/sdr/SensorDataRecordSensor.h>
#include <iterator>

std::vector<uint8_t> SensorDataRecordSensor::get_record_key() {
	configASSERT(this->validate());
	return std::vector<uint8_t>(std::next(this->sdr_data.begin(), 5), std::next(this->sdr_data.begin(), 8));
}

bool SensorDataRecordSensor::validate() {
	if (!SensorDataRecord::validate())
		return false;
	// Check that the ID field's Type/Length specification is valid.
	if (this->sdr_data.size() < this->_get_id_string_offset()+1U /* One byte of ID content */ +1U /* Change offset of last, to size of container */)
		return false;
	unsigned idlen = ipmi_type_length_field_get_length(std::vector<uint8_t>(std::next(this->sdr_data.begin(), this->_get_id_string_offset()), this->sdr_data.end()));
	if (idlen > 17) // IDString TypeLengthCode (= 1) + IDString Bytes (<= 16)
		return false;
	if (this->_get_id_string_offset()+idlen < this->sdr_data.size())
		return false;
	return true;
}

void SensorDataRecordSensor::initialize_blank(std::string name) {
	configASSERT(name.size() <= 16); // Specification limit
	this->sdr_data.resize(0); // Clear
	this->sdr_data.resize(this->_get_id_string_offset(), 0); // Initialize Blank Fields
	this->sdr_data.push_back(0xC0 | name.size()); // Type/Length code: Raw ASCII/Unicode, with length.
	for (auto it = name.begin(), eit = name.end(); it != eit; ++it)
		this->sdr_data.push_back(static_cast<uint8_t>(*it));
}

/// Create a bitmask with the lower `nbits` bits set.
#define LOWBITS(nbits) (0xff >> (8-(nbits)))

/// Define a `type` type SDR_FIELD from byte `byte`[a:b].
#define SDR_FIELD(name, type, byte, a, b) \
	type SensorDataRecordSensor::name() { \
		configASSERT(this->validate()); \
		return (this->sdr_data[byte] >> (b)) & LOWBITS((a)-(b)+1); \
	} \
	void SensorDataRecordSensor::name(type val) { \
		configASSERT((val & LOWBITS((a)-(b)+1)) == val); \
		configASSERT(this->validate()); \
		this->sdr_data[byte] &= ~(LOWBITS((a)-(b)+1)<<(b)); /* Erase old value */ \
		this->sdr_data[byte] |= val<<(b); /* Set new value */ \
	}

SDR_FIELD(sensor_owner_id, uint8_t, 5, 7, 0)
SDR_FIELD(sensor_owner_channel, uint8_t, 6, 7, 4)
SDR_FIELD(sensor_owner_lun, uint8_t, 6, 2, 0)
SDR_FIELD(sensor_number, uint8_t, 7, 7, 0)

SDR_FIELD(entity_id, uint8_t, 8, 7, 0)
SDR_FIELD(entity_instance_is_container, bool, 9, 7, 7)
SDR_FIELD(entity_instance, uint8_t, 9, 6, 0)

uint8_t SensorDataRecordSensor::sensor_type_code() {
	configASSERT(0); // How'd you construct this object?  It should be virtual.
	return 0;
}
void SensorDataRecordSensor::sensor_type_code(uint8_t val) {
	configASSERT(0); // How'd you construct this object?  It should be virtual.
}

uint8_t SensorDataRecordSensor::event_type_reading_code() {
	configASSERT(0); // How'd you construct this object?  It should be virtual.
	return 0;
}
void SensorDataRecordSensor::event_type_reading_code(uint8_t val) {
	configASSERT(0); // How'd you construct this object?  It should be virtual.
}

enum SensorDataRecordSensor::Direction SensorDataRecordSensor::sensor_direction() {
	configASSERT(0); // How'd you construct this object?  It should be virtual.
	return DIR_UNSPECIFIED;
}
void SensorDataRecordSensor::sensor_direction(enum SensorDataRecordSensor::Direction val) {
	configASSERT(0); // How'd you construct this object?  It should be virtual.
}

uint8_t SensorDataRecordSensor::oem() {
	configASSERT(0); // How'd you construct this object?  It should be virtual.
	return 0;
}
void SensorDataRecordSensor::oem(uint8_t val) {
	configASSERT(0); // How'd you construct this object?  It should be virtual.
}

std::string SensorDataRecordSensor::id_string() {
	configASSERT(0); // How'd you construct this object?  It should be virtual.
	return 0;
}
void SensorDataRecordSensor::id_string(std::string val) {
	configASSERT(0); // How'd you construct this object?  It should be virtual.
}

uint8_t SensorDataRecordSensor::_get_id_string_offset() {
	configASSERT(0); // How'd you construct this object?  It should be virtual.
	return 0;
}
