/*
 * SensorDataRecord02.cpp
 *
 *  Created on: Aug 3, 2018
 *      Author: jtikalsky
 */

#include <services/ipmi/sdr/SensorDataRecord02.h>
#include <IPMC.h>

bool SensorDataRecord02::validate() const {
	if (!SensorDataRecordReadableSensor::validate())
		return false;
	if (!SensorDataRecordSharedSensor::validate())
		return false;
	if (this->record_type() != 0x02)
		return false;
	return true;
}

void SensorDataRecord02::initialize_blank(std::string name) {
	SensorDataRecordSensor::initialize_blank(name);
	this->sdr_data[20] |= 0xc0; // Reserved, write 11b, by spec.
	this->share_count(1); // We have at least one, not zero, sensors that we refer to by default.
}

/// Create a bitmask with the lower `nbits` bits set.
#define LOWBITS(nbits) (0xff >> (8-(nbits)))

/// Define a `type` type SDR_FIELD from byte `byte`[a:b].
#define SDR_FIELD(name, type, byte, a, b, attributes) \
	type SensorDataRecord02::name() const attributes { \
		configASSERT(this->validate()); \
		return static_cast<type>((this->sdr_data[byte] >> (b)) & LOWBITS((a)-(b)+1)); \
	} \
	void SensorDataRecord02::name(type val) attributes { \
		configASSERT((static_cast<uint8_t>(val) & LOWBITS((a)-(b)+1)) == static_cast<uint8_t>(val)); \
		configASSERT(this->validate()); \
		this->sdr_data[byte] &= ~(LOWBITS((a)-(b)+1)<<(b)); /* Erase old value */ \
		this->sdr_data[byte] |= static_cast<uint8_t>(val)<<(b); /* Set new value */ \
	}

SDR_FIELD(sensor_direction, enum SensorDataRecordReadableSensor::Direction, 23, 7, 6, )

SDR_FIELD(id_string_instance_modifier_type, enum SensorDataRecordSharedSensor::IDStringInstanceModifierType, 23, 5, 4, )
SDR_FIELD(share_count, uint8_t, 23, 3, 0, )
SDR_FIELD(entity_instance_sharing, uint8_t, 24, 7, 7, )
SDR_FIELD(id_string_instance_modifier_offset, uint8_t, 23, 6, 0, )

SDR_FIELD(hysteresis_high, uint8_t, 25, 7, 0, )
SDR_FIELD(hysteresis_low, uint8_t, 26, 7, 0, )

SDR_FIELD(oem, uint8_t, 30, 7, 0, )

uint8_t SensorDataRecord02::from_float(float value) const {
	// SDR Type 02 don't specify any conversion method.  Convert as y=x.

	uint8_t raw_discrete_value = static_cast<uint16_t>(value);
	// Resolve domain errors.
	if (value >= 0xff)
		raw_discrete_value = 0xff;
	if (value <= 0)
		raw_discrete_value = 0;
	return raw_discrete_value;
}

float SensorDataRecord02::to_float(uint8_t value) const {
	// SDR Type 02 don't specify any conversion method.  Convert as y=x.
	return static_cast<float>(value);
}
