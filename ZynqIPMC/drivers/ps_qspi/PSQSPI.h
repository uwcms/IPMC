/*
 * PSQSPI.h
 *
 *  Created on: Mar 19, 2019
 *      Author: mpv
 */

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_PS_QSPI_PSQSPI_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_PS_QSPI_PSQSPI_H_

#include "xqspips.h"

#include <FreeRTOS.h>
#include <semphr.h>
#include <queue.h>
#include <IPMC.h>
#include <drivers/generics/SPI.h>
#include <drivers/interrupt_based_driver.h>

/**
 * An interrupt-based driver for the PS QSPI.
 */
class PS_QSPI : public SPIMaster, protected InterruptBasedDriver {
public:
	PS_QSPI(u16 DeviceId, u32 IntrId);
	virtual ~PS_QSPI();

	bool transfer(uint8_t chip, const uint8_t *sendbuf, uint8_t *recvbuf, size_t bytes, TickType_t timeout);
	bool transfer_unsafe(const uint8_t *sendbuf, uint8_t *recvbuf, size_t bytes, TickType_t timeout);
	inline bool isQuadSupported() { return true; };

	void select(uint32_t cs);
	void deselect();

	PS_QSPI(PS_QSPI const &) = delete;               ///< Class is not assignable.
    void operator=(PS_QSPI const &x) = delete;       ///< Class is not copyable.

protected:
    QueueHandle_t sync;				///< Internal use. Synchronizes interrupts with waiting threads.
    volatile bool selected;			///< Internal use. Indicates when the slave is selected.
    volatile bool started;			///< Internal use. Indicates when the transfer has started and the instruction OP has been sent.
    void _InterruptHandler();

	XQspiPs QspiInst;               ///< The XQspiPs handle of the driven device.
	enum Mode {
		SINGLE, FAST, DUAL, QUAD
	} OpMode;
};



#endif /* SRC_COMMON_UW_IPMC_DRIVERS_PS_QSPI_PSQSPI_H_ */
