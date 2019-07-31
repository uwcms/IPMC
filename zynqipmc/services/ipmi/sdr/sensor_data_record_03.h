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

#ifndef SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_SDR_SENSORRECORD03_H_
#define SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_SDR_SENSORRECORD03_H_

#include <services/ipmi/sdr/sensor_data_record_readable_sensor.h>
#include <services/ipmi/sdr/sensor_data_record_shared_sensor.h>
#include <stdint.h>
#include <vector>

/**
 * A SensorDataRecord representing and parsing a Type 03 SDR.
 */
class SensorDataRecord03 : public SensorDataRecordSharedSensor {
public:
	//! Instantiate a Type 02 SensorDataRecord
	SensorDataRecord03(const std::vector<uint8_t> &sdr_data = std::vector<uint8_t>()) : SensorDataRecordSensor(sdr_data), SensorDataRecordSharedSensor(sdr_data) { };
	virtual ~SensorDataRecord03() { };

	virtual void validate() const;
	virtual uint8_t parsedRecordType() const { return 0x03; };

	/**
	 * SDR Data Accessors
	 *
	 * @warning Do not call any accessors on a record that does not validate().
	 */
	///@{
	// Helper macro to instantiate getters and setters.
	// a & b represent bit fields used in the source file.
#define SDR_FIELD(name, type, byte, a, b, attributes) \
	virtual type name() const attributes; \
	virtual void name(type val) attributes;

	SDR_FIELD(sensor_type_code, uint8_t, 10, 7, 0, )
	SDR_FIELD(event_type_reading_code, uint8_t, 11, 7, 0, )

	SDR_FIELD(sensor_direction, enum SensorDataRecordReadableSensor::Direction, 12, 7, 6, )

	SDR_FIELD(id_string_instance_modifier_type, enum SensorDataRecordSharedSensor::IDStringInstanceModifierType, 12, 5, 4, )
	SDR_FIELD(share_count, uint8_t, 12, 3, 0, )
	SDR_FIELD(entity_instance_sharing, uint8_t, 13, 7, 7, )
	SDR_FIELD(id_string_instance_modifier_offset, uint8_t, 13, 6, 0, )

	SDR_FIELD(oem, uint8_t, 15, 7, 0, )

#undef SDR_FIELD
	virtual uint8_t getIdStringOffset() const { return 16; };
	///@}
};

#endif /* SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_SDR_SENSORRECORD03_H_ */
