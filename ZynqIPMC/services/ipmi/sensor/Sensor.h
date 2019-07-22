/*
 * Sensor.h
 *
 *  Created on: Sep 28, 2018
 *      Author: jtikalsky
 */

#ifndef SRC_COMMON_UW_IPMC_SERVICES_IPMI_SENSOR_SENSOR_H_
#define SRC_COMMON_UW_IPMC_SERVICES_IPMI_SENSOR_SENSOR_H_

#include <libs/logtree.h>
#include <vector>
#include <stdint.h>

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
	/// Return the current event status in Get Sensor Event Status form (Platform Event format).
	virtual uint16_t get_sensor_event_status(bool *reading_good=NULL) = 0;
	/**
	 * Rearm the sensor, clearing any existing reading (according to the spec)
	 * and ensuring that any IPMI events will be resent.
	 */
	virtual void rearm() = 0;

	/// IPMI sensor runtime configuration.
	///@{
	virtual void all_events_disabled(bool disabled) { this->_all_events_disabled = disabled; };
	virtual bool all_events_disabled() { return this->_all_events_disabled; };
	virtual void sensor_scanning_disabled(bool disabled) { this->_sensor_scanning_disabled = disabled; };
	virtual bool sensor_scanning_disabled() { return this->_sensor_scanning_disabled; };
	virtual void assertion_events_enabled(uint16_t events) { this->_assertion_events_enabled = events & 0x7fff; };
	virtual uint16_t assertion_events_enabled() { return this->_assertion_events_enabled & 0x7fff; };
	virtual void deassertion_events_enabled(uint16_t events) { this->_deassertion_events_enabled = events & 0x7fff; };
	virtual uint16_t deassertion_events_enabled() { return this->_deassertion_events_enabled & 0x7fff; };
	///@}

	std::string sensor_identifier(bool name_only=false) const;

protected:
	bool _all_events_disabled; ///< All IPMI events from this sensor are disabled.
	bool _sensor_scanning_disabled; ///< Sensor scanning is disabled for this sensor.
	uint16_t _assertion_events_enabled; ///< Assertion events enabled. (max 0x0fff, but 0xffff on init to distinguish unset)
	uint16_t _deassertion_events_enabled; ///< Deassertion events enabled. (max 0x0fff, but 0xffff on init to distinguish unset)
};

#endif /* SRC_COMMON_UW_IPMC_SERVICES_IPMI_SENSOR_SENSOR_H_ */
