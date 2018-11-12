/*
 * Sensor.h
 *
 *  Created on: Sep 28, 2018
 *      Author: jtikalsky
 */

#ifndef SRC_COMMON_UW_IPMC_SERVICES_IPMI_SENSOR_SENSOR_H_
#define SRC_COMMON_UW_IPMC_SERVICES_IPMI_SENSOR_SENSOR_H_

#include <vector>
#include <stdint.h>
#include <libs/LogTree.h>

/**
 * A general Sensor class.
 */
class Sensor {
public:
	virtual ~Sensor();
	Sensor(const std::vector<uint8_t> &sdr_key, LogTree &log);

	const std::vector<uint8_t> sdr_key; ///< SDR Key Bytes for this Sensor in the Device SDR Repository
	LogTree &log; ///< The log facility for this sensor.
	mutable LogRepeatSuppressor logunique; ///< A unique checker for our log messages.

	/// Sensor Event Direction
	enum EventDirection {
		EVENT_ASSERTION   = 0,
		EVENT_DEASSERTION = 1
	};
	virtual void send_event(enum EventDirection direction, const std::vector<uint8_t> &event_data);
	/// Return the correct IPMI response data (including completion code) for Get Sensor Reading.
	virtual std::vector<uint8_t> get_sensor_reading() = 0;
	/**
	 * Rearm the sensor, clearing any existing reading (according to the spec)
	 * and ensuring that any IPMI events will be resent.
	 */
	virtual void rearm() = 0;

	/// IPMI sensor runtime configuration.
	///@{
	void all_events_disabled(bool disabled) { this->_all_events_disabled = disabled; };
	bool all_events_disabled() { return this->_all_events_disabled; };
	void sensor_scanning_disabled(bool disabled) { this->_sensor_scanning_disabled = disabled; };
	bool sensor_scanning_disabled() { return this->_sensor_scanning_disabled; };
	///@}

	std::string sensor_identifier() const;
protected:
	bool _all_events_disabled; ///< All IPMI events from this sensor are disabled.
	bool _sensor_scanning_disabled; ///< Sensor scanning is disabled for this sensor.
};

#endif /* SRC_COMMON_UW_IPMC_SERVICES_IPMI_SENSOR_SENSOR_H_ */
