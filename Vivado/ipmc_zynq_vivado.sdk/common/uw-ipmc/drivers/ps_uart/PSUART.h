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
	/**
	 * Instantiate a PS_UART driver.
	 *
	 * \note This performs hardware setup (mainly interrupt configuration).
	 *
	 * \param DeviceId  The DeviceId, used for XUartPs_LookupConfig(), etc
	 * \param IntrId    The interrupt ID, for configuring the GIC.
	 * \param ibufsize  The size of the input buffer to allocate.
	 * \param obufsize  The size of the output buffer to allocate.
	 */
	PS_UART(u32 DeviceId, u32 IntrId, u32 ibufsize=4096, u32 obufsize=4096);
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
	bool echo;                                       ///< Enable auto-echo (not implemented). TODO
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
};

#endif /* UW_IPMC_DRIVERS_PS_UART_PSUART_H_ */
