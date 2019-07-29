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

#ifndef SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_SDR_SENSOR_DATA_REPOSITORY_H_
#define SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_SDR_SENSOR_DATA_REPOSITORY_H_

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
	// Repository specific exceptions:
	DEFINE_LOCAL_GENERIC_EXCEPTION(sdr_invalid_error, std::runtime_error)
	DEFINE_LOCAL_GENERIC_EXCEPTION(reservation_cancelled_error, std::runtime_error)

	SensorDataRepository();
	~SensorDataRepository();

	typedef uint16_t reservation_t;

	/**
	 * Add a Sensor Data Record to this repository.  If another record with the same
	 * type and key bytes already exists, it will be replaced.
	 *
	 * @param record The record to add.
	 * @param reservation The current reservation (or 0 to oneshot reserve for this
	 *                    operation).  If this does not match the current
	 *                    reservation, the operation will not complete.
	 * @return The record ID of the inserted record.
	 * @throw reservation_cancelled_error
	 * @throw sdr_invalid_error
	 */
	uint16_t add(const SensorDataRecord &record, reservation_t reservation);

	/**
	 * Add all records in the specified Sensor Data Repository to this repository.
	 *
	 * @note SDRs which could not be interpreted will not be added.
	 *
	 * @param sdrepository The record to add.
	 * @param reservation The current reservation (or 0 to oneshot reserve for this
	 *                    operation).  If this does not match the current
	 *                    reservation, the operation will not complete.
	 * @throw reservation_cancelled_error
	 */
	void add(const SensorDataRepository &sdrepository, reservation_t reservation);

	/**
	 * Remove the specified Sensor Data Record from this repository.
	 *
	 * @param id The record id of the record to remove.
	 * @param reservation The current reservation (or 0 to oneshot reserve for this
	 *                    operation).  If this does not match the current
	 *                    reservation, the operation will not complete.
	 * @return true if a record was removed else false.
	 * @throw reservation_cancelled_error
	 */
	bool remove(uint16_t id, reservation_t reservation);

	/**
	 * Remove all records with the same type and key bytes as the provided SDR from
	 * the repository.
	 *
	 * @param record The SDR to match for removal.
	 * @param reservation The current reservation (or 0 to oneshot reserve for this
	 *                    operation).  If this does not match the current
	 *                    reservation, the operation will not complete.
	 * @return true if at least one record was removed else false.
	 * @throw reservation_cancelled_error
	 */
	bool remove(const SensorDataRecord &record, reservation_t reservation);

	/**
	 * Erase the contents of this repository.
	 *
	 * @param reservation The current reservation (or 0 to oneshot reserve for this
	 *                    operation).  If this does not match the current
	 *                    reservation, the operation will not complete.
	 * @throw reservation_cancelled_error
	 */
	void clear(reservation_t reservation);

	/**
	 * Retrieve the specified Sensor Data Record from this repository.
	 *
	 * @param id The record id of the record to retrieve.
	 * @return A std::shared_ptr<SensorDataRecord> containing a specialized SDR subclass.
	 * @throw reservation_cancelled_error
	 */
	std::shared_ptr<const SensorDataRecord> get(uint16_t id, reservation_t reservation=0) const;

	/**
	 * Retrieve the specified Sensor Data Record from this repository.
	 *
	 * @param key The record key bytes of the record to retrieve.
	 * @return A std::shared_ptr<SensorDataRecord> containing a specialized SDR subclass.
	 * @throw reservation_cancelled_error
	 */
	std::shared_ptr<const SensorDataRecord> find(const std::vector<uint8_t> &key, reservation_t reservation = 0) const;

	/// We inherit our size_type from our underlying std::vector.
	typedef std::vector< std::shared_ptr<SensorDataRecord> >::size_type size_type;

	//! Return the number of records in this container.
	size_type size() const;

	//! Export our database for external iteration or analysis.
	explicit operator std::vector< std::shared_ptr<const SensorDataRecord> >() const;

	//! Retrieve the last update timestamp for the repository.
	time_t lastUpdateTimestamp();

	/**
	 * Export the data from this SDR repository as a u8 vector.
	 *
	 * @warning This will potentially discard invalid SDR records from the repository.
	 *
	 * @return The SDR repository contents in binary form.
	 */
	std::vector<uint8_t> u8export() const;

	/**
	 * Import the data from the supplied u8 vector, merging with this SDR repository.
	 *
	 * @note Invalid records are silently discarded, but synchronization, format
	 *       or checksum errors are detectable.  If false is returned, no changes to
	 *       the repository will have been made.
	 *
	 * @param data The SDR repository contents in binary form.
	 * @param reservation The current reservation (or 0 to oneshot reserve for this
	 *                    operation).  If this does not match the current
	 *                    reservation, the operation will not complete.
	 * @return true if the operation was successful, false if data was invalid.
	 * @throw reservation_cancelled_error
	 */
	bool u8import(const std::vector<uint8_t> &data, reservation_t reservation = 0);

	//! Returns the ID of the current reservation.
	reservation_t getCurrentReservation() const;

	/**
	 * Reserves the repository, returning the new reservation id.
	 * @return the new reservation id.
	 */
	reservation_t reserve();

private:
	reservation_t reservation;	///< The current reservation number for this repository
	std::vector< std::shared_ptr<SensorDataRecord> > records;	///< The actual SDRs
	time_t last_update_ts;		///< The timestamp of the last update.
	SemaphoreHandle_t mutex;	///< A mutex to protect repository operations.

	//! Renumber the SDRs in the repository after changes.
	void renumber();

	/**
	 * Acquire or verify a supplied reservation.
	 * @param reservation The reservation to acquire or verify.
	 * @throw reservation_cancelled_error
	 */
	void assert_reservation(reservation_t &reservation);
};

#endif /* SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_SDR_SENSOR_DATA_REPOSITORY_H_ */
