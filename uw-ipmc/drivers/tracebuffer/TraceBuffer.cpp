/*
 * TraceBuffer.cpp
 *
 *  Created on: Dec 14, 2017
 *      Author: jtikalsky
 */

#include "string.h"
#include <IPMC.h>
#include <libs/ThreadingPrimitives.h>
#include <drivers/tracebuffer/TraceBuffer.h>
#include "FreeRTOS.h"
#include "task.h"

/**
 * A struct defining a header for the tracebuffer itself.
 */
struct TraceBuffer::TraceBufferHeader {
	uint32_t total_length; ///< The total size of the trace buffer (minus header)
	uint32_t last_record;  ///< The offset of the last record written (0xffffffff if empty)
	uint8_t buffer[0];     ///< An access path for the buffer itself.
};

/**
 * A struct defining a tracebuffer record.
 */
struct TraceBuffer::TraceRecord {
	uint32_t previous_record; ///< The offset of the previous record written (0xffffffff if none)
	uint32_t label_length;    ///< The length in bytes of the label string
	uint32_t data_length;     ///< The length in bytes of the data
	uint16_t loglevel;        ///< The loglevel of the record
	/**
	 * Flags related to the TraceRecord.
	 * [31: 1]  Reserved
	 * [    0]  Binary log data (1),  String log data (0)
	 */
	uint16_t flags;
	uint8_t data[0];          ///< An access path for the variable size data segment
};

#define TRACEREC_PTR(off) (reinterpret_cast<struct TraceRecord*>((this->buf->buffer)+(off)))
#define TRACEBUF_NULL_MARKER 0xffffffff

/**
 * Instantiate a new TraceBuffer, using the provided buffer.
 *
 * @param buf      A buffer for data storage
 * @param bufsize  The size of the buffer in bytes
 */
TraceBuffer::TraceBuffer(uint8_t *buf, size_t bufsize)
	: buf(reinterpret_cast<struct TraceBufferHeader*>(buf)) {

	configASSERT(bufsize >= sizeof(struct TraceBufferHeader)+sizeof(TraceBuffer::TraceRecord));
	configASSERT(bufsize < 0xffffffff);

	this->buf->total_length = bufsize - sizeof(struct TraceBufferHeader);
	this->buf->last_record = TRACEBUF_NULL_MARKER;
};

TraceBuffer::~TraceBuffer() {
	// Nothing to do here.
}

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
void TraceBuffer::log(const char *label, size_t label_len, enum LogTree::LogLevel loglevel, const char *data, size_t data_len, bool binary) {
	const size_t record_length = sizeof(struct TraceRecord) + label_len + data_len;

	// The record must fit in the buffer.
	configASSERT(record_length <= this->buf->total_length);

	if (!IN_INTERRUPT())
		taskENTER_CRITICAL();

	uint32_t next_offset;
	struct TraceRecord *rec;
	if (this->buf->last_record == TRACEBUF_NULL_MARKER) {
		// No records.
		next_offset = 0;
		rec = TRACEREC_PTR(0);
		rec->previous_record = TRACEBUF_NULL_MARKER;
	}
	else {
		uint32_t prev_record_offset = this->buf->last_record;
		rec = TRACEREC_PTR(prev_record_offset);
		next_offset = prev_record_offset + sizeof(struct TraceRecord) + rec->label_length + rec->data_length;
		if (next_offset + record_length > this->buf->total_length)
			next_offset = 0;
		rec = TRACEREC_PTR(next_offset);
		rec->previous_record = prev_record_offset;
	}

	rec->label_length = label_len;
	rec->data_length = data_len;
	rec->loglevel = loglevel;
	rec->flags = (binary ? 1 : 0);
	memcpy(rec->data, label, label_len);
	memcpy(rec->data + label_len, data, data_len);

	this->buf->last_record = next_offset;

	if (!IN_INTERRUPT())
		taskEXIT_CRITICAL();
}
