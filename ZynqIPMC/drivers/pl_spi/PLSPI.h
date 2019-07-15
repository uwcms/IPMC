/*
 * PLSPI.h
 *
 *  Created on: Aug 17, 2018
 *      Author: mpv
 */

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_PL_SPI_PLSPI_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_PL_SPI_PLSPI_H_

#if XSDK_INDEXING || __has_include(<xspi.h>)
#define PLSPI_DRIVER_INCLUDED

#include <xspi.h>
#include <drivers/generics/SPI.h>
#include <drivers/interrupt_based_driver.h>
#include <queue.h>

// TODO: Consider supporting dual or quad SPI as well by default.

/**
 * Driver for Xilinx AXI QSPI IP.
 * Only single mode and master is supported at this stage.
 */
class PL_SPI : public SPIMaster, protected InterruptBasedDriver {
public:
	/**
	 * Constructs the PL QSPI driver.
	 * @param DeviceId AXI QSPI device ID of target IP.
	 * @param IntrId The interrupt ID associated with the IP.
	 */
	PL_SPI(uint16_t DeviceId, uint32_t IntrId);
	virtual ~PL_SPI();

	bool transfer(u8 cs, const u8 *sendbuf, u8 *recvbuf, size_t bytes, TickType_t timeout);
	bool transfer_unsafe(const u8 *sendbuf, u8 *recvbuf, size_t bytes, TickType_t timeout);
	inline bool isQuadSupported() { return false; };

	void select(uint32_t cs);
	void deselect();

private:
	XSpi xspi;	///< Internal use only. Xilinx SPI structure.

	///! Used to activate the SPI bus, useful when the multiple masters share the bus.
	void start();

	///! Used to deactivate the SPI bus, allowing other masters to also drive the bus.
	void stop();

	QueueHandle_t sync;			///< Internal use. Synchronizes interrupts with waiting threads.
	void _InterruptHandler();
};

#endif

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_PL_SPI_PLSPI_H_ */
