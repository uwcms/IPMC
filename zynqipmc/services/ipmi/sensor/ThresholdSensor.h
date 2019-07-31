/*
 * ThresholdSensor.h
 *
 *  Created on: Oct 19, 2018
 *      Author: jtikalsky
 */

#ifndef SRC_COMMON_UW_IPMC_SERVICES_IPMI_SENSOR_THRESHOLDSENSOR_H_
#define SRC_COMMON_UW_IPMC_SERVICES_IPMI_SENSOR_THRESHOLDSENSOR_H_

#include <services/ipmi/sdr/sensor_data_record_01.h>
#include <services/ipmi/sensor/Sensor.h>

/**
 * A standard threshold sensor.
 */
class ThresholdSensor : public Sensor {
public:
	/**
	 * Instantiate a standard threshold sensor.
	 * @param sdr_key The SDR key bytes to retrieve a matching SDR from the device_sdr_repo.
	 * @param log A LogTree for this object.
	 * @param initialize_on_first_update If true, the first update_value() call will initialize the sensor only and not send event messages.
	 */
	ThresholdSensor(const std::vector<uint8_t> &sdr_key, LogTree &log);
	virtual ~ThresholdSensor();

	// If these have never been set (i.e. are 0xffff), for THRESHOLD sensors, we'll default to only Upper going-high and Lower going-low enabled.
	virtual uint16_t assertion_events_enabled() { return this->_assertion_events_enabled == 0xffff ? 0x0a95 : this->_assertion_events_enabled; };
	virtual uint16_t deassertion_events_enabled() { return this->_deassertion_events_enabled == 0xffff ? 0x0a95 : this->_deassertion_events_enabled; };


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

	void update_thresholds_from_sdr(std::shared_ptr<const SensorDataRecord01> sdr01);

	virtual void update_value(const float value, uint16_t event_context=0xffff, uint64_t value_max_age=UINT64_MAX, uint16_t extra_assertions=0, uint16_t extra_deassertions=0);

	/// Deny implicit conversions here.  We don't want to allow accidental submission of unconverted ADC Values.
	///@{
	virtual void update_value(const uint32_t value, uint16_t event_context=0x7fff, uint64_t value_max_age=UINT64_MAX, uint16_t extra_assertions=0, uint16_t extra_deassertions=0) = delete;
	virtual void update_value(const uint16_t value, uint16_t event_context=0x7fff, uint64_t value_max_age=UINT64_MAX, uint16_t extra_assertions=0, uint16_t extra_deassertions=0) = delete;
	virtual void update_value(const uint8_t value, uint16_t event_context=0x7fff, uint64_t value_max_age=UINT64_MAX, uint16_t extra_assertions=0, uint16_t extra_deassertions=0) = delete;
	virtual void update_value(const int value, uint16_t event_context=0x7fff, uint64_t value_max_age=UINT64_MAX, uint16_t extra_assertions=0, uint16_t extra_deassertions=0) = delete;
	///@}

	virtual void nominal_event_status_override(uint16_t mask);
	virtual uint16_t nominal_event_status_override();

	/// A threshold sensor current value state.
	typedef struct {
		float float_value; ///< The raw float value, NAN if no value is available.
		uint8_t byte_value; ///< The IPMI byte value, 0xFF if no SDR is available (but 0xFF is a valid value).
		uint16_t active_events; ///< The currently active events.
		uint16_t event_context; ///< A mask of events that are in context and being processed as of last reading.
		uint16_t enabled_assertions; ///< The event assertions (enabled & supported) at last update.
		uint16_t enabled_deassertions; ///< The event deassertions (enabled & supported) at last update.
	} Value;

	virtual Value get_value() const;
	virtual std::vector<uint8_t> get_sensor_reading();
	virtual uint16_t get_sensor_event_status(bool *reading_good=NULL);
	virtual void rearm();


protected:
	float last_value; ///< The last value stored.
	uint64_t value_expiration; ///< The timestamp at which the value is no longer valid.
	uint16_t active_events; ///< Currently active events
	uint16_t event_context; ///< A mask of events that are in context and can be active.
	uint16_t last_enabled_assertions; ///< A mask of event assertions that were (enabled & supported) at last update.
	uint16_t last_enabled_deassertions; ///< A mask of event deassertions that were (enabled & supported) at last update.
	uint16_t nominal_event_status_override_value; ///< An overridden "nominal event mask".  Valid range: 0x000-0xFFF.  0xFFFF = disabled.
	SemaphoreHandle_t value_mutex; ///< A mutex protecting value fields.
};

#endif /* SRC_COMMON_UW_IPMC_SERVICES_IPMI_SENSOR_THRESHOLDSENSOR_H_ */
