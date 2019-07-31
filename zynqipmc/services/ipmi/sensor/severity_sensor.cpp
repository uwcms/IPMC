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
#include <services/ipmi/sensor/severity_sensor.h>

SeveritySensor::SeveritySensor(const std::vector<uint8_t> &sdr_key, LogTree &log)
	: Sensor(sdr_key, log), status(TRANS_OK) {
}

SeveritySensor::~SeveritySensor() {
}

void SeveritySensor::transition(enum Level level, bool send_event) {
	enum StateTransition old_status = this->status;
	enum StateTransition new_status;

	/* enum StateTransition {
		TRANS_OK           = 0, //!< transition to OK
		TRANS_NC_FROM_OK   = 1, //!< transition to Non-Critical from OK
		TRANS_CR_FROM_LESS = 2, //!< transition to Critical from less severe
		TRANS_NR_FROM_LESS = 3, //!< transition to Non-recoverable from less severe
		TRANS_NC_FROM_MORE = 4, //!< transition to Non-Critical from more severe
		TRANS_CR_FROM_NR   = 5, //!< transition to Critical from Non-recoverable
		TRANS_NR           = 6, //!< transition to Non-recoverable
		TRANS_MONITOR      = 7, //!< Monitor
		TRANS_INFO         = 8, //!< Informational
	}; */
	switch (level) {
	case OK:
		if (old_status == TRANS_OK)
			send_event = false;
		new_status = TRANS_OK;
		break;
	case NC:
		switch (old_status) {
		case TRANS_OK:           new_status = TRANS_NC_FROM_OK; break;
		case TRANS_NC_FROM_OK:   new_status = TRANS_NC_FROM_OK;   send_event = false; break;
		case TRANS_NC_FROM_MORE: new_status = TRANS_NC_FROM_MORE; send_event = false; break;
		case TRANS_CR_FROM_LESS: new_status = TRANS_NC_FROM_MORE; break;
		case TRANS_CR_FROM_NR:   new_status = TRANS_NC_FROM_MORE; break;
		case TRANS_NR_FROM_LESS: new_status = TRANS_NC_FROM_MORE; break;
		case TRANS_NR:           new_status = TRANS_NC_FROM_MORE; break;
		default:                 new_status = TRANS_NC_FROM_OK; break;
		}
		break;
	case CR:
		switch (old_status) {
		case TRANS_OK:           new_status = TRANS_CR_FROM_LESS; break;
		case TRANS_NC_FROM_OK:   new_status = TRANS_CR_FROM_LESS; break;
		case TRANS_NC_FROM_MORE: new_status = TRANS_CR_FROM_LESS; break;
		case TRANS_CR_FROM_LESS: new_status = TRANS_CR_FROM_LESS; send_event = false; break;
		case TRANS_CR_FROM_NR:   new_status = TRANS_CR_FROM_NR;   send_event = false; break;
		case TRANS_NR_FROM_LESS: new_status = TRANS_CR_FROM_NR; break;
		case TRANS_NR:           new_status = TRANS_CR_FROM_NR; break;
		default:                 new_status = TRANS_CR_FROM_LESS; break;
		}
		break;
	case NR:
		switch (old_status) {
		case TRANS_OK:           new_status = TRANS_NR_FROM_LESS; break;
		case TRANS_NC_FROM_OK:   new_status = TRANS_NR_FROM_LESS; break;
		case TRANS_NC_FROM_MORE: new_status = TRANS_NR_FROM_LESS; break;
		case TRANS_CR_FROM_LESS: new_status = TRANS_NR_FROM_LESS; break;
		case TRANS_CR_FROM_NR:   new_status = TRANS_NR_FROM_LESS; break;
		case TRANS_NR_FROM_LESS: new_status = TRANS_NR_FROM_LESS; send_event = false; break;
		case TRANS_NR:           new_status = TRANS_NR;           send_event = false; break;
		default:                 new_status = TRANS_NR; break;
		}
		break;
	case MONITOR:
		if (old_status == TRANS_MONITOR)
			send_event = false;
		new_status = TRANS_MONITOR; break;
	case INFO:
		if (old_status == TRANS_INFO)
			send_event = false;
		new_status = TRANS_INFO; break;
	default:
		throw std::domain_error("Invalid state supplied.");
	}

	this->status = new_status;

	std::vector<uint8_t> data;
	data.push_back(0x40|static_cast<uint8_t>(new_status));
	data.push_back(static_cast<uint8_t>(old_status));
	data.push_back(0 /* Unspecified */);
	if (send_event)
		this->sendEvent(Sensor::EVENT_ASSERTION, data);
}

std::vector<uint8_t> SeveritySensor::getSensorReading() {
	std::vector<uint8_t> out{0/*IPMI::Completion::Success*/,0,0,0,0};
	if (!this->getAllEventsDisabled())
		out[2] |= 0x80;
	if (!this->getSensorScanningDisabled())
		out[2] |= 0x40;
	uint16_t event_state = 1 << static_cast<uint8_t>(this->status);
	out[3] = event_state & 0xff;
	out[4] = event_state >> 8;
	return out;
}

uint16_t SeveritySensor::getSensorEventStatus(bool *reading_good) {
	if (reading_good)
		*reading_good = true;
	return (1 << static_cast<uint8_t>(this->status));
}

void SeveritySensor::rearm() {
	std::vector<uint8_t> data;
	data.push_back(0x00|static_cast<uint8_t>(this->status));
	this->sendEvent(Sensor::EVENT_ASSERTION, data);
}

enum SeveritySensor::Level SeveritySensor::getRawSeverityLevel() const {
	switch (this->status) {
	case TRANS_OK:           return OK; break;
	case TRANS_NC_FROM_OK:   return NC; break;
	case TRANS_NC_FROM_MORE: return NC; break;
	case TRANS_CR_FROM_LESS: return CR; break;
	case TRANS_CR_FROM_NR:   return CR; break;
	case TRANS_NR_FROM_LESS: return NR; break;
	case TRANS_NR:           return NR; break;
	case TRANS_MONITOR:      return MONITOR; break;
	case TRANS_INFO:         return INFO; break;
	default:                 return NR; break;
	}
}

enum SeveritySensor::StateTransition SeveritySensor::getSensorValue() const {
	return this->status;
}

const char *SeveritySensor::StateTransitionLabels[9] = {
	"transition to OK",
	"transition to Non-Critical from OK",
	"transition to Critical from less severe",
	"transition to Non-recoverable from less severe",
	"transition to Non-Critical from more severe",
	"transition to Critical from Non-recoverable",
	"transition to Non-recoverable",
	"Monitor",
	"Informational",
};
