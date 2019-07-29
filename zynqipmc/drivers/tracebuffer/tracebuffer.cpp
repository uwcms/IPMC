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

#include "tracebuffer.h"
#include <libs/threading.h>

#define TRACEREC_PTR(off) (reinterpret_cast<struct TraceRecord*>((this->buf->buffer)+(off)))
#define TRACEBUF_NULL_MARKER 0xffffffff

TraceBuffer::TraceBuffer(uint8_t *buf, size_t bufsize) :
buf(reinterpret_cast<struct TraceBufferHeader*>(buf)) {
	configASSERT(bufsize >= sizeof(struct TraceBufferHeader)+sizeof(TraceBuffer::TraceRecord));
	configASSERT(bufsize < 0xffffffff);

	this->buf->total_length = bufsize - sizeof(struct TraceBufferHeader);
	this->buf->last_record = TRACEBUF_NULL_MARKER;
};

TraceBuffer::~TraceBuffer() {
	// Nothing to do here.
}

void TraceBuffer::log(const char *label, size_t label_len, enum LogTree::LogLevel loglevel, const char *data, size_t data_len, bool binary) {
	const size_t record_length = sizeof(struct TraceRecord) + label_len + data_len;

	// The record must fit in the buffer.
	configASSERT(record_length <= this->buf->total_length);

	CriticalGuard critical(true);

	uint32_t next_offset;
	struct TraceRecord *rec;
	if (this->buf->last_record == TRACEBUF_NULL_MARKER) {
		// No records.
		next_offset = 0;
		rec = TRACEREC_PTR(0);
		rec->previous_record = TRACEBUF_NULL_MARKER;
	} else {
		uint32_t prev_record_offset = this->buf->last_record;
		rec = TRACEREC_PTR(prev_record_offset);
		next_offset = prev_record_offset + sizeof(struct TraceRecord) + rec->label_length + rec->data_length;
		if (next_offset % 4)
			next_offset += 4-(next_offset%4); // Word-align all records.
		if (next_offset + record_length > this->buf->total_length)
			next_offset = 0;
		rec = TRACEREC_PTR(next_offset);
		rec->previous_record = prev_record_offset;
	}

	rec->label_length = label_len;
	rec->data_length = data_len;
	rec->loglevel = loglevel;
	rec->flags = (binary ? 1 : 0);

	uint64_t now64 = get_tick64();
	memcpy(&rec->timestamp, &now64, sizeof(uint64_t)); // Cortex-A9 apparently does not support unaligned strd, only unaligned str.
	memcpy(rec->data, label, label_len);
	memcpy(rec->data + label_len, data, data_len);

	this->buf->last_record = next_offset;
}
