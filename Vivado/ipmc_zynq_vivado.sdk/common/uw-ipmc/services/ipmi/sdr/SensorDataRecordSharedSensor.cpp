/*
 * SensorDataRecord02.cpp
 *
 *  Created on: Aug 3, 2018
 *      Author: jtikalsky
 */

#include <services/ipmi/sdr/SensorDataRecord02.h>
#include <IPMC.h>

/// Create a bitmask with the lower `nbits` bits set.
#define LOWBITS(nbits) (0xff >> (8-(nbits)))

/// Define a `type` type SDR_FIELD from byte `byte`[a:b].
#define SDR_FIELD(name, type, byte, a, b) \
	type SensorDataRecordSharedSensor::name() const { \
		configASSERT(this->validate()); \
		return static_cast<type>((this->sdr_data[byte] >> (b)) & LOWBITS((a)-(b)+1)); \
	} \
	void SensorDataRecordSharedSensor::name(type val) { \
		configASSERT((static_cast<uint8_t>(val) & LOWBITS((a)-(b)+1)) == static_cast<uint8_t>(val)); \
		configASSERT(this->validate()); \
		this->sdr_data[byte] &= ~(LOWBITS((a)-(b)+1)<<(b)); /* Erase old value */ \
		this->sdr_data[byte] |= static_cast<uint8_t>(val)<<(b); /* Set new value */ \
	}

enum SensorDataRecordSharedSensor::IDStringInstanceModifierType SensorDataRecordSharedSensor::id_string_instance_modifier_type() const {
	configASSERT(0); // How'd you construct this object?  It should be virtual.
	return IDMOD_NUMERIC;
}
void SensorDataRecordSharedSensor::id_string_instance_modifier_type(enum SensorDataRecordSharedSensor::IDStringInstanceModifierType val) {
	configASSERT(0); // How'd you construct this object?  It should be virtual.
}

uint8_t SensorDataRecordSharedSensor::share_count() const {
	configASSERT(0); // How'd you construct this object?  It should be virtual.
	return 0;
}
void SensorDataRecordSharedSensor::share_count(uint8_t val) {
	configASSERT(0); // How'd you construct this object?  It should be virtual.
}

uint8_t SensorDataRecordSharedSensor::entity_instance_sharing() const {
	configASSERT(0); // How'd you construct this object?  It should be virtual.
	return 0;
}
void SensorDataRecordSharedSensor::entity_instance_sharing(uint8_t val) {
	configASSERT(0); // How'd you construct this object?  It should be virtual.
}

uint8_t SensorDataRecordSharedSensor::id_string_instance_modifier_offset() const {
	configASSERT(0); // How'd you construct this object?  It should be virtual.
	return 0;
}
void SensorDataRecordSharedSensor::id_string_instance_modifier_offset(uint8_t val) {
	configASSERT(0); // How'd you construct this object?  It should be virtual.
}
