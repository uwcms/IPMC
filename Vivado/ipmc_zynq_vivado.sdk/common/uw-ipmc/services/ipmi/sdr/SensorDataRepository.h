/*
 * SensorDataRepository.h
 *
 *  Created on: Sep 6, 2018
 *      Author: jtikalsky
 */

#ifndef SRC_COMMON_UW_IPMC_SERVICES_IPMI_SDR_SENSORDATAREPOSITORY_H_
#define SRC_COMMON_UW_IPMC_SERVICES_IPMI_SDR_SENSORDATAREPOSITORY_H_

#include <FreeRTOS.h>
#include <semphr.h>
#include <vector>
#include <memory>
#include <stdint.h>
#include <time.h>
#include <services/ipmi/sdr/SensorDataRecord.h>

class SensorDataRepository final {
public:
	SensorDataRepository();
	virtual ~SensorDataRepository();

	bool add(const SensorDataRecord &record, uint8_t reservation);
	bool add(const SensorDataRepository &sdrepository, uint8_t reservation);
	bool remove(uint16_t id, uint8_t reservation);
	bool remove(const SensorDataRecord &record, uint8_t reservation);
	bool clear(uint8_t reservation);
	std::shared_ptr<const SensorDataRecord> get(uint16_t id) const;
	std::shared_ptr<const SensorDataRecord> find(const std::vector<uint8_t> &key) const;
	/// We inherit our size_type from our underlying std::vector.
	typedef std::vector< std::shared_ptr<SensorDataRecord> >::size_type size_type;
	size_type size() const;
	explicit operator std::vector< std::shared_ptr<const SensorDataRecord> >() const;

	time_t last_update_timestamp();

	std::vector<uint8_t> u8export() const;
	bool u8import(const std::vector<uint8_t> &data, uint8_t reservation = 0);

	typedef uint16_t reservation_t;
	reservation_t get_current_reservation() const;
	reservation_t reserve();

protected:
	reservation_t reservation; ///< The current reservation number for this repository
	std::vector< std::shared_ptr<SensorDataRecord> > records; ///< The actual SDRs
	time_t last_update_ts; ///< The timestamp of the last update.
	void renumber();
	SemaphoreHandle_t mutex; ///< A mutex to protect repository operations.
};

#endif /* SRC_COMMON_UW_IPMC_SERVICES_IPMI_SDR_SENSORDATAREPOSITORY_H_ */
