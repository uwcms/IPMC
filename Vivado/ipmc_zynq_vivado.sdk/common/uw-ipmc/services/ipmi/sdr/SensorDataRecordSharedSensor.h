/*
 * SensorDataRecord02.h
 *
 *  Created on: Aug 3, 2018
 *      Author: jtikalsky
 */

#ifndef SRC_COMMON_UW_IPMC_SERVICES_IPMI_SDR_SensorDataRecordSharedSensor_H_
#define SRC_COMMON_UW_IPMC_SERVICES_IPMI_SDR_SensorDataRecordSharedSensor_H_

#include <services/ipmi/sdr/SensorDataRecordSensor.h>
#include <stdint.h>
#include <vector>

/**
 * A SensorDataRecord representing and parsing a Type 01 SDR.
 */
class SensorDataRecordSharedSensor : virtual public SensorDataRecordSensor {
protected:
	/// Instantiate a Type 01 SensorDataRecord
	SensorDataRecordSharedSensor(const std::vector<uint8_t> &sdr_data = std::vector<uint8_t>()) : SensorDataRecordSensor(sdr_data) { };
public:
	virtual ~SensorDataRecordSharedSensor() { };

	/**
	 * SDR Data Accessors
	 *
	 * \warning Do not call any accessors on a record that does not validate().
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

#endif /* SRC_COMMON_UW_IPMC_SERVICES_IPMI_SDR_SensorDataRecord02_H_ */
