/*
 * SensorSource.cpp
 *
 *  Created on: Aug 14, 2018
 *      Author: mpv
 */

// TODO: Decide what to do with this...

#include <IPMC.h>
#include "SensorSource.h"
#include <libs/ThreadingPrimitives.h>

std::string SensorSource::sensorUnitToString(const SensorUnit& u) {
	switch(u) {
	case UNIT_CELSIUS: return "C";
	case UNIT_VOLT: return "V";
	case UNIT_AMPERE: return "A";
	}
	return "?";
}

void SensorSource::startRefreshTask(const std::string& taskname, const uint32_t ms_interval) {
	// TODO: Prevent from starting twice or more
	this->changeRefreshInterval(ms_interval);
	this->taskName = std::string("sd:") + taskname;

	UWTaskCreate(this->taskName, TASK_PRIORITY_DRIVER, [this]() -> void {
		this->_backgroundTask();
	});
}

void SensorSource::changeRefreshInterval(const uint32_t ms_interval) {
	this->tickInterval = pdMS_TO_TICKS(ms_interval);
}

void SensorSource::_backgroundTask() {
	while (true) {
		AbsoluteTimeout timeout(this->tickInterval);

		SensorList l = this->refreshSensorList();

		// Do something with the sensors
		/*for (SensorItem& i : l) {
			printf("%s[%s]: %0.2f %s", i.source, i.sensor, *(float*)&(i.data), SensorSource::sensorUnitToString(i.unit).c_str());
		}*/

		// Sleep for the amount of time remaining
		TickType_t r = timeout.get_timeout();
		if (timeout.get_timeout() == 0) {
			// Sensor gathering took more time than expected
			// TODO: Report possible issue
			printf("%s: Timer overrun in SensorSource.", this->taskName.c_str());
		} else {
			vTaskDelay(r);
		}
	}
}
