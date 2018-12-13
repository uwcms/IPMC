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
#include <libs/except.h>

class SensorDataRepository final {
public:
	SensorDataRepository();
	virtual ~SensorDataRepository();

	DEFINE_LOCAL_GENERIC_EXCEPTION(reservation_cancelled_error, std::runtime_error)
	typedef uint16_t reservation_t;

	void add(const SensorDataRecord &record, reservation_t reservation);
	void add(const SensorDataRepository &sdrepository, reservation_t reservation);
	void remove(uint16_t id, reservation_t reservation);
	void remove(const SensorDataRecord &record, reservation_t reservation);
	void clear(reservation_t reservation);
	std::shared_ptr<const SensorDataRecord> get(uint16_t id, reservation_t reservation=0) const;
	std::shared_ptr<const SensorDataRecord> find(const std::vector<uint8_t> &key, reservation_t reservation=0) const;
	/// We inherit our size_type from our underlying std::vector.
	typedef std::vector< std::shared_ptr<SensorDataRecord> >::size_type size_type;
	size_type size() const;
	explicit operator std::vector< std::shared_ptr<const SensorDataRecord> >() const;

	time_t last_update_timestamp();

	std::vector<uint8_t> u8export() const;
	bool u8import(const std::vector<uint8_t> &data, reservation_t reservation = 0);

	reservation_t get_current_reservation() const;
	reservation_t reserve();

protected:
	reservation_t reservation; ///< The current reservation number for this repository
	std::vector< std::shared_ptr<SensorDataRecord> > records; ///< The actual SDRs
	time_t last_update_ts; ///< The timestamp of the last update.
	void renumber();
	void assert_reservation(reservation_t &reservation);
	SemaphoreHandle_t mutex; ///< A mutex to protect repository operations.
};

#endif /* SRC_COMMON_UW_IPMC_SERVICES_IPMI_SDR_SENSORDATAREPOSITORY_H_ */
