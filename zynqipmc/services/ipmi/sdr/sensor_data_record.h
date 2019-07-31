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

#ifndef SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_SDR_SENSOR_DATA_RECORD_H_
#define SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_SDR_SENSOR_DATA_RECORD_H_

#include <vector>
#include <stdint.h>
#include <memory>
#include <libs/except.h>

/**
 * A minimal class implementing the most basic level of SDR(ecord) support,
 * intended as an interface superclass for all SDR(ecord) objects.
 *
 * @warning Do not call any accessors on a record that does not validate().
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

	/**
	 * Validate the current Sensor Data Record.
	 *
	 * If this function returns true, it is safe to read all relevant fields from
	 * the record.  It only tests if the data is well-formed for the
	 * SensorDataRecord subclass it is being interpreted by.  It may not be valid
	 * with any other parser.
	 */
	virtual void validate() const;

	//! Returns the record type supported by the current handler subclass.
	virtual uint8_t parsedRecordType() const = 0;

	/**
	 * Return a shared_ptr to a the appropriate generic SensorDataRecord subclass
	 * for parsing and managing this record.
	 *
	 * @note This will return a nullptr shared_ptr if the record cannot be handled by
	 *       an appropriate subclass.
	 *
	 * @param The SDR data to interpret.
	 * @return A shared_ptr to a subclass if possible, false otherwise.
	 */
	static std::shared_ptr<SensorDataRecord> interpret(const std::vector<uint8_t> &data);
	/// \overload
	std::shared_ptr<SensorDataRecord> interpret() const { return interpret(this->sdr_data); };

	/**
	 * SDR Data Accessors
	 *
	 * @warning Do not call any accessors on a record that does not validate().
	 */
	///@{
	virtual uint16_t recordId() const;
	virtual void recordId(uint16_t record_id);
	virtual uint8_t recordVersion() const;
	virtual uint8_t recordType() const;
	virtual uint8_t recordLength() const;

	/**
	 * Get the record key bytes for this SensorDataRecord subclass.
	 *
	 * @warning Do not call this on an un-interpreted SensorDataRecord (i.e. non-subclass).
	 * @return The record key bytes.
	 */
	virtual std::vector<uint8_t> recordKey() const = 0;
	///@}

	//! "Is the same record" comparison, based on Type & Key Bytes.
	virtual bool operator==(const SensorDataRecord &b) const {
		if (this->recordType() != b.recordType())
			return false;

		return (this->recordKey() == b.recordKey());
	}

	//! "Is not the same record" comparison, based on Type & Key Bytes.
	virtual bool operator!=(const SensorDataRecord &b) const {
		return !(*this == b);
	}

	/**
	 * Export this SDR as a byte string suitable for Get SDR commands.
	 *
	 * The returned record will have the specified owner IPMB address and channel if
	 * supported and relevant.
	 *
	 * @param self_ipmb_addr Local IPMB address.
	 * @param self_ipmb_lun Local IPMB channel.
	 * @return A u8 vector suitable for Get SDR commands.
	 */
	virtual std::vector<uint8_t> u8export(uint8_t self_ipmb_addr=0, uint8_t self_ipmb_channel=0) const;

	/**
	 * Check whether two SensorDataRecords have identical content (but not
	 * necessarily identical internal extra fields (use a == on .sdr_data for that).
	 *
	 * @param b the record to compare to.
	 * @param compare_record_id true if record_ids must also match.
	 * @return true if the records are identical, else false.
	 */
	virtual bool identicalContent(const SensorDataRecord &b, bool compare_record_id) const;
};

#endif /* SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_SDR_SENSOR_DATA_RECORD_H_ */
