/*
 * XilinxImage.h
 *
 *  Created on: Aug 7, 2018
 *      Author: mpv
 */

#ifndef SRC_COMMON_UW_IPMC_LIBS_XILINXIMAGE_H_
#define SRC_COMMON_UW_IPMC_LIBS_XILINXIMAGE_H_

#include <stddef.h>
#include <stdint.h>

///! Uncomment to enabled debugging
#define XILINXIMAGE_DEBUG

/**
 * Validates a given BIN file in accordance with UG585, UG821 and UG470.
 * @param binfile A buffer in memory that holds the BIN raw data.
 * @param size The total number of bytes of the BIN file.
 * @return true if the image passed all checks, false otherwise.
 */
bool validateBootFile(uint8_t *binfile, size_t size);

#endif /* SRC_COMMON_UW_IPMC_LIBS_XILINXIMAGE_H_ */
