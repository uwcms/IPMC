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
class PS_UART : public UART, protected InterruptBasedDriver {
public:
	PS_UART(u32 DeviceId, u32 IntrId, u32 ibufsize=4096, u32 obufsize=4096, u32 oblocksize=128);
	virtual size_t read(u8 *buf, size_t len, TickType_t timeout, TickType_t data_timeout=portMAX_DELAY);
	/// \overload
	virtual size_t read(char *buf, size_t len, TickType_t timeout, TickType_t data_timeout=portMAX_DELAY) { return this->read(reinterpret_cast<u8*>(buf), len, timeout, data_timeout); };
	virtual size_t write(const u8 *buf, size_t len, TickType_t timeout);
	/// \overload
	virtual size_t write(const char *buf, size_t len, TickType_t timeout) { return this->write(reinterpret_cast<const u8*>(buf), len, timeout); }
	virtual bool clear();
	virtual ~PS_UART();
	virtual void _HandleInterrupt(u32 Event, u32 EventData); ///< \protected Internal.

    PS_UART(PS_UART const &) = delete;               ///< Class is not assignable.
    void operator=(PS_UART const &x) = delete;       ///< Class is not copyable.
protected:
	u32 error_mask;        ///< Error mask containing accumulated errors from recent operations. TODO: allow read.
	XUartPs UartInst;      ///< The XUartPs handle of the driven device.
	RingBuffer<u8> inbuf;  ///< The input buffer.
	RingBuffer<u8> outbuf; ///< The output buffer.
	WaitList readwait;     ///< A waitlist for blocking read operations.
	WaitList writewait;    ///< A waitlist for blocking write operations.
	u32 oblocksize;        ///< The maximum block size for output operations.  Relevant to wait times when the queue is full.
	void _InterruptHandler();

	u32 XUartPs_Recv(XUartPs *InstancePtr);
	u32 XUartPs_Send(XUartPs *InstancePtr);

	u32 XUartPs_ReceiveBuffer(XUartPs *InstancePtr);
	u32 XUartPs_SendBuffer(XUartPs *InstancePtr);
};

#endif /* UW_IPMC_DRIVERS_PS_UART_PSUART_H_ */
