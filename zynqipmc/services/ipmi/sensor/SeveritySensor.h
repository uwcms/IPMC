#ifndef SRC_COMMON_UW_IPMC_SERVICES_IPMI_SENSOR_SEVERITYSENSOR_H_
#define SRC_COMMON_UW_IPMC_SERVICES_IPMI_SENSOR_SEVERITYSENSOR_H_

#include <services/ipmi/sensor/Sensor.h>

/**
 * A Severity Sensor
 */
class SeveritySensor: public Sensor {
public:
	/// Instantiate the SeveritySensor
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

	/// IPMI2 Table 42-2, Generic Event/Reading Type Codes
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

	virtual void transition(enum Level level, bool send_event=true);
	virtual std::vector<uint8_t> get_sensor_reading();
	virtual uint16_t get_sensor_event_status(bool *reading_good=NULL);
	virtual void rearm();

	/// Return the current severity status
	virtual enum Level get_raw_severity_level() const;
	virtual enum StateTransition get_sensor_value() const;

protected:
	enum StateTransition status; ///< The sensor state
};

#endif /* SRC_COMMON_UW_IPMC_SERVICES_IPMI_SENSOR_HOTSWAPSENSOR_H_ */
