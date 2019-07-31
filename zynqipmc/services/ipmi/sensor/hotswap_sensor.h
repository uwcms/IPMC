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

#ifndef SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_SENSOR_HOTSWAP_SENSOR_H_
#define SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_SENSOR_HOTSWAP_SENSOR_H_

#include <services/ipmi/sensor/sensor.h>

/**
 * A PICMG 3.0 Hot Swap Sensor
 */
class HotswapSensor final : public Sensor {
public:
	/// Instantiate the HotswapSensor
	HotswapSensor(const std::vector<uint8_t> &sdr_key, LogTree &log);
	virtual ~HotswapSensor();

	//! PICMG State Transition Reasons (PICMG 3.0 Table 3-23)
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

	/**
	 * Update the current M-state and send the appropriate events.
	 * @param new_state The new M-state
	 * @param reason The reason for the state transition
	 * @param send_event If true, send the appropriate IPMI event.
	 */
	void transition(uint8_t new_state, enum StateTransitionReason reason, bool send_event = true);

	virtual std::vector<uint8_t> getSensorReading();
	virtual uint16_t getSensorEventStatus(bool *reading_good=NULL);
	virtual void rearm();

	/// Return the current M-State
	uint8_t getMState() { return this->mstate; };

protected:
	uint8_t mstate;				///< The current M-State
	uint8_t previous_mstate;	///< The previous M-State
	enum StateTransitionReason last_transition_reason; ///< The last transition reason.
	SemaphoreHandle_t mutex;	///< A mutex to protect internal state.
};

#endif /* SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_SENSOR_HOTSWAP_SENSOR_H_ */
