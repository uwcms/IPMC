/*
 * This file is part of the ZYNQ-IPMC Framework.
 *
 * The ZYNQ-IPMC Framework is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The ZYNQ-IPMC Framework is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the ZYNQ-IPMC Framework.  If not, see <https://www.gnu.org/licenses/>.
 */

// Only include driver if PL UART in the BSP.
#if XSDK_INDEXING || __has_include("xuartlite.h")

#include "pl_uart.h"
#include "xuartlite_l.h"
#include <stdio.h>
#include <libs/except.h>

void PLUART::_InterruptHandler()
{
	// Note: Interrupts won't be disabled while running this interrupt.
	// If nesting is enabled that this cause problems.

	this->recv();
	this->send();

	// TODO: Do error detection, inc. streambuffer full
}

void PLUART::recv() {
	// Read status register
	uint8_t status_reg = XUartLite_GetStatusReg(this->uartlite.RegBaseAddress);

	if ((status_reg & (XUL_SR_RX_FIFO_FULL | XUL_SR_RX_FIFO_VALID_DATA)) != 0) {
		uint8_t *ptr = nullptr;
		size_t max_bytes = 0;
		size_t byte_count = 0;

		// Request DMA pointer from ring buffer
		this->inbuf.setup_dma_input(&ptr, &max_bytes);

		do {
			uint8_t data = XUartLite_ReadReg(this->uartlite.RegBaseAddress, XUL_RX_FIFO_OFFSET);

			if ((max_bytes != 0) && (max_bytes == byte_count)) {
				// DMA completely filled but we still have data to read, attempt to get a new DMA pointer
				// If there is no space in ring buffer maxBytes will be zero and it will still flush the RX FIFO
				this->inbuf.notify_dma_input_occurred(byte_count);

				this->inbuf.setup_dma_input(&ptr, &max_bytes);
				byte_count = 0;
			}

			if (max_bytes > byte_count) {
				ptr[byte_count++] = data;
			}

			status_reg = XUartLite_GetStatusReg(this->uartlite.RegBaseAddress);
		} while ((status_reg & (XUL_SR_RX_FIFO_FULL | XUL_SR_RX_FIFO_VALID_DATA)) != 0);

		// Report to DMA how many words were filled
		this->inbuf.notify_dma_input_occurred(byte_count);

		this->readwait.wake(); // Wake read function if bytes were received successfully
	}
}

void PLUART::send() {
	// Read status register
	uint8_t status_reg = XUartLite_GetStatusReg(this->uartlite.RegBaseAddress);

	if (((status_reg & XUL_SR_TX_FIFO_EMPTY) != 0) && !this->outbuf.empty()) {
		uint8_t *ptr = nullptr;
		size_t max_bytes = 0;
		size_t byte_count = 0;

		// Request DMA pointer from ring buffer
		this->outbuf.setup_dma_output(&ptr, &max_bytes);

		do {
			if (byte_count == max_bytes) {
				// Possible DMA buffer roll
				this->outbuf.notify_dma_output_occurred(byte_count);
				byte_count = 0;

				if (this->outbuf.empty()) break;

				this->outbuf.setup_dma_output(&ptr, &max_bytes);
			}

			XUartLite_WriteReg(this->uartlite.RegBaseAddress, XUL_TX_FIFO_OFFSET, ptr[byte_count++]);

			status_reg = XUartLite_GetStatusReg(this->uartlite.RegBaseAddress);
		} while ((status_reg & XUL_SR_TX_FIFO_EMPTY) != 0);

		// Report back how many bytes were actually written to DMA
		if (byte_count)
			this->outbuf.notify_dma_output_occurred(byte_count);

		this->writewait.wake();  // Wake write function if bytes were sent successfully
	}
}

PLUART::PLUART(uint16_t device_id, uint16_t intr_id, size_t ibufsize, size_t obufsize) :
InterruptBasedDriver(intr_id, 0x3), inbuf(RingBuffer<uint8_t>(ibufsize)), outbuf(RingBuffer<uint8_t>(obufsize)) {
	// Initialize the UartLite driver so that it's ready to use.
	if (XST_SUCCESS != XUartLite_Initialize(&(this->uartlite), device_id)) {
		throw except::hardware_error("Unable to initialize PLUART(device_id=" + std::to_string(device_id) + ")");
	}

	// Perform a self-test to ensure that the hardware was built correctly.
	if (XST_SUCCESS != XUartLite_SelfTest(&(this->uartlite))) {
		throw except::hardware_error("Self-test failed for PLUART(device_id=" + std::to_string(device_id) + ")");
	}

	// Enable the interrupt of UartLite so that interrupts will occur.
	this->enableInterrupts();
	XUartLite_EnableInterrupt(&(this->uartlite));
}

PLUART::~PLUART() {
}

size_t PLUART::read(uint8_t *buf, size_t len, TickType_t timeout, TickType_t data_timeout) {
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


		if (bytesread == len) break;
		if (bytesread && abs_data_timeout < abstimeout)
			abstimeout = abs_data_timeout; // We have data, so if we have a data_timeout, we're now on it instead.
		if (!sub.wait(abstimeout.get_timeout())) break; // Timed out.
	}
	return bytesread;
}

size_t PLUART::write(const uint8_t *buf, size_t len, TickType_t timeout) {
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

			this->send();

			this->enableInterrupts();
		}
		byteswritten += batch_byteswritten;

		if (byteswritten == len) break;
		if (!sub.wait(abstimeout.get_timeout())) break; // Timed out.
	}
	return byteswritten;
}

bool PLUART::clear() {
	this->disableInterrupts();
	this->inbuf.reset();
	this->enableInterrupts();

	return true;
}

#endif

