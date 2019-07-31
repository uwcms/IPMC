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

#include <core.h>
#include <libs/printf.h>
#include <libs/except.h>
#include <libs/threading.h>
#include <math.h>
#include <services/ipmi/ipmbsvc/ipmbsvc.h>
#include <services/ipmi/sdr/sensor_data_record_01.h>
#include <services/ipmi/sdr/sensor_data_record_readable_sensor.h>
#include <services/ipmi/sdr/sensor_data_record_sensor.h>
#include <services/ipmi/sdr/sensor_data_repository.h>
#include <services/ipmi/sensor/sensor.h>

Sensor::Sensor(const std::vector<uint8_t> &sdr_key, LogTree &log)
	: kSdrKey(sdr_key), log(log), logunique(log, pdMS_TO_TICKS(10000)),
	  all_events_disabled(false), sensor_scanning_disabled(false),
	  assertion_events_enabled(0xffff), deassertion_events_enabled(0xffff) {
	if (sdr_key.size() != 3) {
		throw std::domain_error("A Sensor SDR key must be 3 bytes.");
	}
}

Sensor::~Sensor() {
}

void Sensor::sendEvent(enum EventDirection direction, const std::vector<uint8_t> &event_data) {
	std::shared_ptr<const SensorDataRecordSensor> sdr = std::dynamic_pointer_cast<const SensorDataRecordSensor>(device_sdr_repo.find(this->kSdrKey));
	if (!sdr) {
		this->logunique.logUnique(stdsprintf("Unable to locate sensor %s in the Device SDR Repository!  Event not transmitted!", this->sensorIdentifier().c_str()), LogTree::LOG_ERROR);
		return;
	}

	IPMBSvc::EventReceiver er;
	CriticalGuard critical(true);
	memcpy(&er, &ipmi_event_receiver, sizeof(IPMBSvc::EventReceiver));
	critical.release();

	if (!ipmi_event_receiver.ipmb || ipmi_event_receiver.addr == 0xFF /* Disabled */) {
		this->logunique.logUnique(stdsprintf("There is not yet an IPMI Event Receiver.  Discarding events on sensor \"%s\".", sdr->id_string().c_str()), LogTree::LOG_DIAGNOSTIC);
		return;
	}

	if (this->getAllEventsDisabled()) {
		this->log.log(stdsprintf("Discarding event on \"%s\" sensor: all events disabled on this sensor", sdr->id_string().c_str()), LogTree::LOG_INFO);
		return;
	}

	std::vector<uint8_t> data;
	data.push_back(0x04); // Revision 04h, specified.
	data.push_back(sdr->sensor_type_code());
	data.push_back(sdr->sensor_number());
	data.push_back((static_cast<uint8_t>(direction) << 7) | sdr->event_type_reading_code());
	data.insert(data.end(), event_data.begin(), event_data.end());
	std::shared_ptr<IPMIMessage> msg = std::make_shared<IPMIMessage>(0, er.ipmb->getIPMBAddress(), er.lun, er.addr, IPMI::NetFn::Sensor_Event, IPMI::Sensor_Event::Platform_Event, data);
	this->log.log(stdsprintf("Sending event on \"%s\" sensor to %hhu.%02hhx: %s", sdr->id_string().c_str(), er.lun, er.addr, msg->format().c_str()), LogTree::LOG_INFO);
	ipmb0->send(msg);
}

std::string Sensor::sensorIdentifier(bool name_only) const {
	std::shared_ptr<const SensorDataRecordSensor> sdr = std::dynamic_pointer_cast<const SensorDataRecordSensor>(device_sdr_repo.find(this->kSdrKey));
	if (sdr) {
		if (name_only) {
			return sdr->id_string();
		} else {
			return stdsprintf("%02hhx%02hhx%02hhx (%s)", this->kSdrKey[0], this->kSdrKey[1], this->kSdrKey[2], sdr->id_string().c_str());
		}
	} else {
		if (name_only) {
			return "<No SDR Located>";
		} else {
			return stdsprintf("%02hhx%02hhx%02hhx (<No SDR Located>)", this->kSdrKey[0], this->kSdrKey[1], this->kSdrKey[2]);
		}
	}
}
