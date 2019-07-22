/*
 * Sensor.cpp
 *
 *  Created on: Sep 28, 2018
 *      Author: jtikalsky
 */

#include <services/ipmi/sensor/Sensor.h>
#include <services/ipmi/sdr/SensorDataRepository.h>
#include <services/ipmi/sdr/SensorDataRecordSensor.h>
#include <services/ipmi/sdr/SensorDataRecordReadableSensor.h>
#include <services/ipmi/sdr/SensorDataRecord01.h>
#include <services/ipmi/ipmbsvc/IPMBSvc.h>
#include <libs/ThreadingPrimitives.h>
#include <libs/printf.h>
#include <libs/except.h>
#include <IPMC.h>
#include <Core.h>
#include <math.h>

/**
 * Instantiate a Sensor.
 *
 * @param sdr_key The SDR key bytes identifying this sensor in the Device SDR Repository
 * @param log The LogTree for messages from this class.
 */
Sensor::Sensor(const std::vector<uint8_t> &sdr_key, LogTree &log)
	: sdr_key(sdr_key), log(log), logunique(log, pdMS_TO_TICKS(10000)),
	  _all_events_disabled(false), _sensor_scanning_disabled(false),
	  _assertion_events_enabled(0xffff), _deassertion_events_enabled(0xffff) {
	if (sdr_key.size() != 3)
		throw std::domain_error("A Sensor SDR key must be 3 bytes.");
}

Sensor::~Sensor() {
}

/**
 * Send an IPMI event for this sensor.
 *
 * @param direction The event direction (assertion vs deassertion)
 * @param event_data The event data values (see IPMI2 spec)
 */
void Sensor::send_event(enum EventDirection direction, const std::vector<uint8_t> &event_data) {
	std::shared_ptr<const SensorDataRecordSensor> sdr = std::dynamic_pointer_cast<const SensorDataRecordSensor>(device_sdr_repo.find(this->sdr_key));
	if (!sdr) {
		this->logunique.logUnique(stdsprintf("Unable to locate sensor %s in the Device SDR Repository!  Event not transmitted!", this->sensor_identifier().c_str()), LogTree::LOG_ERROR);
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
	if (this->all_events_disabled()) {
			this->log.log(stdsprintf("Discarding event on \"%s\" sensor: all events disabled on this sensor", sdr->id_string().c_str()), LogTree::LOG_INFO);
			return;
	}

	std::vector<uint8_t> data;
	data.push_back(0x04); // Revision 04h, specified.
	data.push_back(sdr->sensor_type_code());
	data.push_back(sdr->sensor_number());
	data.push_back((static_cast<uint8_t>(direction) << 7) | sdr->event_type_reading_code());
	data.insert(data.end(), event_data.begin(), event_data.end());
	std::shared_ptr<IPMI_MSG> msg = std::make_shared<IPMI_MSG>(0, er.ipmb->ipmb_address, er.lun, er.addr, IPMI::NetFn::Sensor_Event, IPMI::Sensor_Event::Platform_Event, data);
	this->log.log(stdsprintf("Sending event on \"%s\" sensor to %hhu.%02hhx: %s", sdr->id_string().c_str(), er.lun, er.addr, msg->format().c_str()), LogTree::LOG_INFO);
	ipmb0->send(msg);
}

/**
 * Generates a human-readable sensor description suitable for identifying the
 * sensor in logs.  It includes the SDR Key, and if the SDR can be located, the
 * sensor name stored in it.
 *
 * @param name_only Return only the sensor name, not a full ID string including key bytes.
 * @return A human-readable unique identifer for this sensor
 */
std::string Sensor::sensor_identifier(bool name_only) const {
	std::shared_ptr<const SensorDataRecordSensor> sdr = std::dynamic_pointer_cast<const SensorDataRecordSensor>(device_sdr_repo.find(this->sdr_key));
	if (sdr) {
		if (name_only)
			return sdr->id_string();
		else
			return stdsprintf("%02hhx%02hhx%02hhx (%s)", this->sdr_key[0], this->sdr_key[1], this->sdr_key[2], sdr->id_string().c_str());
	}
	else {
		if (name_only)
			return "<No SDR Located>";
		else
			return stdsprintf("%02hhx%02hhx%02hhx (<No SDR Located>)", this->sdr_key[0], this->sdr_key[1], this->sdr_key[2]);
	}
}
