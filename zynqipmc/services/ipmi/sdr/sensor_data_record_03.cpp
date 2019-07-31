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

#include <services/ipmi/sdr/sensor_data_record_03.h>

void SensorDataRecord03::validate() const {
	SensorDataRecordSharedSensor::validate();
	if (this->recordType() != 0x03) {
		throw invalid_sdr_error("SensorDataRecord03 supports only type 03h SDRs.");
	}
}

/// Create a bitmask with the lower `nbits` bits set.
#define LOWBITS(nbits) (0xff >> (8-(nbits)))

/// Define a `type` type SDR_FIELD from byte `byte`[a:b].
#define SDR_FIELD(name, type, byte, a, b, attributes) \
	type SensorDataRecord03::name() const attributes { \
		this->validate(); \
		return static_cast<type>((this->sdr_data[byte] >> (b)) & LOWBITS((a)-(b)+1)); \
	} \
	void SensorDataRecord03::name(type val) attributes { \
		if ((static_cast<uint8_t>(val) & LOWBITS((a)-(b)+1)) != static_cast<uint8_t>(val)) \
			throw std::domain_error("The supplied value does not fit correctly in the field."); \
		this->validate(); \
		this->sdr_data[byte] &= ~(LOWBITS((a)-(b)+1)<<(b)); /* Erase old value */ \
		this->sdr_data[byte] |= static_cast<uint8_t>(val)<<(b); /* Set new value */ \
	}

SDR_FIELD(sensor_type_code, uint8_t, 10, 7, 0, )
SDR_FIELD(event_type_reading_code, uint8_t, 11, 7, 0, )

SDR_FIELD(sensor_direction, enum SensorDataRecordReadableSensor::Direction, 12, 7, 6, )

SDR_FIELD(id_string_instance_modifier_type, enum SensorDataRecordSharedSensor::IDStringInstanceModifierType, 12, 5, 4, )
SDR_FIELD(share_count, uint8_t, 12, 3, 0, )
SDR_FIELD(entity_instance_sharing, uint8_t, 13, 7, 7, )
SDR_FIELD(id_string_instance_modifier_offset, uint8_t, 13, 6, 0, )

SDR_FIELD(oem, uint8_t, 15, 7, 0, )
