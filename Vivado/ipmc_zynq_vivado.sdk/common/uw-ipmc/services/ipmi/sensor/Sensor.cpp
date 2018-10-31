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
#include <IPMC.h>
#include <math.h>

/**
 * Instantiate a Sensor.
 *
 * @param sdr_key The SDR key bytes identifying this sensor in the Device SDR Repository
 * @param log The LogTree for messages from this class.
 */
Sensor::Sensor(const std::vector<uint8_t> &sdr_key, LogTree &log)
	: sdr_key(sdr_key), log(log), logunique(log, pdMS_TO_TICKS(10000)),
	  _all_events_disabled(false), _sensor_scanning_disabled(false) {
	configASSERT(sdr_key.size() == 3);
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
		this->logunique.log_unique(stdsprintf("Unable to locate sensor %02hhx%02hhx%02hhx in the Device SDR Repository!  Event not transmitted!", this->sdr_key[0], this->sdr_key[1], this->sdr_key[2]), LogTree::LOG_ERROR);
		return;
	}

	portENTER_CRITICAL();
	EventReceiver er;
	memcpy(&er, &ipmi_event_receiver, sizeof(EventReceiver));
	portEXIT_CRITICAL();

	if (!ipmi_event_receiver.ipmb) {
		this->logunique.log_unique(stdsprintf("There is not yet an IPMI Event Receiver.  Discarding events on sensor \"%s\".", sdr->id_string().c_str()), LogTree::LOG_DIAGNOSTIC);
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
