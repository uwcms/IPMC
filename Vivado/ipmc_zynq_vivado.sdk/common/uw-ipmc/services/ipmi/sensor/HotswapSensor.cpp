/*
 * HotswapSensor.cpp
 *
 *  Created on: Oct 30, 2018
 *      Author: jtikalsky
 */

#include <services/ipmi/sensor/HotswapSensor.h>

/**
 * Update the current M-state and send the appropriate events.
 * @param new_state The new M-state
 * @param reason The reason for the state transition
 */
void HotswapSensor::transition(uint8_t new_state, enum StateTransitionReason reason) {
	configASSERT(new_state < 8); // Check in range.
	std::vector<uint8_t> data;
	data.push_back(0xA|new_state);
	data.push_back((static_cast<uint8_t>(reason)<<4)|this->mstate);
	data.push_back(0 /* FRU Device ID */);
	this->mstate = new_state;
	this->send_event(Sensor::EVENT_ASSERTION, data);
}

std::vector<uint8_t> HotswapSensor::get_sensor_reading() {
	std::vector<uint8_t> out{0/*IPMI::Completion::Success*/,0,0,0};
	if (this->all_events_disabled())
		out[2] |= 0x80;
	if (this->sensor_scanning_disabled())
		out[2] |= 0x40;
	out[3] = this->mstate;
	return out;
}
