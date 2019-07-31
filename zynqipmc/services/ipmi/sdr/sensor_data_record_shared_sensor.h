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

#ifndef SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_SDR_SENSOR_DATA_RECORD_SHARED_SENSOR_H_
#define SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_SDR_SENSOR_DATA_RECORD_SHARED_SENSOR_H_

#include <services/ipmi/sdr/sensor_data_record_sensor.h>
#include <stdint.h>
#include <vector>

/**
 * A SensorDataRecord representing and parsing a Type 01 SDR.
 */
class SensorDataRecordSharedSensor : virtual public SensorDataRecordSensor {
protected:
	//! Instantiate a Type 01 SensorDataRecord
	SensorDataRecordSharedSensor(const std::vector<uint8_t> &sdr_data = std::vector<uint8_t>()) : SensorDataRecordSensor(sdr_data) { };

public:
	virtual ~SensorDataRecordSharedSensor() { };

	/**
	 * SDR Data Accessors
	 *
	 * @warning Do not call any accessors on a record that does not validate().
	 */
	///@{
#define SDR_FIELD(name, type, byte, a, b, attributes) \
	virtual type name() const attributes; \
	virtual void name(type val) attributes;

	enum IDStringInstanceModifierType {
		IDMOD_NUMERIC = 0,
		IDMOD_ALPHA   = 1,
	};
	SDR_FIELD(id_string_instance_modifier_type, enum IDStringInstanceModifierType, VARIABLE, 5, 4, = 0)
	SDR_FIELD(share_count, uint8_t, VARIBLE, 3, 0, = 0)
	SDR_FIELD(entity_instance_sharing, uint8_t, VARIABLE, 7, 7, = 0)
	SDR_FIELD(id_string_instance_modifier_offset, uint8_t, VARIABLE, 6, 0, = 0)

#undef SDR_FIELD
	///@}
};

#endif /* SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_SDR_SENSOR_DATA_RECORD_SHARED_SENSOR_H_ */
