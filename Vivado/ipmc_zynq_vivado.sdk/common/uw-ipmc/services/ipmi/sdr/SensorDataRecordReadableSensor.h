/*
 * SensorDataRecordReadableSensor.h
 *
 *  Created on: Aug 8, 2018
 *      Author: jtikalsky
 */

#ifndef SRC_COMMON_UW_IPMC_SERVICES_IPMI_SDR_SENSORDATARECORDREADABLESENSOR_H_
#define SRC_COMMON_UW_IPMC_SERVICES_IPMI_SDR_SENSORDATARECORDREADABLESENSOR_H_

#include <services/ipmi/sdr/SensorDataRecordSensor.h>

class SensorDataRecordReadableSensor: virtual public SensorDataRecordSensor {
protected:
	// Instantiate a Readable Sensor Record
	SensorDataRecordReadableSensor(const std::vector<uint8_t> &sdr_data) : SensorDataRecordSensor(sdr_data) { };
public:
	virtual ~SensorDataRecordReadableSensor() { };

	/**
	 * SDR Data Accessors
	 *
	 * \warning Do not call any accessors on a record that does not validate().
	 */
	///@{
#define SDR_FIELD(name, type, byte, a, b) \
	virtual type name(); \
	virtual void name(type val);

	SDR_FIELD(sensor_setable, bool, 10, 7, 7)
	SDR_FIELD(scanning_enabled, bool, 10, 6, 6)
	SDR_FIELD(events_enabled, bool, 10, 5, 5)
	SDR_FIELD(initialize_thresholds, bool, 10, 4, 4)
	SDR_FIELD(initialize_hysteresis, bool, 10, 3, 3)
	SDR_FIELD(initialize_sensor_type, bool, 10, 2, 2)
	SDR_FIELD(events_enabled_default, bool, 10, 1, 1)
	SDR_FIELD(scanning_enabled_default, bool, 10, 0, 0)

	SDR_FIELD(ignore_if_entity_absent, bool, 11, 7, 7)
	SDR_FIELD(sensor_auto_rearm, bool, 11, 6, 6)
	SDR_FIELD(sensor_hysteresis_support, uint8_t, 11, 5, 4)
	SDR_FIELD(sensor_threshold_access_support, uint8_t, 11, 3, 2)
	SDR_FIELD(sensor_event_message_control_support, uint8_t, 11, 1, 0)

	SDR_FIELD(sensor_type_code, uint8_t, 12, 7, 0)
	SDR_FIELD(event_type_reading_code, uint8_t, 13, 7, 0)

	/// Assertion / Lower Threshold Reading Mask
	virtual uint16_t assertion_lower_threshold_reading_mask();
	virtual void assertion_lower_threshold_reading_mask(uint16_t val);

	/// Deassertion / Upper Threshold Reading Mask
	virtual uint16_t deassertion_upper_threshold_reading_mask();
	virtual void deassertion_upper_threshold_reading_mask(uint16_t val);

	/// Discrete Reading Mask / Setable Threshold Reading Mask
	virtual uint16_t discrete_reading_setable_threshold_reading_mask();
	virtual void discrete_reading_setable_threshold_reading_mask(uint16_t val);

	enum UnitsNumericFormat {
		UNITS_UNSIGNED     = 0,
		UNITS_1SCOMPLEMENT = 1,
		UNITS_2SCOMPLEMENT = 2,
		UNITS_NONNUMERIC   = 3,
	};
	SDR_FIELD(units_numeric_format, enum UnitsNumericFormat, 20, 7, 6)

	enum UnitsRateUnit {
		RATE_UNIT_NONE     = 0,
		RATE_UNIT_USEC     = 1,
		RATE_UNIT_MSEC     = 2,
		RATE_UNIT_SEC      = 3,
		RATE_UNIT_MIN      = 4,
		RATE_UNIT_HOUR     = 5,
		RATE_UNIT_DAY      = 6,
		RATE_UNIT_RESERVED = 7,
	};
	SDR_FIELD(units_rate_unit, enum UnitsRateUnit, 20, 5, 3)

	enum UnitsModifierUnit {
		MODIFIER_UNIT_NONE     = 0,
		MODIFIER_UNIT_DIVIDE   = 1,
		MODIFIER_UNIT_MULTIPLY = 2,
		MODIFIER_UNIT_RESERVED = 3,
	};
	SDR_FIELD(units_modifier_unit, enum UnitsModifierUnit, 20, 2, 1)
	SDR_FIELD(units_percentage, bool, 20, 0, 0)

	SDR_FIELD(hysteresis_high, uint8_t, VARIABLE, 7, 0) // Different in Type 02
	SDR_FIELD(hysteresis_low, uint8_t, VARIABLE, 7, 0) // Different in Type 02
#undef SDR_FIELD
	///@}
	static const std::map<uint8_t, std::string> sensor_unit_type_codes; ///< A table of human readable sensor unit types.
};

#endif /* SRC_COMMON_UW_IPMC_SERVICES_IPMI_SDR_SENSORDATARECORDREADABLESENSOR_H_ */
