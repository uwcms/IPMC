/*
 * PSUART.cpp
 *
 *  Created on: Sep 15, 2017
 *      Author: jtikalsky
 */

#include "PSUART.h"
#include "IPMC.h"
#include "xscugic.h"
#include "xuartps.h"
#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>

#define IXR_RECV_ENABLE \
	(XUARTPS_IXR_TOUT | XUARTPS_IXR_PARITY | XUARTPS_IXR_FRAMING | \
	 XUARTPS_IXR_OVER | XUARTPS_IXR_RXFULL | XUARTPS_IXR_RXOVR)

#define enableRecvInterrupts() \
	XUartPs_WriteReg(this->UartInst.Config.BaseAddress, XUARTPS_IER_OFFSET, (u32)IXR_RECV_ENABLE);

#define disableRecvInterrupts() \
	XUartPs_WriteReg(this->UartInst.Config.BaseAddress, XUARTPS_IDR_OFFSET, (u32)IXR_RECV_ENABLE);

void PS_UART::recv()
{
	u32 CsrRegister;
	u32 ReceivedCount = 0U;
	u8 *DmaInPtr;
	size_t Items;

	// Request DMA pointer from ring buffer
	this->inbuf.setup_dma_input(&DmaInPtr, &Items);

	// Read the Channel Status Register to determine if there is any data in
	// the RX FIFO
	CsrRegister = XUartPs_ReadReg(this->UartInst.Config.BaseAddress, XUARTPS_SR_OFFSET);

	// Loop until there is no more data in RX FIFO or the specified
	// number of bytes has been received
	while ((ReceivedCount < Items) && ((CsrRegister & (u32)XUARTPS_SR_RXEMPTY) == (u32)0)) {
		DmaInPtr[ReceivedCount++] = XUartPs_ReadReg(this->UartInst.Config.BaseAddress, XUARTPS_FIFO_OFFSET);

		CsrRegister = XUartPs_ReadReg(this->UartInst.Config.BaseAddress, XUARTPS_SR_OFFSET);
	}

	// Report to DMA how many words were filled
	this->inbuf.notify_dma_input_occurred(ReceivedCount);
	this->readwait.wake(); // Wake read function if bytes were received successfully
}

void PS_UART::send()
{
	u32 SentCount = 0U;
	u8 *DmaOutPtr;
	size_t Items;

	// Request DMA pointer from ring buffer
	this->outbuf.setup_dma_output(&DmaOutPtr, &Items);

	// If the TX FIFO is full, send nothing.
	// Otherwise put bytes into the TX FIFO until it is full, or all of the
	// data has been put into the FIFO.
	while ((!XUartPs_IsTransmitFull(this->UartInst.Config.BaseAddress)) && (Items > SentCount)) {
		// Fill the FIFO from the buffer
		XUartPs_WriteReg(this->UartInst.Config.BaseAddress, XUARTPS_FIFO_OFFSET,
				((u32)DmaOutPtr[SentCount++]));
	}

	// Report back how many bytes were actually written to DMA
	this->outbuf.notify_dma_output_occurred(SentCount);

	// If interrupts are enabled as indicated by the receive interrupt, then
	// enable the TX FIFO empty interrupt, so further action can be taken
	// for this sending.
	if (!this->outbuf.empty()) {
		// There is still data to send, so re-enable TX empty interrupt
		// Unclear why base drivers has RX check here for TX, removed
		/*ImrRegister = XUartPs_ReadReg(this->UartInst.Config.BaseAddress, XUARTPS_IMR_OFFSET);
		if (((ImrRegister & XUARTPS_IXR_RXFULL) != (u32)0) ||
			((ImrRegister & XUARTPS_IXR_RXEMPTY) != (u32)0)||
			((ImrRegister & XUARTPS_IXR_RXOVR) != (u32)0)) {*/
			XUartPs_WriteReg(this->UartInst.Config.BaseAddress, XUARTPS_IER_OFFSET,
						   /*ImrRegister |*/ (u32)XUARTPS_IXR_TXEMPTY);
		//}
	}

	if (SentCount) {
		// Wake write function if bytes were sent successfully
		this->writewait.wake();
	}
}

void PS_UART::_InterruptHandler() {
	u32 IsrStatus;

	Xil_AssertVoid(this->UartInst.IsReady == XIL_COMPONENT_IS_READY);

	// Read the interrupt ID register to determine which interrupt is active
	IsrStatus = XUartPs_ReadReg(this->UartInst.Config.BaseAddress, XUARTPS_IMR_OFFSET);
	IsrStatus &= XUartPs_ReadReg(this->UartInst.Config.BaseAddress, XUARTPS_ISR_OFFSET);

	// Clear interrupts before any operation to prevent driver locking, which has been seen!
	XUartPs_WriteReg(this->UartInst.Config.BaseAddress, XUARTPS_ISR_OFFSET, IsrStatus);

	// Dispatch an appropriate handler
	if((IsrStatus & ((u32)XUARTPS_IXR_RXOVR | (u32)XUARTPS_IXR_RXEMPTY |
			(u32)XUARTPS_IXR_RXFULL | (u32)XUARTPS_IXR_TOUT)) != (u32)0) {
		// If there are bytes still to be received in the specified buffer go ahead and
		// receive them. Removing bytes from the RX FIFO will clear the interrupt.
		if (!this->inbuf.full()) {
			this->recv();
		}
	}

	if((IsrStatus & (u32)XUARTPS_IXR_TXEMPTY) != (u32)0) {
		// If there are not bytes to be sent from the specified buffer then disable the transmit
		// interrupt so it will stop interrupting as it interrupts any time the FIFO is empty
		if (this->outbuf.empty()) {
			// There is nothing else in the transmit buffer to send
			// Disable interrupt so it doesn't keep triggering
			XUartPs_WriteReg(this->UartInst.Config.BaseAddress, XUARTPS_IDR_OFFSET,
					(u32)XUARTPS_IXR_TXEMPTY);
		} else {
			// If FIFO is empty and there is data to send then keep doing so
			this->send();
		}
	}

	// XUARTPS_IXR_RBRK is applicable only for Zynq Ultrascale+ MP
	if ((IsrStatus & ((u32)XUARTPS_IXR_OVER | (u32)XUARTPS_IXR_FRAMING |
			(u32)XUARTPS_IXR_PARITY | (u32)XUARTPS_IXR_RBRK)) != (u32)0) {
		// Some errors, increase the error counter
		this->error_count++;

		// If there are bytes still to be received in the specified buffer go ahead and
		// receive them. Removing bytes from the RX FIFO will clear the interrupt.
		if (!this->inbuf.full()) {
			this->recv();
		}
	}

	if((IsrStatus & ((u32)XUARTPS_IXR_DMS)) != (u32)0) {
		// Modem status interrupt not supported, just read to clean status
		XUartPs_ReadReg(this->UartInst.Config.BaseAddress, XUARTPS_MODEMSR_OFFSET);
	}
}

