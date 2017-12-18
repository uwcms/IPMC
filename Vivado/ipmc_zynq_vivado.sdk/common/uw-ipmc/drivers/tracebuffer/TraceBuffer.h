/*
 * TraceBuffer.h
 *
 *  Created on: Dec 14, 2017
 *      Author: jtikalsky
 */

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_TRACEBUFFER_TRACEBUFFER_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_TRACEBUFFER_TRACEBUFFER_H_

#include <stdint.h>
#include <stddef.h>
#include <libs/LogTree.h>

/**
 * This provides a Trace Buffer facility to allow memory dumps of structured
 * high-volume detailed tracing data.
 */
class TraceBuffer {
public:
	TraceBuffer(uint8_t *buf, size_t bufsize);
	virtual ~TraceBuffer();

	void log(const char *label, size_t label_len, enum LogTree::LogLevel loglevel, const char *data, size_t data_len, bool binary=false);

protected:
	struct TraceRecord;
	struct TraceBufferHeader;
	struct TraceBufferHeader *buf; ///< The trace buffer itself
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_TRACEBUFFER_TRACEBUFFER_H_ */
