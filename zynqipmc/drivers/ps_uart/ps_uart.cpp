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

// Only include driver if PS UART in the BSP.
#if XSDK_INDEXING || __has_include("xuartps.h")

#include <drivers/ps_uart/ps_uart.h>
#include <string>
#include <libs/except.h>

#define IXR_RECV_ENABLE \
	(XUARTPS_IXR_TOUT | XUARTPS_IXR_PARITY | XUARTPS_IXR_FRAMING | \
	 XUARTPS_IXR_OVER | XUARTPS_IXR_RXFULL | XUARTPS_IXR_RXOVR)

#define enableRecvInterrupts() \
	XUartPs_WriteReg(this->uartps.Config.BaseAddress, XUARTPS_IER_OFFSET, (uint32_t)IXR_RECV_ENABLE);

#define disableRecvInterrupts() \
	XUartPs_WriteReg(this->uartps.Config.BaseAddress, XUARTPS_IDR_OFFSET, (uint32_t)IXR_RECV_ENABLE);

void PSUART::recv()
{
	uint32_t csr_reg = 0;
	size_t recv_count = 0;
	uint8_t *dmaptr = nullptr;
	size_t items = 0;

	// Request DMA pointer from ring buffer
	this->inbuf.setupDMAInput(&dmaptr, &items);

	// Read the Channel Status Register to determine if there is any data in
	// the RX FIFO
	csr_reg = XUartPs_ReadReg(this->uartps.Config.BaseAddress, XUARTPS_SR_OFFSET);

	// Loop until there is no more data in RX FIFO or the specified
	// number of bytes has been received
	while ((recv_count < items) && ((csr_reg & (uint32_t)XUARTPS_SR_RXEMPTY) == (uint32_t)0)) {
		dmaptr[recv_count++] = XUartPs_ReadReg(this->uartps.Config.BaseAddress, XUARTPS_FIFO_OFFSET);

		csr_reg = XUartPs_ReadReg(this->uartps.Config.BaseAddress, XUARTPS_SR_OFFSET);
	}

	// Report to DMA how many words were filled
	this->inbuf.notifyDMAInputOccurred(recv_count);
	this->readwait.wake(); // Wake read function if bytes were received successfully
}

void PSUART::send()
{
	size_t send_count = 0;
	uint8_t *dmaptr = nullptr;
	size_t items = 0;

	// Request DMA pointer from ring buffer
	this->outbuf.setupDMAOutput(&dmaptr, &items);

	// If the TX FIFO is full, send nothing.
	// Otherwise put bytes into the TX FIFO until it is full, or all of the
	// data has been put into the FIFO.
	while ((!XUartPs_IsTransmitFull(this->uartps.Config.BaseAddress)) && (items > send_count)) {
		// Fill the FIFO from the buffer
		XUartPs_WriteReg(this->uartps.Config.BaseAddress, XUARTPS_FIFO_OFFSET,
				((uint32_t)dmaptr[send_count++]));
	}

	// Report back how many bytes were actually written to DMA
	this->outbuf.notifyDMAOutputOccurred(send_count);

	// If interrupts are enabled as indicated by the receive interrupt, then
	// enable the TX FIFO empty interrupt, so further action can be taken
	// for this sending.
	if (!this->outbuf.empty()) {
		// There is still data to send, so re-enable TX empty interrupt
		// Unclear why base drivers has RX check here for TX, removed
		/*ImrRegister = XUartPs_ReadReg(this->uartps.Config.BaseAddress, XUARTPS_IMR_OFFSET);
		if (((ImrRegister & XUARTPS_IXR_RXFULL) != (uint32_t)0) ||
			((ImrRegister & XUARTPS_IXR_RXEMPTY) != (uint32_t)0)||
			((ImrRegister & XUARTPS_IXR_RXOVR) != (uint32_t)0)) {*/
			XUartPs_WriteReg(this->uartps.Config.BaseAddress, XUARTPS_IER_OFFSET,
						   /*ImrRegister |*/ (uint32_t)XUARTPS_IXR_TXEMPTY);
		//}
	}

	if (send_count) {
		// Wake write function if bytes were sent successfully
		this->writewait.wake();
	}
}

