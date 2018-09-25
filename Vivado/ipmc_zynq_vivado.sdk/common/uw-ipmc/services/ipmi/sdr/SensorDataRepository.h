/*
 * SensorDataRepository.h
 *
 *  Created on: Sep 6, 2018
 *      Author: jtikalsky
 */

#ifndef SRC_COMMON_UW_IPMC_SERVICES_IPMI_SDR_SENSORDATAREPOSITORY_H_
#define SRC_COMMON_UW_IPMC_SERVICES_IPMI_SDR_SENSORDATAREPOSITORY_H_

#include <vector>
#include <memory>
#include <stdint.h>
#include <services/ipmi/sdr/SensorDataRecord.h>

class SensorDataRepository {
public:
	SensorDataRepository() : reservation(1) { };
	virtual ~SensorDataRepository() { };

	void add(const SensorDataRecord &record);
	void add(const SensorDataRepository &sdrepository);
	void remove(uint16_t key);
	void clear();
	std::shared_ptr<const SensorDataRecord> get(uint16_t key) const;
	std::vector< std::shared_ptr<SensorDataRecord> >::size_type size() const;

	std::vector<uint8_t> u8export() const;
	void u8import(const std::vector<uint8_t> &data);

	/**
	 * Returns the current reservation id for this repository.
	 * @return the current reservation id for this repository
	 */
	uint8_t get_reservation() const { return this->reservation; };
	uint8_t reserve();
protected:
	uint8_t reservation; ///< The current reservation number for this repository
	std::vector< std::shared_ptr<SensorDataRecord> > records; ///< The actual SDRs
	void renumber();
};

#endif /* SRC_COMMON_UW_IPMC_SERVICES_IPMI_SDR_SENSORDATAREPOSITORY_H_ */
