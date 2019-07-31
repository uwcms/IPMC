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

#ifndef SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_SDR_SENSOR_DATA_RECORD_SENSOR_H_
#define SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_SDR_SENSOR_DATA_RECORD_SENSOR_H_

#include "sensor_data_record.h"
#include <map>
#include <string>

/**
 * A generic intermediary class providing the common interface between type 01,
 * type 02, type 03 SDRs.
 */
class SensorDataRecordSensor : public SensorDataRecord {
protected:
	//! Protected constructor since our type-code based accessors are technically virtual
	SensorDataRecordSensor(const std::vector<uint8_t> &sdr_data = std::vector<uint8_t>()) : SensorDataRecord(sdr_data) { };

public:
	virtual ~SensorDataRecordSensor() { };

	virtual std::vector<uint8_t> recordKey() const;
	virtual void validate() const;
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

	SDR_FIELD(sensor_owner_id, uint8_t, 5, 7, 0, )
	SDR_FIELD(sensor_owner_channel, uint8_t, 6, 7, 4, )
	SDR_FIELD(sensor_owner_lun, uint8_t, 6, 2, 0, )
	SDR_FIELD(sensor_number, uint8_t, 7, 7, 0, )

	SDR_FIELD(entity_id, uint8_t, 8, 7, 0, )
	SDR_FIELD(entity_instance_is_container, bool, 9, 7, 7, )
	SDR_FIELD(entity_instance, uint8_t, 9, 6, 0, )

	SDR_FIELD(sensor_type_code, uint8_t, VARIABLE, 7, 0, =0) // Different in Type 03

	//! A convenience constant for the Event/Reading Type Code "Threshold".  (A sensor is discrete if this is not its event/reading type code.)
	static const uint8_t EVENT_TYPE_THRESHOLD_SENSOR = 0x01;
	SDR_FIELD(event_type_reading_code, uint8_t, VARIABLE, 7, 0, =0) // Different in Type 03

	enum Direction {
		DIR_UNSPECIFIED = 0,
		DIR_INPUT       = 1,
		DIR_OUTPUT      = 2,
		DIR_RESERVED    = 3,
	};
	SDR_FIELD(sensor_direction, enum Direction, VARIABLE, 1, 0, =0)

	SDR_FIELD(oem, uint8_t, VARIABLE, 7, 0, =0)

	SDR_FIELD(id_string, std::string, VARIABLE, 7, 0, )

#undef SDR_FIELD

	virtual std::vector<uint8_t> u8export(uint8_t self_ipmb_addr=0, uint8_t self_ipmb_channel=0) const;

protected:
	virtual uint8_t getIdStringOffset() const = 0;
	virtual uint8_t getExtendedDataOffset() const;
	///@}
};

#endif /* SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_SDR_SENSOR_DATA_RECORD_SENSOR_H_ */
