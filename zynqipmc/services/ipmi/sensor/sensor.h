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

#ifndef SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_SENSOR_SENSOR_H_
#define SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_SENSOR_SENSOR_H_

#include <libs/logtree/logtree.h>
#include <vector>
#include <stdint.h>

/**
 * A general Sensor class.
 */
class Sensor {
public:
	/**
	 * Instantiate a Sensor.
	 *
	 * @param sdr_key The SDR key bytes identifying this sensor in the Device SDR Repository
	 * @param log The LogTree for messages from this class.
	 */
	Sensor(const std::vector<uint8_t> &sdr_key, LogTree &log);
	virtual ~Sensor();

	//! Sensor Event Direction
	enum EventDirection {
		EVENT_ASSERTION   = 0,
		EVENT_DEASSERTION = 1
	};

	/**
	 * Send an IPMI event for this sensor.
	 *
	 * @param direction The event direction (assertion vs deassertion).
	 * @param event_data The event data values (see IPMI2 spec).
	 */
	void sendEvent(enum EventDirection direction, const std::vector<uint8_t> &event_data);

	//! Return the correct IPMI response data (including completion code) for Get Sensor Reading.
	virtual std::vector<uint8_t> getSensorReading() = 0;

	//! Return the current event status in Get Sensor Event Status form (Platform Event format).
	virtual uint16_t getSensorEventStatus(bool *reading_good = nullptr) = 0;

	/**
	 * Rearm the sensor, clearing any existing reading (according to the spec)
	 * and ensuring that any IPMI events will be resent.
	 */
	virtual void rearm() = 0;

	/// IPMI sensor runtime configuration.
	///@{
	virtual void setAllEventsDisabled(bool disabled) { this->all_events_disabled = disabled; };
	virtual bool getAllEventsDisabled() { return this->all_events_disabled; };
	virtual void setSensorScanningDisabled(bool disabled) { this->sensor_scanning_disabled = disabled; };
	virtual bool getSensorScanningDisabled() { return this->sensor_scanning_disabled; };
	virtual void setAssertionEventsEnabled(uint16_t events) { this->assertion_events_enabled = events & 0x7fff; };
	virtual uint16_t getAssertionEventsEnabled() { return this->assertion_events_enabled & 0x7fff; };
	virtual void setDeassertionEventsEnabled(uint16_t events) { this->deassertion_events_enabled = events & 0x7fff; };
	virtual uint16_t getDeassertionEventsEnabled() { return this->deassertion_events_enabled & 0x7fff; };
	///@}

	/**
	 * Generates a human-readable sensor description suitable for identifying the
	 * sensor in logs.  It includes the SDR Key, and if the SDR can be located, the
	 * sensor name stored in it.
	 *
	 * @param name_only Return only the sensor name, not a full ID string including key bytes.
	 * @return A human-readable unique identifer for this sensor.
	 */
	std::string sensorIdentifier(bool name_only = false) const;

	//! Returns the SDR key of this sensor.
	inline const std::vector<uint8_t>& getSdrKey() const { return this->kSdrKey; };

	//! Returns the log associated with this sensor.
	inline LogTree& getLog() { return this->log; };

protected:
	const std::vector<uint8_t> kSdrKey; ///< SDR Key Bytes for this Sensor in the Device SDR Repository
	LogTree &log; ///< The log facility for this sensor.
	mutable LogRepeatSuppressor logunique; ///< A unique checker for our log messages.

	bool all_events_disabled; ///< All IPMI events from this sensor are disabled.
	bool sensor_scanning_disabled; ///< Sensor scanning is disabled for this sensor.
	uint16_t assertion_events_enabled; ///< Assertion events enabled. (max 0x0fff, but 0xffff on init to distinguish unset)
	uint16_t deassertion_events_enabled; ///< Deassertion events enabled. (max 0x0fff, but 0xffff on init to distinguish unset)
};

#endif /* SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_SENSOR_SENSOR_H_ */
