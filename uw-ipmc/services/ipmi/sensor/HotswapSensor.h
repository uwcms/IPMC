/*
 * HotswapSensor.h
 *
 *  Created on: Oct 30, 2018
 *      Author: jtikalsky
 */

#ifndef SRC_COMMON_UW_IPMC_SERVICES_IPMI_SENSOR_HOTSWAPSENSOR_H_
#define SRC_COMMON_UW_IPMC_SERVICES_IPMI_SENSOR_HOTSWAPSENSOR_H_

#include <services/ipmi/sensor/Sensor.h>

/**
 * A PICMG 3.0 Hot Swap Sensor
 */
class HotswapSensor: public Sensor {
public:
	/// Instantiate the HotswapSensor
	HotswapSensor(const std::vector<uint8_t> &sdr_key, LogTree &log);
	virtual ~HotswapSensor();

	/// PICMG State Transition Reasons (PICMG 3.0 Table 3-23)
	enum StateTransitionReason {
		TRANS_NORMAL                                  = 0,
		TRANS_COMMANDED_BY_SHELF                      = 1,
		TRANS_OPERATOR_SWITCH                         = 2,
		TRANS_FRU_PROGRAMATIC                         = 3,
		TRANS_COMMUNICATION_LOST_OR_REGAINED          = 4,
		TRANS_COMMUNICATION_LOST_OR_REGAINED_INTERNAL = 5,
		TRANS_SURPRISE_EXTRACTION                     = 6,
		TRANS_PROVIDED_INFORMATION                    = 7,
		TRANS_INVALID_HW_ADDRESS                      = 8,
		TRANS_UNEXPECTED_DEACTIVATION                 = 9,
		TRANS_SURPRISE_POWER_FAIL                     = 10,
		TRANS_REASON_UNKNOWN                          = 0xF
	};
	virtual void transition(uint8_t new_state, enum StateTransitionReason reason);
	virtual std::vector<uint8_t> get_sensor_reading();
	virtual void rearm();

	/// Return the current M-State
	virtual uint8_t get_mstate() { return this->mstate; };

protected:
	uint8_t mstate; ///< The current M-State
	uint8_t previous_mstate; ///< The previous M-State
	enum StateTransitionReason last_transition_reason; ///< The last transition reason.
	SemaphoreHandle_t mutex; ///< A mutex to protect internal state.
};

#endif /* SRC_COMMON_UW_IPMC_SERVICES_IPMI_SENSOR_HOTSWAPSENSOR_H_ */
