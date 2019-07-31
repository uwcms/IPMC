#ifndef SRC_COMMON_UW_IPMC_SERVICES_IPMI_SENSOR_SENSORSET_H_
#define SRC_COMMON_UW_IPMC_SERVICES_IPMI_SENSOR_SENSORSET_H_

#include <memory>
#include <map>
#include <FreeRTOS.h>
#include <semphr.h>
#include <services/ipmi/sdr/sensor_data_record_sensor.h>
#include <services/ipmi/sdr/sensor_data_repository.h>
#include <services/ipmi/sensor/Sensor.h>

/**
 * A class providing threadsafe access and management for a set of sensors.
 */
class SensorSet final {
public:
	SensorSet(SensorDataRepository *sdr_repo=NULL);
	~SensorSet();

	typedef std::map< uint8_t, std::shared_ptr<Sensor> > container_type;
	void add(std::shared_ptr<Sensor> sensor);
	void remove(uint8_t sensor_number);
	std::shared_ptr<Sensor> get(uint8_t sensor_number);
	operator container_type();
	std::shared_ptr<Sensor> find_by_sdr_key(const std::vector<uint8_t> &sdr_key);
	std::shared_ptr<Sensor> find_by_sdr(std::shared_ptr<const SensorDataRecordSensor> sdr);
	std::shared_ptr<Sensor> find_by_name(const std::string &name);

protected:
	container_type set; ///< The actual sensor set.
	SemaphoreHandle_t mutex; ///< A mutex protecting the actual sensor set.
	SensorDataRepository *sdr_repo; ///< A SDR repository used for name lookups.
};

#endif /* SRC_COMMON_UW_IPMC_SERVICES_IPMI_SENSOR_SENSORSET_H_ */
