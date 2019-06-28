/*
 * PLUART.cpp
 *
 *  Created on: Jul 3, 2018
 *      Author: mpv
 */

#include "PLUART.h"

#if XSDK_INDEXING || __has_include("xuartlite.h")

#include <xscugic.h>
#include <stdio.h>
#include "xil_exception.h"
#include "xuartlite_l.h"
#include <libs/printf.h>
#include <libs/except.h>
#include <libs/ThreadingPrimitives.h>

void PL_UART::_InterruptHandler()
{
	// Note: Interrupts won't be disabled while running this interrupt.
	// If nesting is enabled that this cause problems.

	this->_Recv();
	this->_Send();

	// TODO: Do error detection, inc. streambuffer full
}

void PL_UART::_Recv() {
	// Read status register
	uint8_t StatusRegister = XUartLite_GetStatusReg(this->UartLite.RegBaseAddress);

	if ((StatusRegister & (XUL_SR_RX_FIFO_FULL | XUL_SR_RX_FIFO_VALID_DATA)) != 0) {
		u8 *ptr;
		size_t maxBytes;
		size_t byteCount = 0;

		// Request DMA pointer from ring buffer
		this->inbuf.setup_dma_input(&ptr, &maxBytes);

		do {
			uint8_t data = XUartLite_ReadReg(this->UartLite.RegBaseAddress, XUL_RX_FIFO_OFFSET);

			if ((maxBytes != 0) && (maxBytes == byteCount)) {
				// DMA completely filled but we still have data to read, attempt to get a new DMA pointer
				// If there is no space in ring buffer maxBytes will be zero and it will still flush the RX FIFO
				this->inbuf.notify_dma_input_occurred(byteCount);

				this->inbuf.setup_dma_input(&ptr, &maxBytes);
				byteCount = 0;
			}

			if (maxBytes > byteCount) {
				ptr[byteCount++] = data;
			}

			StatusRegister = XUartLite_GetStatusReg(this->UartLite.RegBaseAddress);
		} while ((StatusRegister & (XUL_SR_RX_FIFO_FULL | XUL_SR_RX_FIFO_VALID_DATA)) != 0);

		// Report to DMA how many words were filled
		this->inbuf.notify_dma_input_occurred(byteCount);

		this->readwait.wake(); // Wake read function if bytes were received successfully
	}
}

void PL_UART::_Send() {
	// Read status register
	uint8_t StatusRegister = XUartLite_GetStatusReg(this->UartLite.RegBaseAddress);

	if (((StatusRegister & XUL_SR_TX_FIFO_EMPTY) != 0) && !this->outbuf.empty()) {
		u8 *ptr;
		size_t maxBytes;
		size_t byteCount = 0;

		// Request DMA pointer from ring buffer
		this->outbuf.setup_dma_output(&ptr, &maxBytes);

		do {
			if (byteCount == maxBytes) {
				// Possible DMA buffer roll
				this->outbuf.notify_dma_output_occurred(byteCount);
				byteCount = 0;

				if (this->outbuf.empty()) break;

				this->outbuf.setup_dma_output(&ptr, &maxBytes);
			}

			XUartLite_WriteReg(this->UartLite.RegBaseAddress, XUL_TX_FIFO_OFFSET, ptr[byteCount++]);

			StatusRegister = XUartLite_GetStatusReg(this->UartLite.RegBaseAddress);
		} while ((StatusRegister & XUL_SR_TX_FIFO_EMPTY) != 0);

		// Report back how many bytes were actually written to DMA
		if (byteCount)
			this->outbuf.notify_dma_output_occurred(byteCount);

		this->writewait.wake();  // Wake write function if bytes were sent successfully
	}
}

PL_UART::PL_UART(uint16_t DeviceId, uint32_t IntrId, size_t ibufsize, size_t obufsize) :
InterruptBasedDriver(IntrId, 0x3), inbuf(RingBuffer<u8>(ibufsize)), outbuf(RingBuffer<u8>(obufsize)) {
	// Initialize the UartLite driver so that it's ready to use.
	if (XST_SUCCESS != XUartLite_Initialize(&(this->UartLite), DeviceId))
		throw except::hardware_error(stdsprintf("Unable to initialize PL_UART(%hu, %lu, ...)", DeviceId, IntrId));

	// Perform a self-test to ensure that the hardware was built correctly.
	if (XST_SUCCESS != XUartLite_SelfTest(&(this->UartLite)))
		throw except::hardware_error(stdsprintf("Self-test failed for PL_UART(%hu, %lu, ...)", DeviceId, IntrId));

	// Enable the interrupt of UartLite so that interrupts will occur.
	this->enableInterrupts();
	XUartLite_EnableInterrupt(&(this->UartLite));
}

PL_UART::~PL_UART() {
}

size_t PL_UART::read(u8 *buf, size_t len, TickType_t timeout, TickType_t data_timeout) {
	configASSERT(!(IN_INTERRUPT() || IN_CRITICAL())); // Don't allow in interrupts and critical regions

	AbsoluteTimeout abstimeout(timeout);
	AbsoluteTimeout abs_data_timeout(data_timeout);
	size_t bytesread = 0;
	while (bytesread < len) {
		/* We join the readwait queue now, because if we did this later we would
		 * have problems with a race condition between the read attempt and
		 * starting the wait.  We can cancel it later with a wait(timeout=0).
		 */
		WaitList::Subscription sub;
		sub = this->readwait.join();

		/* Entering a critical section to strongly pair the read and interrupt
		 * re-enable.
		 */
		this->disableInterrupts();

		size_t batch_bytesread = this->inbuf.read(buf+bytesread, len-bytesread);

		this->enableInterrupts();

		bytesread += batch_bytesread;


		if (bytesread == len)
			break;
		if (bytesread && abs_data_timeout < abstimeout)
			abstimeout = abs_data_timeout; // We have data, so if we have a data_timeout, we're now on it instead.
		if (!sub.wait(abstimeout.get_timeout()))
			break; // Timed out.
	}
	return bytesread;
}

size_t PL_UART::write(const u8 *buf, size_t len, TickType_t timeout) {
	configASSERT(!(IN_INTERRUPT() || IN_CRITICAL())); // Don't allow in interrupts and critical regions

	AbsoluteTimeout abstimeout(timeout);
	size_t byteswritten = 0;
	while (byteswritten < len) {
		/* We join the writewait queue now, because if we did this later we
		 * would have problems with a race condition between the write attempt
		 * and starting the wait.  We can cancel it later with a
		 * wait(timeout=0).
		 */
		WaitList::Subscription sub;
		sub = this->writewait.join();

		size_t batch_byteswritten = this->outbuf.write(buf+byteswritten, len-byteswritten);
		if (batch_byteswritten) {
			this->disableInterrupts();

			this->_Send();

			this->enableInterrupts();
		}
		byteswritten += batch_byteswritten;

		if (byteswritten == len)
			break;
		if (!sub.wait(abstimeout.get_timeout()))
			break; // Timed out.
	}
	return byteswritten;
}

bool PL_UART::clear() {
	this->disableInterrupts();
	this->inbuf.reset();
	this->enableInterrupts();

	return true;
}

#endif