PS_UART::PS_UART(u32 DeviceId, u32 IntrId, u32 ibufsize, u32 obufsize) :
		InterruptBasedDriver(IntrId), error_count(0),
		inbuf(RingBuffer<u8>(ibufsize)), outbuf(RingBuffer<u8>(obufsize)) {
	XUartPs_Config *Config = XUartPs_LookupConfig(DeviceId);
	s32 Status = XUartPs_CfgInitialize(&this->UartInst, Config, Config->BaseAddress);
	configASSERT(Status == XST_SUCCESS);

	XUartPs_SetInterruptMask(&this->UartInst, 0);
	XUartPs_SetRecvTimeout(&this->UartInst, 8); // My example says this is u32s, the comments say its nibbles, the TRM says its baud_sample_clocks.

	// Enable receive interrupts
	enableRecvInterrupts();
}

PS_UART::~PS_UART() {
	// Disable receive interrupts
	disableRecvInterrupts();
}

size_t PS_UART::read(u8 *buf, size_t len, TickType_t timeout, TickType_t data_timeout) {
	configASSERT(timeout == 0 || !(IN_INTERRUPT() || IN_CRITICAL()));

	AbsoluteTimeout abstimeout(timeout);
	AbsoluteTimeout abs_data_timeout(data_timeout);
	size_t bytesread = 0;
	while (bytesread < len) {
		/* We join the readwait queue now, because if we did this later we would
		 * have problems with a race condition between the read attempt and
		 * starting the wait.  We can cancel it later with a wait(timeout=0).
		 */
		WaitList::Subscription sub;
		if (!IN_INTERRUPT())
			sub = this->readwait.join();

		/* Entering a critical section to strongly pair the read and interrupt
		 * re-enable.
		 */
		if (!IN_INTERRUPT())
			this->disableInterrupts();

		size_t batch_bytesread = this->inbuf.read(buf+bytesread, len-bytesread);
		if (batch_bytesread) {
			/* We have retrieved SOMETHING from the buffer.  Re-enable receive
			 * interrupts, in case they were disabled due to a full buffer.
			 */
			enableRecvInterrupts();
		}
		bytesread += batch_bytesread;

		if (!IN_INTERRUPT())
			this->enableInterrupts();
		// </critical>

		if (IN_INTERRUPT())
			break; // Interrupts can't wait for more.
		if (bytesread == len)
			break;
		if (bytesread && abs_data_timeout < abstimeout)
			abstimeout = abs_data_timeout; // We have data, so if we have a data_timeout, we're now on it instead.
		if (!sub.wait(abstimeout.get_timeout()))
			break; // Timed out.
	}
	return bytesread;
}

size_t PS_UART::write(const u8 *buf, size_t len, TickType_t timeout) {
	configASSERT(timeout == 0 || !(IN_INTERRUPT() || IN_CRITICAL()));

	AbsoluteTimeout abstimeout(timeout);
	size_t byteswritten = 0;
	while (byteswritten < len) {
		/* We join the writewait queue now, because if we did this later we
		 * would have problems with a race condition between the write attempt
		 * and starting the wait.  We can cancel it later with a
		 * wait(timeout=0).
		 */
		WaitList::Subscription sub;
		if (!IN_INTERRUPT())
			sub = this->writewait.join();

		/* Entering a critical section to strongly pair the write and output
		 * refresh.
		 */
		if (!IN_INTERRUPT())
			this->disableInterrupts();

		size_t batch_byteswritten = this->outbuf.write(buf+byteswritten, len-byteswritten);
		if (batch_byteswritten) {
			// Trigger a send if the TX FIFO is empty
			u32 ImrRegister = XUartPs_ReadReg(this->UartInst.Config.BaseAddress, XUARTPS_IMR_OFFSET);

			if (!(ImrRegister & (XUARTPS_IXR_TXEMPTY))) {
				this->send();
			}
		}
		byteswritten += batch_byteswritten;

		if (!IN_INTERRUPT())
			this->enableInterrupts();
		// </critical>

		if (IN_INTERRUPT())
			break; // Interrupts can't wait for more.
		if (byteswritten == len)
			break;
		if (!sub.wait(abstimeout.get_timeout()))
			break; // Timed out.
	}
	return byteswritten;
}

bool PS_UART::clear() {
	this->disableInterrupts();
	this->inbuf.reset();
	this->enableInterrupts();

	return true;
}
