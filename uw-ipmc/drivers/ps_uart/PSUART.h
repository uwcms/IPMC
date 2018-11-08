#ifndef UW_IPMC_DRIVERS_PS_UART_PSUART_H_
#define UW_IPMC_DRIVERS_PS_UART_PSUART_H_

#include "xuartps.h"
#include <FreeRTOS.h>
#include <semphr.h>
#include <libs/RingBuffer.h>
#include <drivers/generics/UART.h>
#include <drivers/InterruptBasedDriver.h>

/**
 * An interrupt based driver wrapper for the Zynq7000 PS UART devices.
 *
 * This driver will maintain an input and output buffer of user-specified
 * byte length.
 */
class PS_UART final : public UART, protected InterruptBasedDriver {
public:
	/**
	 * Instantiate a PS_UART driver.
	 *
	 * \note This performs hardware setup (mainly interrupt configuration).
	 *
	 * \param DeviceId    The DeviceId, used for XUartPs_LookupConfig(), etc
	 * \param IntrId      The interrupt ID, for configuring the GIC.
	 * \param ibufsize    The size of the input buffer to allocate.
	 * \param obufsize    The size of the output buffer to allocate.
	 */
	PS_UART(u32 DeviceId, u32 IntrId, u32 ibufsize = 4096, u32 obufsize = 4096);
	~PS_UART();

	size_t read(u8 *buf, size_t len, TickType_t timeout, TickType_t data_timeout=portMAX_DELAY);
	size_t write(const u8 *buf, size_t len, TickType_t timeout);
	bool clear();

	///! Returns the total number of errors.
	inline u32 getErrors() { return this->error_count; };

    PS_UART(PS_UART const &) = delete;               ///< Class is not assignable.
    void operator=(PS_UART const &x) = delete;       ///< Class is not copyable.

protected:
	volatile u32 error_count;       ///< Error counter containing accumulated errors from recent operations.
	XUartPs UartInst;      			///< The XUartPs handle of the driven device.
	RingBuffer<u8> inbuf;  			///< The input buffer.
	RingBuffer<u8> outbuf; 			///< The output buffer.
	WaitList readwait;     			///< A waitlist for blocking read operations.
	WaitList writewait;    			///< A waitlist for blocking write operations.
	void _InterruptHandler();		///< Internal interrupt handler for UART IRQ.

	void recv();					///< Internally called to receive to the ring buffer.
	void send();					///< Internally called to send from the ring buffer.
};

#endif /* UW_IPMC_DRIVERS_PS_UART_PSUART_H_ */
