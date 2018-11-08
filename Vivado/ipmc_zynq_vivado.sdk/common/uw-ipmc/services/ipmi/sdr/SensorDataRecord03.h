/*
 * SensorDataRecord03.h
 *
 *  Created on: Aug 3, 2018
 *      Author: jtikalsky
 */

#ifndef SRC_COMMON_UW_IPMC_SERVICES_IPMI_SDR_SensorDataRecord03_H_
#define SRC_COMMON_UW_IPMC_SERVICES_IPMI_SDR_SensorDataRecord03_H_

#include <services/ipmi/sdr/SensorDataRecordReadableSensor.h>
#include <services/ipmi/sdr/SensorDataRecordSharedSensor.h>
#include <stdint.h>
#include <vector>

/**
 * A SensorDataRecord representing and parsing a Type 03 SDR.
 */
class SensorDataRecord03 : public SensorDataRecordSharedSensor {
public:
	/// Instantiate a Type 02 SensorDataRecord
	SensorDataRecord03(const std::vector<uint8_t> &sdr_data = std::vector<uint8_t>()) : SensorDataRecordSensor(sdr_data), SensorDataRecordSharedSensor(sdr_data) { };
	virtual ~SensorDataRecord03() { };
	virtual bool validate() const;
	virtual uint8_t parsed_record_type() const { return 0x03; };

	/**
	 * SDR Data Accessors
	 *
	 * \warning Do not call any accessors on a record that does not validate().
	 */
	///@{
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
	virtual uint8_t _get_id_string_offset() const { return 16; };
	///@}
};

#endif /* SRC_COMMON_UW_IPMC_SERVICES_IPMI_SDR_SensorDataRecord03_H_ */
