#ifndef CRC32_H
#define CRC32_H

#include <stdint.h>
#include <stddef.h>

void crc32(const void *data, size_t n_bytes, uint32_t *crc);

#endif
