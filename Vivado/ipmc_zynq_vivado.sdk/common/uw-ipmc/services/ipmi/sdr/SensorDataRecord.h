/*
 * SensorDataRecord.h
 *
 *  Created on: Aug 3, 2018
 *      Author: jtikalsky
 */

#ifndef SRC_COMMON_UW_IPMC_SERVICES_IPMI_SDR_SENSORDATARECORD_H_
#define SRC_COMMON_UW_IPMC_SERVICES_IPMI_SDR_SENSORDATARECORD_H_

#include <vector>
#include <stdint.h>
#include <memory>

/**
 * A minimal class implementing the most basic level of SDR(ecord) support,
 * intended as an interface superclass for all SDR(ecord) objects.
 *
 * \warning Do not call any accessors on a record that does not validate().
 */
class SensorDataRecord {
public:
	std::vector<uint8_t> sdr_data; ///< Sensor Data Record bytes

	/**
	 * Instantiate a SensorDataRecord object with the provided raw record.
	 * @param sdr_data The raw SDR record bytes
	 */
	SensorDataRecord(const std::vector<uint8_t> &sdr_data) : sdr_data(sdr_data) { };
	virtual ~SensorDataRecord() { };

	virtual bool validate();

	std::shared_ptr<SensorDataRecord> interpret(); // TODO: Review.

	/**
	 * SDR Data Accessors
	 *
	 * \warning Do not call any accessors on a record that does not validate().
	 */
	///@{
	virtual uint16_t get_record_id();
	virtual void set_record_id(uint16_t record_id);
	virtual uint8_t get_record_version();
	virtual uint8_t get_record_type();

	/**
	 * Get the record key bytes for this SensorDataRecord subclass.
	 *
	 * \warning Do not call this on an un-interpreted SensorDataRecord (i.e. non-subclass).
	 * @return The record key bytes.
	 */
	virtual std::vector<uint8_t> get_record_key();
	///@}
};

#endif /* SRC_COMMON_UW_IPMC_SERVICES_IPMI_SDR_SENSORDATARECORD_H_ */
