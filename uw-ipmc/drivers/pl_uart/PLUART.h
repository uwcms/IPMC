/*
 * PLUART.h
 *
 *  Created on: Jul 3, 2018
 *      Author: mpv
 */

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_PL_UART_PLUART_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_PL_UART_PLUART_H_

#include <stdio.h>
#include <FreeRTOS.h>
#include <semphr.h>
#include <stream_buffer.h>
#include <drivers/generics/UART.h>
#include "xuartlite.h"

// TODO: Remove UartLite dependencies, which is barely used and is just consuming space

/**
 * A FreeRTOS interrupt based driver for Xilinx UartLite IP core.
 */
class PL_UART : public UART {
public:
	/**
	 * Create a PL based UART interface
	 * @param DeviceId The device ID, normally AXI_UARTLITE_<>
	 * @param IntrId The device interrupt ID from GIC, normally XPAR_FABRIC_AXI_UARTLITE_<>
	 * @param ibufsize The input buffer size
	 * @param obufsize The output buffer size
	 */
	PL_UART(uint16_t DeviceId, uint32_t IntrId, size_t ibufsize=4096, size_t obufsize=4096);
	virtual ~PL_UART();

	size_t read(u8 *buf, size_t len, TickType_t timeout=portMAX_DELAY, TickType_t data_timeout=portMAX_DELAY);
	size_t write(const u8 *buf, size_t len, TickType_t timeout=portMAX_DELAY);

	/**
	 * Clear the receiver buffer
	 * @return false if there is a thread waiting for data
	 */
	bool clear();

	//FILE* getFileDesc();
//private:
	 XUartLite UartLite; ///< Internal use only.
	 uint32_t IntrId; ///< Internal use only.

	 StreamBufferHandle_t recvstream; ///< Internal use only.
	 StreamBufferHandle_t sendstream; ///< Internal use only.
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_PL_UART_PLUART_H_ */
