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
	Sensor(const std::vector<uint8_t> &sdr_key, LogTree &log);
	virtual ~Sensor();

	const std::vector<uint8_t> sdr_key; ///< SDR Key Bytes for this Sensor in the Device SDR Repository
	LogTree &log; ///< The log facility for this sensor.
	mutable LogRepeatSuppressor logunique; ///< A unique checker for our log messages.

	/// Sensor Event Direction
	enum EventDirection {
		EVENT_ASSERTION   = 0,
		EVENT_DEASSERTION = 1
	};
	void send_event(enum EventDirection direction, const std::vector<uint8_t> &event_data);
};

#endif /* SRC_COMMON_UW_IPMC_SERVICES_IPMI_SENSOR_SENSOR_H_ */
