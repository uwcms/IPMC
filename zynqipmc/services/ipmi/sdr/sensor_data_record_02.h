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

#ifndef SRC_COMMON_ZYNCIPMC_SERVICES_IPMI_SDR_SENSORDATARECORD02_H_
#define SRC_COMMON_ZYNCIPMC_SERVICES_IPMI_SDR_SENSORDATARECORD02_H_

#include <services/ipmi/sdr/sensor_data_record_readable_sensor.h>
#include <services/ipmi/sdr/sensor_data_record_shared_sensor.h>
#include <stdint.h>
#include <vector>

/**
 * A SensorDataRecord representing and parsing a Type 02 SDR.
 */
class SensorDataRecord02 : public SensorDataRecordReadableSensor, SensorDataRecordSharedSensor {
public:
	//! Instantiate a Type 02 SensorDataRecord
	SensorDataRecord02(const std::vector<uint8_t> &sdr_data = std::vector<uint8_t>()) : SensorDataRecordSensor(sdr_data), SensorDataRecordReadableSensor(sdr_data), SensorDataRecordSharedSensor(sdr_data) { };
	virtual ~SensorDataRecord02() { };

	virtual void validate() const;
	virtual uint8_t parsedRecordType() const { return 0x02; };
	virtual void initializeBlank(std::string name);

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

	SDR_FIELD(sensor_direction, enum SensorDataRecordReadableSensor::Direction, 23, 7, 6, )

	SDR_FIELD(id_string_instance_modifier_type, enum SensorDataRecordSharedSensor::IDStringInstanceModifierType, 23, 5, 4, )
	SDR_FIELD(share_count, uint8_t, 23, 3, 0, )
	SDR_FIELD(entity_instance_sharing, uint8_t, 24, 7, 7, )
	SDR_FIELD(id_string_instance_modifier_offset, uint8_t, 23, 6, 0, )

	SDR_FIELD(hysteresis_high, uint8_t, 25, 7, 0, )
	SDR_FIELD(hysteresis_low, uint8_t, 26, 7, 0, )

	SDR_FIELD(oem, uint8_t, 30, 7, 0, )

#undef SDR_FIELD
	virtual uint8_t getIdStringOffset() const { return 31; };
	///@}

	virtual uint8_t fromFloat(float value) const;
	virtual float toFloat(uint8_t value) const;
};

#endif /* SRC_COMMON_ZYNCIPMC_SERVICES_IPMI_SDR_SENSORDATARECORD02_H_ */
