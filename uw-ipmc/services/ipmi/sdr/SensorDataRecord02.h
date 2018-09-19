/*
 * SensorDataRecord02.h
 *
 *  Created on: Aug 3, 2018
 *      Author: jtikalsky
 */

#ifndef SRC_COMMON_UW_IPMC_SERVICES_IPMI_SDR_SensorDataRecord02_H_
#define SRC_COMMON_UW_IPMC_SERVICES_IPMI_SDR_SensorDataRecord02_H_

#include <services/ipmi/sdr/SensorDataRecordReadableSensor.h>
#include <services/ipmi/sdr/SensorDataRecordSharedSensor.h>
#include <stdint.h>
#include <vector>

/**
 * A SensorDataRecord representing and parsing a Type 02 SDR.
 */
class SensorDataRecord02 : public SensorDataRecordReadableSensor, SensorDataRecordSharedSensor {
public:
	/// Instantiate a Type 02 SensorDataRecord
	SensorDataRecord02(const std::vector<uint8_t> &sdr_data = std::vector<uint8_t>()) : SensorDataRecordSensor(sdr_data), SensorDataRecordReadableSensor(sdr_data), SensorDataRecordSharedSensor(sdr_data) { };
	virtual ~SensorDataRecord02() { };
	virtual bool validate() const;
	virtual uint8_t parsed_record_type() const { return 0x02; };
	virtual void initialize_blank(std::string name);

	/**
	 * SDR Data Accessors
	 *
	 * \warning Do not call any accessors on a record that does not validate().
	 */
	///@{
#define SDR_FIELD(name, type, byte, a, b) \
	virtual type name() const; \
	virtual void name(type val);

	SDR_FIELD(sensor_direction, enum SensorDataRecordReadableSensor::Direction, 23, 7, 6)

	SDR_FIELD(id_string_instance_modifier_type, enum SensorDataRecordSharedSensor::IDStringInstanceModifierType, 23, 5, 4)
	SDR_FIELD(share_count, uint8_t, 23, 3, 0)
	SDR_FIELD(entity_instance_sharing, uint8_t, 24, 7, 7)
	SDR_FIELD(id_string_instance_modifier_offset, uint8_t, 23, 6, 0)

	SDR_FIELD(hysteresis_high, uint8_t, 25, 7, 0)
	SDR_FIELD(hysteresis_low, uint8_t, 26, 7, 0)

	SDR_FIELD(oem, uint8_t, 30, 7, 0)

#undef SDR_FIELD
	virtual uint8_t _get_id_string_offset() const { return 31; };
	///@}

};

#endif /* SRC_COMMON_UW_IPMC_SERVICES_IPMI_SDR_SensorDataRecord02_H_ */
