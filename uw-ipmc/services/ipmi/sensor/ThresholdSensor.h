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

	virtual void update_value(const float value);

	/// A threshold sensor current value state.
	typedef struct {
		float float_value; ///< The raw float value, NAN if no value is available.
		uint8_t byte_value; ///< The IPMI byte value, 0xFF if no SDR is available (but 0xFF is a valid value).
		uint16_t active_thresholds; ///< The currentyl active thresholds in IPMI "Get Sensor Reading" order.
	} Value;

	virtual Value get_value() const;
	virtual std::vector<uint8_t> get_sensor_reading();
	virtual void rearm();

protected:
	uint16_t active_thresholds; ///< Currently active thresholds.
	float last_value; ///< The last value stored.
	SemaphoreHandle_t value_mutex; ///< A mutex protecting value fields.
};

#endif /* SRC_COMMON_UW_IPMC_SERVICES_IPMI_SENSOR_THRESHOLDSENSOR_H_ */
