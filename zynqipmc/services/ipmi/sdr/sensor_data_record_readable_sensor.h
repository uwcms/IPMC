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

#ifndef SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_SDR_SENSORDATARECORDREADABLESENSOR_H_
#define SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_SDR_SENSORDATARECORDREADABLESENSOR_H_

#include <services/ipmi/sdr/sensor_data_record_sensor.h>

class SensorDataRecordReadableSensor: virtual public SensorDataRecordSensor {
protected:
	// Instantiate a Readable Sensor Record
	SensorDataRecordReadableSensor(const std::vector<uint8_t> &sdr_data = std::vector<uint8_t>()) : SensorDataRecordSensor(sdr_data) { };

public:
	virtual ~SensorDataRecordReadableSensor() { };

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

	SDR_FIELD(sensor_setable, bool, 10, 7, 7, )
	SDR_FIELD(initialize_scanning_enabled, bool, 10, 6, 6, )
	SDR_FIELD(initialize_events_enabled, bool, 10, 5, 5, )
	SDR_FIELD(initialize_thresholds, bool, 10, 4, 4, )
	SDR_FIELD(initialize_hysteresis, bool, 10, 3, 3, )
	SDR_FIELD(initialize_sensor_type, bool, 10, 2, 2, )
	SDR_FIELD(events_enabled_default, bool, 10, 1, 1, )
	SDR_FIELD(scanning_enabled_default, bool, 10, 0, 0, )

	SDR_FIELD(ignore_if_entity_absent, bool, 11, 7, 7, )
	SDR_FIELD(sensor_auto_rearm, bool, 11, 6, 6, )
	enum ThresholdHysteresisAccessSupport {
		ACCESS_ABSENT     = 0,
		ACCESS_READONLY   = 1,
		ACCESS_READWRITE  = 2,
		ACCESS_FIXED      = 3,
	};
	SDR_FIELD(sensor_hysteresis_support, enum ThresholdHysteresisAccessSupport, 11, 5, 4, )
	SDR_FIELD(sensor_threshold_access_support, enum ThresholdHysteresisAccessSupport, 11, 3, 2, )
	enum EventMessageControlSupport {
		EVTCTRL_GRANULAR  = 0, // per threshold/discrete-state event enable/disable (implies sensor & global disable support)
		EVTCTRL_SENSOR    = 1, // entire sensor only (implies global disable support)
		EVTCTRL_GLOBAL    = 2, // global disable only
		EVTCTRL_NO_EVENTS = 3, // no events from sensor
	};
	SDR_FIELD(sensor_event_message_control_support, enum EventMessageControlSupport, 11, 1, 0, )

	SDR_FIELD(sensor_type_code, uint8_t, 12, 7, 0, )
	SDR_FIELD(event_type_reading_code, uint8_t, 13, 7, 0, )

	//! Threshold comparisons returned.
	virtual uint8_t threshold_comparisons_returned() const;
	virtual void threshold_comparisons_returned(uint8_t val);

	//! Assertion / lower Threshold Reading Mask
	virtual uint16_t assertion_lower_threshold_reading_mask() const;
	virtual void assertion_lower_threshold_reading_mask(uint16_t val);

	//! Deassertion / upper Threshold Reading Mask
	virtual uint16_t deassertion_upper_threshold_reading_mask() const;
	virtual void deassertion_upper_threshold_reading_mask(uint16_t val);

	//! Discrete Reading Mask / setable Threshold Reading Mask
	virtual uint16_t discrete_reading_setable_threshold_reading_mask() const;
	virtual void discrete_reading_setable_threshold_reading_mask(uint16_t val);

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
	SDR_FIELD(units_rate_unit, enum UnitsRateUnit, 20, 5, 3, )

	enum UnitsModifierUnitMethod {
		MODIFIER_UNIT_NONE     = 0,
		MODIFIER_UNIT_DIVIDE   = 1,
		MODIFIER_UNIT_MULTIPLY = 2,
		MODIFIER_UNIT_RESERVED = 3,
	};
	SDR_FIELD(units_modifier_unit_method, enum UnitsModifierUnitMethod, 20, 2, 1, )
	SDR_FIELD(units_percentage, bool, 20, 0, 0, )

	SDR_FIELD(units_base_unit, uint8_t, 21, 7, 1, )
	SDR_FIELD(units_modifier_unit, uint8_t, 22, 7, 1, )

	SDR_FIELD(hysteresis_high, uint8_t, VARIABLE, 7, 0, =0) // Different in Type 02
	SDR_FIELD(hysteresis_low, uint8_t, VARIABLE, 7, 0, =0) // Different in Type 02
#undef SDR_FIELD
	///@}

	/**
	 * Convert a floating point value to a one-byte IPMI value using the formula
	 * specified by this SDR.
	 *
	 * @param value A float sensor value.
	 * @return A one-byte IPMI sensor value.
	 */
	virtual uint8_t fromFloat(float value) const = 0;

	/**
	 * Convert a one-byte IPMI value to a floating point value using the formula
	 * specified by this SDR.
	 *
	 * @param value A one-byte IPMI sensor value.
	 * @return A float sensor value.
	 */
	virtual float toFloat(uint8_t value) const = 0;

	//! A table of human readable sensor unit types.
	static const std::map<uint8_t, std::string> sensor_unit_type_codes;
};

#endif /* SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_SDR_SENSORDATARECORDREADABLESENSOR_H_ */
