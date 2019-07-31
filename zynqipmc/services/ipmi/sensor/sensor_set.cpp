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
#include <libs/threading.h>
#include <semphr.h>
#include <services/ipmi/sensor/sensor_set.h>

SensorSet::SensorSet(SensorDataRepository *sdr_repo) : sdr_repo(sdr_repo) {
	configASSERT(this->mutex = xSemaphoreCreateMutex());
}

SensorSet::~SensorSet() {
	vSemaphoreDelete(this->mutex);
}

void SensorSet::add(std::shared_ptr<Sensor> sensor) {
	if (!sensor) return;

	MutexGuard<false> lock(this->mutex, true);
	this->set[sensor->getSdrKey()[2]] = sensor;
}

void SensorSet::remove(uint8_t sensor_number) {
	MutexGuard<false> lock(this->mutex, true);
	if (this->set.count(sensor_number)) {
		this->set.erase(sensor_number);
	}
}

std::shared_ptr<Sensor> SensorSet::get(uint8_t sensor_number) {
	MutexGuard<false> lock(this->mutex, true);
	std::shared_ptr<Sensor> ret = nullptr;
	if (this->set.count(sensor_number)) {
		ret = this->set.at(sensor_number);
	}
	return ret;
}

SensorSet::operator SensorSet::container_type() {
	MutexGuard<false> lock(this->mutex, true);
	return container_type(this->set);
}

std::shared_ptr<Sensor> SensorSet::findBySdrKey(const std::vector<uint8_t> &sdr_key) {
	MutexGuard<false> lock(this->mutex, true);
	std::shared_ptr<Sensor> ret = nullptr;
	for (auto it = this->set.begin(), eit = this->set.end(); it != eit; ++it) {
		if (it->second->getSdrKey() == sdr_key) {
			ret = it->second;
			break;
		}
	}
	return ret;
}

std::shared_ptr<Sensor> SensorSet::findBySdr(std::shared_ptr<const SensorDataRecordSensor> sdr) {
	if (!sdr) return nullptr;

	return this->findBySdrKey(sdr->recordKey());
}

std::shared_ptr<Sensor> SensorSet::findByName(const std::string &name) {
	if (!this->sdr_repo) return nullptr; // We can't do name lookups.

	MutexGuard<false> lock(this->mutex, true);
	std::shared_ptr<Sensor> ret = nullptr;
	for (auto it = this->set.begin(), eit = this->set.end(); it != eit; ++it) {
		std::shared_ptr<const SensorDataRecordSensor> sdr = std::dynamic_pointer_cast<const SensorDataRecordSensor>(this->sdr_repo->find(it->second->getSdrKey()));
		if (!sdr) continue;

		if (sdr->id_string() == name) {
			ret = it->second;
			break;
		}
	}
	return ret;
}
