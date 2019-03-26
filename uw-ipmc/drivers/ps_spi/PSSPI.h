/*
 * PSSPI.h
 *
 *  Created on: Jan 2, 2018
 *      Author: jtikalsky
 */

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_PS_SPI_PSSPI_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_PS_SPI_PSSPI_H_

#include "xspips.h"

#include <FreeRTOS.h>
#include <semphr.h>
#include <queue.h>
#include <IPMC.h>
#include <drivers/generics/SPI.h>

/**
 * An interrupt-based driver for the PS SPI.
 */
class PS_SPI : public SPIMaster {
public:
	PS_SPI(u16 DeviceId, u32 IntrId);
	virtual ~PS_SPI();

	bool transfer(u8 chip, const u8 *sendbuf, u8 *recvbuf, size_t bytes, TickType_t timeout);
	bool transfer_unsafe(const u8 *sendbuf, u8 *recvbuf, size_t bytes, TickType_t timeout);
	inline bool isQuadSupported() { return false; };

	void select(uint32_t cs);
	void deselect();

	void _HandleInterrupt(u32 Event, u32 EventData); ///< \protected Internal.
	PS_SPI(PS_SPI const &) = delete;               ///< Class is not assignable.
    void operator=(PS_SPI const &x) = delete;       ///< Class is not copyable.

protected:
	u32 error_not_done;             ///< Error containing accumulated errors of non completed transfers. TODO: allow read.
	u32 error_byte_count;           ///< Error containing accumulated errors of byte count mismatches. TODO: allow read.
	XSpiPs SpiInst;                 ///< The XSpiPs handle of the driven device.
	QueueHandle_t irq_sync_q;       ///< IRQ-task syncronization queue
	volatile bool transfer_running; ///< Indicates whether an interrupt-driven transfer is in progress.
	u32 IntrId;                     ///< Interrupt ID, used to disable interrupts in the destructor.

	/**
	 * irq transfer status
	 */
	struct trans_st_t
	{
		u32 byte_count;    ///< transfer byte count provided to irq handler by xilinx ps_spi drive
		u32 event_status;  ///< transfer status provided to irq handler by  xilinx ps_spi driver
	};
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_PS_SPI_PSSPI_H_ */