void PSUART::_InterruptHandler() {
	uint32_t isr_status;

	Xil_AssertVoid(this->uartps.IsReady == XIL_COMPONENT_IS_READY);

	// Read the interrupt ID register to determine which interrupt is active
	isr_status = XUartPs_ReadReg(this->uartps.Config.BaseAddress, XUARTPS_IMR_OFFSET);
	isr_status &= XUartPs_ReadReg(this->uartps.Config.BaseAddress, XUARTPS_ISR_OFFSET);

	// Clear interrupts before any operation to prevent driver locking, which has been seen!
	XUartPs_WriteReg(this->uartps.Config.BaseAddress, XUARTPS_ISR_OFFSET, isr_status);

	// Dispatch an appropriate handler
	if((isr_status & ((uint32_t)XUARTPS_IXR_RXOVR | (uint32_t)XUARTPS_IXR_RXEMPTY |
			(uint32_t)XUARTPS_IXR_RXFULL | (uint32_t)XUARTPS_IXR_TOUT)) != (uint32_t)0) {
		// If there are bytes still to be received in the specified buffer go ahead and
		// receive them. Removing bytes from the RX FIFO will clear the interrupt.
		if (!this->inbuf.full()) {
			this->recv();
		}
	}

	if((isr_status & (uint32_t)XUARTPS_IXR_TXEMPTY) != (uint32_t)0) {
		// If there are not bytes to be sent from the specified buffer then disable the transmit
		// interrupt so it will stop interrupting as it interrupts any time the FIFO is empty
		if (this->outbuf.empty()) {
			// There is nothing else in the transmit buffer to send
			// Disable interrupt so it doesn't keep triggering
			XUartPs_WriteReg(this->uartps.Config.BaseAddress, XUARTPS_IDR_OFFSET,
					(uint32_t)XUARTPS_IXR_TXEMPTY);
		} else {
			// If FIFO is empty and there is data to send then keep doing so
			this->send();
		}
	}

	// XUARTPS_IXR_RBRK is applicable only for Zynq Ultrascale+ MP
	if ((isr_status & ((uint32_t)XUARTPS_IXR_OVER | (uint32_t)XUARTPS_IXR_FRAMING |
			(uint32_t)XUARTPS_IXR_PARITY | (uint32_t)XUARTPS_IXR_RBRK)) != (uint32_t)0) {
		// Some errors, increase the error counter
		this->error_count++;

		// If there are bytes still to be received in the specified buffer go ahead and
		// receive them. Removing bytes from the RX FIFO will clear the interrupt.
		if (!this->inbuf.full()) {
			this->recv();
		}
	}

	if((isr_status & ((uint32_t)XUARTPS_IXR_DMS)) != (uint32_t)0) {
		// Modem status interrupt not supported, just read to clean status
		XUartPs_ReadReg(this->uartps.Config.BaseAddress, XUARTPS_MODEMSR_OFFSET);
	}
}

PSUART::PSUART(uint16_t device_id, uint16_t intr_id, size_t ibufsize, size_t obufsize) :
InterruptBasedDriver(intr_id), error_count(0),
inbuf(RingBuffer<uint8_t>(ibufsize)), outbuf(RingBuffer<uint8_t>(obufsize)) {
	XUartPs_Config *Config = XUartPs_LookupConfig(device_id);
	if (Config == nullptr) {
		throw except::hardware_error("Unable retrieve configuration for PSUART(device_id=" + std::to_string(device_id) + ")");
	}

	if (XST_SUCCESS != XUartPs_CfgInitialize(&this->uartps, Config, Config->BaseAddress)) {
		throw except::hardware_error("Unable to initialize PSUART(device_id=" + std::to_string(device_id) + ")");
	}

	XUartPs_SetInterruptMask(&this->uartps, 0);
	XUartPs_SetRecvTimeout(&this->uartps, 8); // My example says this is uint32_ts, the comments say its nibbles, the TRM says its baud_sample_clocks.

	// Enable receive interrupts
	this->enableInterrupts();
	enableRecvInterrupts();
}

PSUART::~PSUART() {
	// Disable receive interrupts
	disableRecvInterrupts();
}

size_t PSUART::read(uint8_t *buf, size_t len, TickType_t timeout, TickType_t data_timeout) {
	configASSERT(timeout == 0 || !(IN_INTERRUPT() || IN_CRITICAL()));

	AbsoluteTimeout abstimeout(timeout);
	AbsoluteTimeout abs_data_timeout(data_timeout);
	size_t bytesread = 0;
	while (bytesread < len) {
		/* We join the readwait queue now, because if we did this later we would
		 * have problems with a race condition between the read attempt and
		 * starting the wait.  We can cancel it later with a wait(timeout=0).
		 */
		WaitList<true>::Subscription sub;
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
		if (!sub.wait(abstimeout.getTimeout()))
			break; // Timed out.
	}
	return bytesread;
}

size_t PSUART::write(const uint8_t *buf, size_t len, TickType_t timeout) {
	configASSERT(timeout == 0 || !(IN_INTERRUPT() || IN_CRITICAL()));

	AbsoluteTimeout abstimeout(timeout);
	size_t byteswritten = 0;
	while (byteswritten < len) {
		/* We join the writewait queue now, because if we did this later we
		 * would have problems with a race condition between the write attempt
		 * and starting the wait.  We can cancel it later with a
		 * wait(timeout=0).
		 */
		WaitList<true>::Subscription sub;
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
			uint32_t ImrRegister = XUartPs_ReadReg(this->uartps.Config.BaseAddress, XUARTPS_IMR_OFFSET);

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
		if (!sub.wait(abstimeout.getTimeout()))
			break; // Timed out.
	}
	return byteswritten;
}

bool PSUART::clear() {
	this->disableInterrupts();
	this->inbuf.reset();
	this->enableInterrupts();

	return true;
}

#endif

