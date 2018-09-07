/*
 * SensorDataRecord03.cpp
 *
 *  Created on: Aug 3, 2018
 *      Author: jtikalsky
 */

#include <services/ipmi/sdr/SensorDataRecord03.h>
#include <IPMC.h>

bool SensorDataRecord03::validate() const {
	if (!SensorDataRecordSharedSensor::validate())
		return false;
	if (this->record_type() != 0x03)
		return false;
	return true;
}

/// Create a bitmask with the lower `nbits` bits set.
#define LOWBITS(nbits) (0xff >> (8-(nbits)))

/// Define a `type` type SDR_FIELD from byte `byte`[a:b].
#define SDR_FIELD(name, type, byte, a, b) \
	type SensorDataRecord03::name() const { \
		configASSERT(this->validate()); \
		return static_cast<type>((this->sdr_data[byte] >> (b)) & LOWBITS((a)-(b)+1)); \
	} \
	void SensorDataRecord03::name(type val) { \
		configASSERT((static_cast<uint8_t>(val) & LOWBITS((a)-(b)+1)) == static_cast<uint8_t>(val)); \
		configASSERT(this->validate()); \
		this->sdr_data[byte] &= ~(LOWBITS((a)-(b)+1)<<(b)); /* Erase old value */ \
		this->sdr_data[byte] |= static_cast<uint8_t>(val)<<(b); /* Set new value */ \
	}

SDR_FIELD(sensor_direction, enum SensorDataRecordReadableSensor::Direction, 12, 7, 6)

SDR_FIELD(id_string_instance_modifier_type, enum SensorDataRecordSharedSensor::IDStringInstanceModifierType, 12, 5, 4)
SDR_FIELD(share_count, uint8_t, 12, 3, 0)
SDR_FIELD(entity_instance_sharing, uint8_t, 13, 7, 7)
SDR_FIELD(id_string_instance_modifier_offset, uint8_t, 13, 6, 0)

SDR_FIELD(oem, uint8_t, 15, 7, 0)
