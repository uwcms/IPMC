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

#ifndef SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_SENSOR_SENSOR_SET_H_
#define SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_SENSOR_SENSOR_SET_H_

#include <memory>
#include <map>
#include <FreeRTOS.h>
#include <semphr.h>
#include <services/ipmi/sdr/sensor_data_record_sensor.h>
#include <services/ipmi/sdr/sensor_data_repository.h>
#include <services/ipmi/sensor/sensor.h>

/**
 * A class providing threadsafe access and management for a set of sensors.
 */
class SensorSet final {
public:
	/**
	 * Instantiate a SensorSet.
	 *
	 * @param sdr_repo The optional SDR repository to look up sensor names in for
	 *                 find_by_name() searches.
	 */
	SensorSet(SensorDataRepository *sdr_repo = nullptr);
	~SensorSet();

	typedef std::map< uint8_t, std::shared_ptr<Sensor> > container_type;

	/**
	 * Add a sensor to the set, replacing any existing sensor with the same sensor
	 * number.
	 *
	 * @param sensor The sensor
	 */
	void add(std::shared_ptr<Sensor> sensor);

	/**
	 * Remove a provided sensor from the set.
	 *
	 * @param sensor The number of the sensor to remove
	 */
	void remove(uint8_t sensor_number);

	/**
	 * Retrieve an individual sensor by its number.
	 *
	 * @note This is the most efficient access method.
	 *
	 * @param sensor_number The sensor number from the SDR.
	 * @return A sensor or nullptr if unavailable.
	 */
	std::shared_ptr<Sensor> get(uint8_t sensor_number);

	/**
	 * Allow a user to copy/assign us as a std:map and provide a snapshot copy of
	 * our data, for iteration or other purposes.
	 *
	 * @return A snapshot of the inner container.
	 */
	operator container_type();

	/**
	 * Look up and return a Sensor by its SDR Key.
	 *
	 * @param sdr_key The SDR key the sensor associates with.
	 * @return A sensor or nullptr if unavailable.
	 */
	std::shared_ptr<Sensor> findBySdrKey(const std::vector<uint8_t> &sdr_key);

	/**
	 * Look up and return a Sensor by its SDR.
	 *
	 * @param sdr The SDR the sensor associates with.
	 * @return A sensor or nullptr if unavailable.
	 */
	std::shared_ptr<Sensor> findBySdr(std::shared_ptr<const SensorDataRecordSensor> sdr);

	/**
	 * Look up and return a Sensor by its ID String.
	 *
	 * @note This is the slowest access method as it requires an SDR lookup for each
	 *       sensor in the set.
	 *
	 * @note This method will always return nullptr if there is no associated SDR
	 *       repository.
	 *
	 * @param sdr The ID string of the sensor's associated SDR.
	 * @return A sensor or nullptr if unavailable.
	 */
	std::shared_ptr<Sensor> findByName(const std::string &name);

private:
	container_type set; ///< The actual sensor set.
	SemaphoreHandle_t mutex; ///< A mutex protecting the actual sensor set.
	SensorDataRepository *sdr_repo; ///< A SDR repository used for name lookups.
};

#endif /* SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_SENSOR_SENSOR_SET_H_ */
