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

#ifndef SRC_COMMON_ZYNQIPMC_DRIVERS_TRACEBUFFER_TRACEBUFFER_H_
#define SRC_COMMON_ZYNQIPMC_DRIVERS_TRACEBUFFER_TRACEBUFFER_H_

#include <stdint.h>
#include <stddef.h>
#include <string>
#include <libs/logtree/logtree.h>

/**
 * This provides a Trace Buffer facility to allow memory dumps of structured
 * high-volume detailed tracing data.
 */
class TraceBuffer final {
public:
	/**
	 * Instantiate a new TraceBuffer, using the provided buffer.
	 * @param buf      A buffer for data storage
	 * @param bufsize  The size of the buffer in bytes
	 */
	TraceBuffer(uint8_t *buf, size_t bufsize);
	~TraceBuffer();

	/**
	 * Write a log message to the trace buffer.
	 *
	 * While this is normally to be used by the LogTree subsystem, it is ISR SAFE,
	 * and can therefore be used to log tracing data directly in the ISR, where
	 * standard logging (to console, ethernet, and other) is unavailable.
	 *
	 * @param label      The full label of the log facility committing this message
	 * @param label_len  The length of `label`
	 * @param loglevel   The loglevel of this message
	 * @param data       The content of the message
	 * @param data_len   The length of `data` (allowing binary tracing)
	 * @param binary     True if the log data is binary instead of human readable
	 */
	void log(const char *label, size_t label_len, enum LogTree::LogLevel loglevel, const char *data, size_t data_len, bool binary=false);
	void log(std::string label, enum LogTree::LogLevel loglevel, std::string data, bool binary=false) {
		this->log(label.data(), label.size(), loglevel, data.data(), data.size(), binary);
	};

protected:
	//! A struct defining a header for the tracebuffer itself.
	struct TraceBufferHeader {
		uint32_t total_length;	///< The total size of the trace buffer (minus header)
		uint32_t last_record;	///< The offset of the last record written (0xffffffff if empty)
		uint8_t buffer[0];		///< An access path for the buffer itself.
	};

	//! A struct defining a tracebuffer record.
	struct TraceRecord {
		uint32_t previous_record;	///< Offset of the previous record written (0xffffffff if none).
		uint32_t label_length;		///< Length in bytes of the label string.
		uint32_t data_length;		///< Length in bytes of the data.
		uint16_t loglevel;			///< Loglevel of the record.
		/**
		 * Flags related to the TraceRecord.
		 * [31: 1]  Reserved
		 * [    0]  Binary log data (1),  String log data (0)
		 */
		uint16_t flags;
		uint64_t timestamp;       ///< The tick64 this record was written at.
		uint8_t data[0];          ///< Access path for the variable size data segment.
	};

	struct TraceBufferHeader *buf; ///< The trace buffer itself
};

#endif /* SRC_COMMON_ZYNQIPMC_DRIVERS_TRACEBUFFER_TRACEBUFFER_H_ */
