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

#ifndef SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_SENSOR_SEVERITY_SENSOR_H_
#define SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_SENSOR_SEVERITY_SENSOR_H_

#include <services/ipmi/sensor/sensor.h>

/**
 * A Severity Sensor
 */
class SeveritySensor final : public Sensor {
public:
	//! Instantiate the SeveritySensor
	SeveritySensor(const std::vector<uint8_t> &sdr_key, LogTree &log);
	virtual ~SeveritySensor();

	enum Level {
		OK      = 0,
		NC      = 1,
		CR      = 2,
		NR      = 3,
		MONITOR = 7,
		INFO    = 8,
	};

	//! IPMI2 Table 42-2, Generic Event/Reading Type Codes
	enum StateTransition {
		TRANS_OK           = 0, //!< transition to OK
		TRANS_NC_FROM_OK   = 1, //!< transition to Non-Critical from OK
		TRANS_CR_FROM_LESS = 2, //!< transition to Critical from less severe
		TRANS_NR_FROM_LESS = 3, //!< transition to Non-recoverable from less severe
		TRANS_NC_FROM_MORE = 4, //!< transition to Non-Critical from more severe
		TRANS_CR_FROM_NR   = 5, //!< transition to Critical from Non-recoverable
		TRANS_NR           = 6, //!< transition to Non-recoverable
		TRANS_MONITOR      = 7, //!< Monitor
		TRANS_INFO         = 8, //!< Informational
	};

	static const char *StateTransitionLabels[9];

	/**
	 * Update the current severity level and send the appropriate events.
	 * @param level The new severity level
	 * @param send_event If true, send the appropriate IPMI event.
	 */
	void transition(enum Level level, bool send_event=true);

	virtual std::vector<uint8_t> getSensorReading();
	virtual uint16_t getSensorEventStatus(bool *reading_good=nullptr);
	virtual void rearm();

	//! Return the current severity status.
	enum Level getRawSeverityLevel() const;
	enum StateTransition getSensorValue() const;

protected:
	enum StateTransition status; ///< The sensor state
};

#endif /* SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_SENSOR_SEVERITY_SENSOR_H_ */
