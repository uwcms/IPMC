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

#include <FreeRTOS.h>
#include <semphr.h>
#include <libs/printf.h>
#include <libs/except.h>
#include <libs/threading.h>
#include <services/ipmi/sensor/hotswap_sensor.h>

HotswapSensor::HotswapSensor(const std::vector<uint8_t> &sdr_key, LogTree &log)
	: Sensor(sdr_key, log), mstate(1), previous_mstate(0),
	  last_transition_reason(TRANS_NORMAL) {
	configASSERT(this->mutex = xSemaphoreCreateMutex());
}

HotswapSensor::~HotswapSensor() {
	vSemaphoreDelete(this->mutex);
}

void HotswapSensor::transition(uint8_t new_state, enum StateTransitionReason reason, bool send_event) {
	if (new_state >= 8) {
		throw std::domain_error(stdsprintf("Only M0-M7 are supported, not M%hhu.", new_state));
	}

	std::vector<uint8_t> data;
	MutexGuard<false> lock(this->mutex, true);
	data.push_back(0xA0|new_state);
	data.push_back((static_cast<uint8_t>(reason)<<4)|this->mstate);
	data.push_back(0 /* FRU Device ID */);
	this->previous_mstate = this->mstate;
	this->mstate = new_state;
	this->last_transition_reason = reason;
	lock.release();

	if (send_event) {
		this->sendEvent(Sensor::EVENT_ASSERTION, data);
	}
}

std::vector<uint8_t> HotswapSensor::getSensorReading() {
	std::vector<uint8_t> out{0/*IPMI::Completion::Success*/,0,0,0};
	if (!this->getAllEventsDisabled()) {
		out[2] |= 0x80;
	}

	if (!this->getSensorScanningDisabled()) {
		out[2] |= 0x40;
	}
	out[3] = this->mstate;
	return out;
}
uint16_t HotswapSensor::getSensorEventStatus(bool *reading_good) {
	if (reading_good) {
		*reading_good = true;
	}

	return (1 << this->mstate);
}

void HotswapSensor::rearm() {
	/* It is unclear what previous state or reason code I am expected to send in
	 * the event of a rearm of the hotswap sensor, but I presume we should
	 * resend the last event.
	 */
	std::vector<uint8_t> data;
	MutexGuard<false> lock(this->mutex, true);
	data.push_back(0xA0|this->mstate);
	data.push_back((static_cast<uint8_t>(this->last_transition_reason)<<4)|this->previous_mstate);
	data.push_back(0 /* FRU Device ID */);
	lock.release();
	this->sendEvent(Sensor::EVENT_ASSERTION, data);
}
