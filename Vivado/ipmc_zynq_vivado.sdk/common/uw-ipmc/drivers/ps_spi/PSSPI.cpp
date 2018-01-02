/*
 * PSSPI.cpp
 *
 *  Created on: Jan 2, 2018
 *      Author: jtikalsky
 */

#include <drivers/ps_spi/PSSPI.h>

PS_SPI::PS_SPI(u16 DeviceId, u32 IntrId) {
	// TODO: Write

}

PS_SPI::~PS_SPI() {
	// TODO: Write
}

/**
 * This function will perform a SPI transfer in a blocking manner.
 *
 * \param chip     The chip select to enable.
 * \param sendbuf  The data to send.
 * \param recvbuf  A buffer for received data.
 * \param bytes    The number of bytes to transfer
 * \return false on error, else true
 */
bool PS_SPI::transfer(u8 chip, u8 *sendbuf, u8 *recvbuf, size_t bytes) {
	configASSERT(0);
	return false;
}
