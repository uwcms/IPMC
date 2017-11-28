#ifndef UW_IPMC_DRIVERS_PS_UART_PSUART_H_
#define UW_IPMC_DRIVERS_PS_UART_PSUART_H_

#include "xuartps.h"
#include <FreeRTOS.h>
#include <semphr.h>
#include <libs/RingBuffer.h>
#include <libs/ThreadingPrimitives.h>

/**
 * An interrupt based driver wrapper for the Zynq7000 PS UART devices.
 *
 * This driver will maintain an input and output buffer of user-specified
 * byte length.
 */
class PS_UART {
public:
	PS_UART(u32 DeviceId, u32 IntrId, u32 ibufsize=4096, u32 obufsize=4096, u32 oblocksize=128);
	size_t read(u8 *buf, size_t len, TickType_t timeout);
	/**
	 * \overload
	 */
	size_t read(char *buf, size_t len, TickType_t timeout) { return this->read(reinterpret_cast<u8*>(buf), len, timeout); };
	size_t write(const u8 *buf, size_t len, TickType_t timeout);
	/**
	 * \overload
	 */
	size_t write(const char *buf, size_t len, TickType_t timeout) { return this->write(reinterpret_cast<const u8*>(buf), len, timeout); }
	virtual ~PS_UART();
	void _HandleInterrupt(u32 Event, u32 EventData); ///< \protected Internal.
    PS_UART(PS_UART const &) = delete;               ///< Class is not assignable.
    void operator=(PS_UART const &x) = delete;       ///< Class is not copyable.
protected:
	u32 error_mask;        ///< Error mask containing accumulated errors from recent operations. TODO: allow read.
	XUartPs UartInst;      ///< The XUartPs handle of the driven device.
	RingBuffer<u8> inbuf;  ///< The input buffer.
	RingBuffer<u8> outbuf; ///< The output buffer.
	WaitList readwait;     ///< A waitlist for blocking read operations.
	WaitList writewait;    ///< A waitlist for blocking write operations.
	volatile bool write_running; ///< Indicates whether a interrupt-driven write is in progress.
	u32 oblocksize;        ///< The maximum block size for output operations.  Relevant to wait times when the queue is full.
	u32 IntrId;            ///< Interrupt ID, used to disable interrupts in the destructor.
};

#endif /* UW_IPMC_DRIVERS_PS_UART_PSUART_H_ */
