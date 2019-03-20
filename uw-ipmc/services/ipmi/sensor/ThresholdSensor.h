/*
 * ThresholdSensor.h
 *
 *  Created on: Oct 19, 2018
 *      Author: jtikalsky
 */

#ifndef SRC_COMMON_UW_IPMC_SERVICES_IPMI_SENSOR_THRESHOLDSENSOR_H_
#define SRC_COMMON_UW_IPMC_SERVICES_IPMI_SENSOR_THRESHOLDSENSOR_H_

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
	 */
	ThresholdSensor(const std::vector<uint8_t> &sdr_key, LogTree &log);
	virtual ~ThresholdSensor();

	/**
	 * This structure holds the raw threshold values to be used for automatic
	 * threshold comparisons for sensors with Type 02 SDRs (which do not contain
	 * threshold data).  If a Type01 SDR is seen, this data will be updated from
	 * it on each update.
	 */
	struct {
		uint8_t lnc;
		uint8_t lcr;
		uint8_t lnr;
		uint8_t unc;
		uint8_t ucr;
		uint8_t unr;
	} thresholds;

	virtual void update_value(const float value, bool in_context=true, uint64_t value_max_age=UINT64_MAX, uint16_t force_assertions=0, uint16_t force_deassertions=0);

	/// Deny implicit conversions here.  We don't want to allow accidental submission of unconverted ADC Values.
	///@{
	virtual void update_value(const uint32_t value, bool in_context=true, uint64_t value_max_age=UINT64_MAX, uint16_t force_assertions=0, uint16_t force_deassertions=0) = delete;
	virtual void update_value(const uint16_t value, bool in_context=true, uint64_t value_max_age=UINT64_MAX, uint16_t force_assertions=0, uint16_t force_deassertions=0) = delete;
	virtual void update_value(const uint8_t value, bool in_context=true, uint64_t value_max_age=UINT64_MAX, uint16_t force_assertions=0, uint16_t force_deassertions=0) = delete;
	virtual void update_value(const int value, bool in_context=true, uint64_t value_max_age=UINT64_MAX, uint16_t force_assertions=0, uint16_t force_deassertions=0) = delete;
	///@}

	/// A threshold sensor current value state.
	typedef struct {
		float float_value; ///< The raw float value, NAN if no value is available.
		uint8_t byte_value; ///< The IPMI byte value, 0xFF if no SDR is available (but 0xFF is a valid value).
		uint16_t active_thresholds; ///< The currently active thresholds in IPMI "Platform Event" format.
		bool in_context; ///< True if the sensor was in context and processing events as of last reading.
	} Value;

	virtual Value get_value() const;
	virtual std::vector<uint8_t> get_sensor_reading();
	virtual void rearm();

protected:
	float last_value; ///< The last value stored.
	uint64_t value_expiration; ///< The timestamp at which the value is no longer valid.
	uint16_t active_thresholds; ///< Currently active thresholds in "Platform Event" format
	bool in_context; ///< True if the sensor is in context and should process events.
	SemaphoreHandle_t value_mutex; ///< A mutex protecting value fields.
};

#endif /* SRC_COMMON_UW_IPMC_SERVICES_IPMI_SENSOR_THRESHOLDSENSOR_H_ */
