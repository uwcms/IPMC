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

#ifndef SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_SENSOR_THRESHOLD_SENSOR_H_
#define SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_SENSOR_THRESHOLD_SENSOR_H_

#include <services/ipmi/sdr/sensor_data_record_01.h>
#include <services/ipmi/sensor/sensor.h>

/**
 * A standard threshold sensor.
 */
class ThresholdSensor final : public Sensor {
public:
	/**
	 * Instantiate a standard threshold sensor.
	 * @param sdr_key The SDR key bytes to retrieve a matching SDR from the device_sdr_repo.
	 * @param log A LogTree for this object.
	 * @param initialize_on_first_update If true, the first updateValue() call will initialize the sensor only and not send event messages.
	 */
	ThresholdSensor(const std::vector<uint8_t> &sdr_key, LogTree &log);
	virtual ~ThresholdSensor();

	// If these have never been set (i.e. are 0xffff), for THRESHOLD sensors, we'll default to only Upper going-high and Lower going-low enabled.
	virtual uint16_t getAssertionEventsEnabled() { return this->assertion_events_enabled == 0xffff ? 0x0a95 : this->assertion_events_enabled; };
	virtual uint16_t getDeassertionEventsEnabled() { return this->deassertion_events_enabled == 0xffff ? 0x0a95 : this->deassertion_events_enabled; };

	/**
	 * This structure holds the raw threshold values to be used for automatic
	 * threshold comparisons for sensors with Type 02 SDRs (which do not contain
	 * threshold data).  If a Type01 SDR is seen, this data will be updated from
	 * it on each update.
	 */
	struct threshold_configuration {
		uint8_t lnc;
		uint8_t lcr;
		uint8_t lnr;
		uint8_t unc;
		uint8_t ucr;
		uint8_t unr;
	} thresholds;

	/**
	 * Update thresholds in local cache from the SDR if present.
	 * @param sdr01 The sdr to use.  It must be a type 01 SDR or nullptr for NOOP.
	 */
	void updateThresholdsFromSdr(std::shared_ptr<const SensorDataRecord01> sdr01);

	/**
	 * Update the internal state of this sensor with the provided float value and
	 * generate any relevant events based on this.
	 *
	 * If the provided value is NAN, the sensor is presumed to be out of context,
	 * there is considered to be no reading available, and event processing is
	 * suspended.
	 *
	 * @param value The new sensor value.
	 * @param in_context True if this sensor is in context for this update. (Out of
	 *                   context sensors will not send or process events.)
	 * @param value_max_age The number of ticks after which the value should be
	 *                      considered to be out of date and should be replaced with
	 *                      NAN.
	 * @param extra_assertions Send these event assertions regardless of the (non-NaN) value.
	 * @param extra_deassertions Send these event deassertions regardless of the (non-NaN) value.
	 * @return A list of the event data sent with any generated IPMI messages (even if not ultimately put on an IPMB).
	 *
	 * @note For extra_(de)assertions, the following bitmask is interpreted:
	 *       bit 11: 1b = upper non-recoverable going high occurred
	 *       bit 10: 1b = upper non-recoverable going low occurred
	 *       bit  9: 1b = upper critical going high occurred
	 *       bit  8: 1b = upper critical going low occurred
	 *       bit  7: 1b = upper non-critical going high occurred
	 *       bit  6: 1b = upper non-critical going low occurred
	 *       bit  5: 1b = lower non-recoverable going high occurred
	 *       bit  4: 1b = lower non-recoverable going low occurred
	 *       bit  3: 1b = lower critical going high occurred
	 *       bit  2: 1b = lower critical going low occurred
	 *       bit  1: 1b = lower non-critical going high occurred
	 *       bit  0: 1b = lower non-critical going low occurred
	 */
	std::vector<std::vector<uint8_t>> updateValue(const float value, uint16_t event_context=0xffff, uint64_t value_max_age=UINT64_MAX, uint16_t extra_assertions=0, uint16_t extra_deassertions=0);

	/// Deny implicit conversions here.  We don't want to allow accidental submission of unconverted ADC Values.
	///@{
	void updateValue(const uint32_t value, uint16_t event_context=0x7fff, uint64_t value_max_age=UINT64_MAX, uint16_t extra_assertions=0, uint16_t extra_deassertions=0) = delete;
	void updateValue(const uint16_t value, uint16_t event_context=0x7fff, uint64_t value_max_age=UINT64_MAX, uint16_t extra_assertions=0, uint16_t extra_deassertions=0) = delete;
	void updateValue(const uint8_t value, uint16_t event_context=0x7fff, uint64_t value_max_age=UINT64_MAX, uint16_t extra_assertions=0, uint16_t extra_deassertions=0) = delete;
	void updateValue(const int value, uint16_t event_context=0x7fff, uint64_t value_max_age=UINT64_MAX, uint16_t extra_assertions=0, uint16_t extra_deassertions=0) = delete;
	///@}

	/**
	 * Override the auto-calculated nominal event status mask.
	 *
	 * This value is used to initialize event bits that come into context or are
	 * rearmed while it is set.
	 *
	 * @param mask The new mask to set, or 0xFFFF to clear the override.
	 */
	void setNominalEventStatusOverride(uint16_t mask);

	/**
	 * Retrieve the override value for the nominal event status mask.
	 *
	 * This value is used to initialize event bits that come into context or are
	 * rearmed while it is set.
	 *
	 * @return The override mask, or 0xFFFF if disabled.
	 */
	uint16_t getNominalEventStatusOverride();

	//! A threshold sensor current value state.
	typedef struct {
		float float_value;				///< The raw float value, NAN if no value is available.
		uint8_t byte_value;				///< The IPMI byte value, 0xFF if no SDR is available (but 0xFF is a valid value).
		uint16_t active_events;			///< The currently active events.
		uint16_t event_context;			///< A mask of events that are in context and being processed as of last reading.
		uint16_t enabled_assertions;	///< The event assertions (enabled & supported) at last update.
		uint16_t enabled_deassertions;	///< The event deassertions (enabled & supported) at last update.
	} Value;

	/**
	 * Get the current values and thresholds for this sensor.
	 *
	 * @note byte_value may be 0xFF if there is no SDR available to interpret it,
	 *       but this is also a valid value.  If no value is available, float_value
	 *       will be NAN.
	 *
	 * @return The current values & thresholds.
	 */
	Value getValue() const;

	virtual std::vector<uint8_t> getSensorReading();
	virtual uint16_t getSensorEventStatus(bool *reading_good = nullptr);
	virtual void rearm();

protected:
	float last_value;					///< The last value stored.
	uint64_t value_expiration;			///< The timestamp at which the value is no longer valid.
	uint16_t active_events;				///< Currently active events
	uint16_t event_context;				///< A mask of events that are in context and can be active.
	uint16_t last_enabled_assertions;	///< A mask of event assertions that were (enabled & supported) at last update.
	uint16_t last_enabled_deassertions;	///< A mask of event deassertions that were (enabled & supported) at last update.
	uint16_t nominal_event_status_override_value;	///< An overridden "nominal event mask".  Valid range: 0x000-0xFFF.  0xFFFF = disabled.
	SemaphoreHandle_t value_mutex;		///< A mutex protecting value fields.
};

#endif /* SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_SENSOR_THRESHOLD_SENSOR_H_ */
