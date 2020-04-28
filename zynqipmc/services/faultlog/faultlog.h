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

#ifndef SRC_COMMON_ZYNQIPMC_SERVICES_FAULTLOG_FAULTLOG_H_
#define SRC_COMMON_ZYNQIPMC_SERVICES_FAULTLOG_FAULTLOG_H_

#include <services/persistentstorage/persistent_storage.h>
#include <services/console/command_parser.h>
#include <libs/logtree/logtree.h>
#include <deque>
#include <semphr.h>

/**
 * A class providing EEPROM fault log functionality.
 */
class FaultLog : public ConsoleCommandSupport {
public:
	FaultLog(PersistentStorage &persistent_storage, LogTree &logtree);
	virtual ~FaultLog();

	/**
	 * A fault log entry
	 */
	struct fault {
		uint32_t unixtime; ///< Epoch timestamp of the event (auto-filled on submit)
		uint8_t sequence; ///< sequence number (treat as opaque, auto-filled on submit)
		enum {
			FAULT_TYPE_MZ_FAULTED   = 0,
			FAULT_TYPE_SENSOR_EVENT = 1,
			FAULT_OEM               = 255,
		} fault_type:8; ///< The fault type.
		union {
			uint8_t fault_data[10];
			struct {
				uint8_t mz_number;
				uint8_t _unused[9];
			} mz_fault;
			struct {
				uint8_t ipmi_eventmsg[7];
				uint8_t _unused[3];
			} sensor_event;
		};
	};

	/**
	 * Submit a fault to the log.
	 *
	 * @param fault The fault to log. (Sequence and timestamp will be updated)
	 */
	void submit(struct fault &fault);

	/**
	 * Retrieve the contents of the fault log.
	 * @return The contents of the fault log.
	 */
	std::deque<struct fault> dump() const;

	/**
	 * Erase the contents of the fault log.
	 */
	void erase();

	/**
	 * Sets the verbosity config variable (not used internally)
	 * @param config The new value.
	 */
	void set_verbosity_config(uint32_t config) {
		this->faultlog->verbosity_config = config;
		this->persistent_storage.flush(&this->faultlog->verbosity_config, sizeof(uint32_t));
	};

	/**
	 * Reads the value of the verbosity config variable (not used internally)
	 * @return The current value. (default = 0xffffffff)
	 */
	uint32_t get_verbosity_config() { return this->faultlog->verbosity_config; };

	// From base class ConsoleCommandSupport:
	virtual void registerConsoleCommands(CommandParser &parser, const std::string &prefix="");
	virtual void deregisterConsoleCommands(CommandParser &parser, const std::string &prefix="");

	// Consider header and block size if choosing to change this.
	// This MUST be < 255 on pain of infinite loop. (not just <=)
	static const size_t fault_log_max_records = 127;

protected:
	PersistentStorage &persistent_storage; ///< Persistent Storage
	LogTree &logtree; ///< The LogTree for status logging (unrelated to fault storage)
	SemaphoreHandle_t mutex; ///< A mutex for log operations.

	/**
	 * The fault log header
	 */
	struct faultlog {
		uint8_t max_record_count;
		uint8_t _unused[3];
		uint32_t verbosity_config;
		struct fault log[fault_log_max_records];
	};
	struct faultlog *faultlog; ///< The actual fault log region
	uint8_t next_record; ///< The next record to write.
	uint8_t next_sequence; ///< The sequence number of the next record.

	// Available console commands:
	class DumpCommand;
	class EraseCommand;
};

#endif /* SRC_COMMON_ZYNQIPMC_SERVICES_FAULTLOG_FAULTLOG_H_ */
