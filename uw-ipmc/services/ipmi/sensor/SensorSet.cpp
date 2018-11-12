/*
 * SensorSet.cpp
 *
 *  Created on: Nov 9, 2018
 *      Author: jtikalsky
 */

#include <services/ipmi/sensor/SensorSet.h>
#include <FreeRTOS.h>
#include <semphr.h>

/**
 * Instantiate a SensorSet
 *
 * @param sdr_repo The optional SDR repository to look up sensor names in for
 *                 find_by_name() searches.
 */
SensorSet::SensorSet(SensorDataRepository *sdr_repo) : sdr_repo(sdr_repo) {
	configASSERT(this->mutex = xSemaphoreCreateMutex());
}
SensorSet::~SensorSet() {
	vSemaphoreDelete(this->mutex);
}

/**
 * Add a sensor to the set, replacing any existing sensor with the same sensor
 * number.
 *
 * @param sensor The sensor
 */
void SensorSet::add(std::shared_ptr<Sensor> sensor) {
	configASSERT(sensor);
	xSemaphoreTake(this->mutex, portMAX_DELAY);
	this->set[sensor->sdr_key[2]] = sensor;
	xSemaphoreGive(this->mutex);
}

/**
 * Remove a provided sensor from the set.
 *
 * @param sensor The number of the sensor to remove
 */
void SensorSet::remove(uint8_t sensor_number) {
	xSemaphoreTake(this->mutex, portMAX_DELAY);
	if (this->set.count(sensor_number))
		this->set.erase(sensor_number);
	xSemaphoreGive(this->mutex);
}

/**
 * Retrieve an individual sensor by its number.
 *
 * \note This is the most efficient access method.
 *
 * @param sensor_number The sensor number from the SDR.
 * @return A sensor or NULL if unavailable.
 */
std::shared_ptr<Sensor> SensorSet::get(uint8_t sensor_number) {
	xSemaphoreTake(this->mutex, portMAX_DELAY);
	std::shared_ptr<Sensor> ret = NULL;
	if (this->set.count(sensor_number))
		ret = this->set.at(sensor_number);
	xSemaphoreGive(this->mutex);
	return ret;
}

/**
 * Allow a user to copy/assign us as a std:map and provide a snapshot copy of
 * our data, for iteration or other purposes.
 *
 * @return A snapshot of the inner container.
 */
SensorSet::operator SensorSet::container_type() {
	xSemaphoreTake(this->mutex, portMAX_DELAY);
	container_type ret(this->set);
	xSemaphoreGive(this->mutex);
	return ret;
}

/**
 * Look up and return a Sensor by its SDR Key.
 *
 * @param sdr_key The SDR key the sensor associates with.
 * @return A sensor or NULL if unavailable.
 */
std::shared_ptr<Sensor> SensorSet::find_by_sdr_key(const std::vector<uint8_t> &sdr_key) {
	xSemaphoreTake(this->mutex, portMAX_DELAY);
	std::shared_ptr<Sensor> ret = NULL;
	for (auto it = this->set.begin(), eit = this->set.end(); it != eit; ++it) {
		if (it->second->sdr_key == sdr_key) {
			ret = it->second;
			break;
		}
	}
	xSemaphoreGive(this->mutex);
	return ret;
}

/**
 * Look up and return a Sensor by its SDR.
 *
 * @param sdr The SDR the sensor associates with.
 * @return A sensor or NULL if unavailable.
 */
std::shared_ptr<Sensor> SensorSet::find_by_sdr(std::shared_ptr<const SensorDataRecordSensor> sdr) {
	configASSERT(sdr);
	return this->find_by_sdr_key(sdr->record_key());
}

/**
 * Look up and return a Sensor by its ID String.
 *
 * \note This is the slowest access method as it requires an SDR lookup for each
 *       sensor in the set.
 *
 * \note This method will always return NULL if there is no associated SDR
 *       repository.
 *
 * @param sdr The ID string of the sensor's associated SDR.
 * @return A sensor or NULL if unavailable.
 */
std::shared_ptr<Sensor> SensorSet::find_by_name(const std::string &name) {
	if (!this->sdr_repo)
		return NULL; // We can't do name lookups.
	xSemaphoreTake(this->mutex, portMAX_DELAY);
	std::shared_ptr<Sensor> ret = NULL;
	for (auto it = this->set.begin(), eit = this->set.end(); it != eit; ++it) {
		std::shared_ptr<const SensorDataRecordSensor> sdr = std::dynamic_pointer_cast<const SensorDataRecordSensor>(this->sdr_repo->find(it->second->sdr_key));
		if (!sdr)
			continue;
		if (sdr->id_string() == name) {
			ret = it->second;
			break;
		}
	}
	xSemaphoreGive(this->mutex);
	return ret;
}
