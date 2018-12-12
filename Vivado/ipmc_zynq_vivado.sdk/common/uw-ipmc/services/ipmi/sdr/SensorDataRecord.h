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
#include <libs/except.h>

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
	SensorDataRecord(const std::vector<uint8_t> &sdr_data = std::vector<uint8_t>()) : sdr_data(sdr_data) { };
	virtual ~SensorDataRecord() { };

	DEFINE_LOCAL_GENERIC_EXCEPTION(invalid_sdr_error, std::runtime_error)

	virtual void validate() const;
	/**
	 * @return the Record Type supported by the current handler subclass.
	 */
	virtual uint8_t parsed_record_type() const { return 0xFF; };

	static std::shared_ptr<SensorDataRecord> interpret(const std::vector<uint8_t> &data);
	/// \overload
	std::shared_ptr<SensorDataRecord> interpret() const { return interpret(this->sdr_data); };

	/**
	 * SDR Data Accessors
	 *
	 * \warning Do not call any accessors on a record that does not validate().
	 */
	///@{
	virtual uint16_t record_id() const;
	virtual void record_id(uint16_t record_id);
	virtual uint8_t record_version() const;
	virtual uint8_t record_type() const;
	virtual uint8_t record_length() const;

	/**
	 * Get the record key bytes for this SensorDataRecord subclass.
	 *
	 * \warning Do not call this on an un-interpreted SensorDataRecord (i.e. non-subclass).
	 * @return The record key bytes.
	 */
	virtual std::vector<uint8_t> record_key() const = 0;
	///@}

	/// "Is the same record" comparison, based on Type & Key Bytes.
	virtual bool operator==(const SensorDataRecord &b) const {
		if (this->record_type() != b.record_type())
			return false;
		return (this->record_key() == b.record_key());
	}
	/// "Is not the same record" comparison, based on Type & Key Bytes.
	virtual bool operator!=(const SensorDataRecord &b) const {
		return !(*this == b);
	}

	virtual std::vector<uint8_t> u8export(uint8_t self_ipmb_addr=0, uint8_t self_ipmb_channel=0);
	virtual bool identical_content(const SensorDataRecord &b, bool compare_record_id) const;
};

#endif /* SRC_COMMON_UW_IPMC_SERVICES_IPMI_SDR_SENSORDATARECORD_H_ */
