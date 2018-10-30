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

static void PS_UART_InterruptPassthrough(PS_UART *ps_uart, u32 Event, u32 EventData) {
	ps_uart->_HandleInterrupt(Event, EventData);
}

static void XUartPs_EnableInterruptMask(XUartPs *InstancePtr, u32 Mask)
{
	configASSERT(InstancePtr != NULL);

	Mask &= (u32)XUARTPS_IXR_MASK;

	/* Write the mask to the IER Register */
	XUartPs_WriteReg(InstancePtr->Config.BaseAddress,
		 XUARTPS_IER_OFFSET, Mask);
}

static void XUartPs_DisableInterruptMask(XUartPs *InstancePtr, u32 Mask)
{
	configASSERT(InstancePtr != NULL);

	Mask &= (u32)XUARTPS_IXR_MASK;

	/* Write the inverse of the Mask to the IDR register */
	XUartPs_WriteReg(InstancePtr->Config.BaseAddress,
		 XUARTPS_IDR_OFFSET, (~Mask));

}

u32 PS_UART::XUartPs_ReceiveBuffer(XUartPs *InstancePtr)
{
	u32 CsrRegister;
	u32 ReceivedCount = 0U;
	u32 ByteStatusValue, EventData;
	u32 Event;

	u8 *dma_inbuf;
	size_t items;

	this->inbuf.setup_dma_input(&dma_inbuf, &items);

	/*
	 * Read the Channel Status Register to determine if there is any data in
	 * the RX FIFO
	 */
	CsrRegister = XUartPs_ReadReg(InstancePtr->Config.BaseAddress,
				XUARTPS_SR_OFFSET);

	/*
	 * Loop until there is no more data in RX FIFO or the specified
	 * number of bytes has been received
	 */
	while((ReceivedCount < items) &&
		(((CsrRegister & XUARTPS_SR_RXEMPTY) == (u32)0))){

		if (InstancePtr->is_rxbs_error) {
			ByteStatusValue = XUartPs_ReadReg(
						InstancePtr->Config.BaseAddress,
						XUARTPS_RXBS_OFFSET);
			if((ByteStatusValue & XUARTPS_RXBS_MASK)!= (u32)0) {
				EventData = ByteStatusValue;
				Event = XUARTPS_EVENT_PARE_FRAME_BRKE;
				/*
				 * Call the application handler to indicate that there is a receive
				 * error or a break interrupt, if the application cares about the
				 * error it call a function to get the last errors.
				 */
				InstancePtr->Handler(InstancePtr->CallBackRef,
							Event, EventData);
			}
		}

		dma_inbuf[ReceivedCount++] = XUartPs_ReadReg(InstancePtr->Config.BaseAddress, XUARTPS_FIFO_OFFSET);

		CsrRegister = XUartPs_ReadReg(InstancePtr->Config.BaseAddress, XUARTPS_SR_OFFSET);
	}
	InstancePtr->is_rxbs_error = 0;

	this->inbuf.notify_dma_input_occurred(ReceivedCount);
	this->readwait.wake(); // We received something.

	return ReceivedCount;
}

u32 PS_UART::XUartPs_Recv(XUartPs *InstancePtr)
{
	u32 ReceivedCount;
	u32 ImrRegister;

	/* Assert validates the input arguments */
	Xil_AssertNonvoid(InstancePtr != NULL);
	Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

	/*
	 * Disable all the interrupts.
	 * This stops a previous operation that may be interrupt driven
	 */
	//ImrRegister = XUartPs_ReadReg(InstancePtr->Config.BaseAddress, XUARTPS_IMR_OFFSET);
	//XUartPs_WriteReg(InstancePtr->Config.BaseAddress, XUARTPS_IDR_OFFSET, XUARTPS_IXR_MASK);

	/* Receive the data from the device */
	ReceivedCount = XUartPs_ReceiveBuffer(InstancePtr);

	/* Restore the interrupt state */
	//XUartPs_WriteReg(InstancePtr->Config.BaseAddress, XUARTPS_IER_OFFSET, ImrRegister);

	return ReceivedCount;
}

u32 PS_UART::XUartPs_SendBuffer(XUartPs *InstancePtr)
{
	u32 SentCount = 0U;
	u32 ImrRegister;

	u8 *dma_outbuf = NULL;
	size_t items;

	this->outbuf.setup_dma_output(&dma_outbuf, &items);
	if (items > this->oblocksize)
		items = this->oblocksize;

	/*
	 * If the TX FIFO is full, send nothing.
	 * Otherwise put bytes into the TX FIFO until it is full, or all of the
	 * data has been put into the FIFO.
	 */
	while ((!XUartPs_IsTransmitFull(InstancePtr->Config.BaseAddress)) &&
		   (items > SentCount)) {

		/* Fill the FIFO from the buffer */
		XUartPs_WriteReg(InstancePtr->Config.BaseAddress,
				   XUARTPS_FIFO_OFFSET,
				   ((u32)dma_outbuf[SentCount]));

		/* Increment the send count. */
		SentCount++;
	}

	this->outbuf.notify_dma_output_occurred(SentCount);

	/*
	 * If interrupts are enabled as indicated by the receive interrupt, then
	 * enable the TX FIFO empty interrupt, so further action can be taken
	 * for this sending.
	 */
	if (!this->outbuf.empty()) {
		// There is still data to send, so re-enable TX empty interrupt
		ImrRegister = XUartPs_ReadReg(InstancePtr->Config.BaseAddress, XUARTPS_IMR_OFFSET);
		if (((ImrRegister & XUARTPS_IXR_RXFULL) != (u32)0) ||
			((ImrRegister & XUARTPS_IXR_RXEMPTY) != (u32)0)||
			((ImrRegister & XUARTPS_IXR_RXOVR) != (u32)0)) {

			XUartPs_WriteReg(InstancePtr->Config.BaseAddress,
						   XUARTPS_IER_OFFSET,
						   ImrRegister | (u32)XUARTPS_IXR_TXEMPTY);
		}
	}

	if (SentCount) {
		this->writewait.wake();
	}

	/*if (this->outbuf.empty()) {
		this->write_running = false;
	}*/

	return SentCount;
}

u32 PS_UART::XUartPs_Send(XUartPs *InstancePtr)
{
	u32 BytesSent;

	/* Asserts validate the input arguments */
	Xil_AssertNonvoid(InstancePtr != NULL);
	Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

	/*
	 * Disable the UART transmit interrupts to allow this call to stop a
	 * previous operation that may be interrupt driven.
	 */
	/*XUartPs_WriteReg(InstancePtr->Config.BaseAddress, XUARTPS_IDR_OFFSET,
					  XUARTPS_IXR_TXEMPTY);*/

	/*
	 * Transmit interrupts will be enabled in XUartPs_SendBuffer(), after
	 * filling the TX FIFO.
	 */
	BytesSent = XUartPs_SendBuffer(InstancePtr);

	return BytesSent;
}

void PS_UART::_InterruptHandler() {
	u32 IsrStatus;

	Xil_AssertVoid(this->UartInst.IsReady == XIL_COMPONENT_IS_READY);

	/*
	 * Read the interrupt ID register to determine which
	 * interrupt is active
	 */
	IsrStatus = XUartPs_ReadReg(this->UartInst.Config.BaseAddress,
				   XUARTPS_IMR_OFFSET);

	IsrStatus &= XUartPs_ReadReg(this->UartInst.Config.BaseAddress,
				   XUARTPS_ISR_OFFSET);

	/* Dispatch an appropriate handler. */
	if((IsrStatus & ((u32)XUARTPS_IXR_RXOVR | (u32)XUARTPS_IXR_RXEMPTY |
			(u32)XUARTPS_IXR_RXFULL) | (u32)XUARTPS_IXR_TOUT) != (u32)0) {
		/* Received data interrupt */
		/*
		 * If there are bytes still to be received in the specified buffer
		 * go ahead and receive them. Removing bytes from the RX FIFO will
		 * clear the interrupt.
		 */
		if (!this->inbuf.full()) {
			(void)XUartPs_ReceiveBuffer(&(this->UartInst));
		}
	}

	if((IsrStatus & (u32)XUARTPS_IXR_TXEMPTY) != (u32)0) {
		/* Transmit data interrupt */
		/*
		 * If there are not bytes to be sent from the specified buffer then disable
		 * the transmit interrupt so it will stop interrupting as it interrupts
		 * any time the FIFO is empty
		 */
		if (this->outbuf.empty()) {
			// There is nothing else in the transmit buffer to send
			// Disable interrupt so it doesn't keep triggering
			XUartPs_WriteReg(this->UartInst.Config.BaseAddress,
					XUARTPS_IDR_OFFSET,
					(u32)XUARTPS_IXR_TXEMPTY);

		} else if ((IsrStatus & ((u32)XUARTPS_IXR_TXEMPTY)) != (u32)0) {
			// If FIFO is empty and there is data to send then keep doing so
			XUartPs_SendBuffer(&(this->UartInst));
		}
	}

	/* XUARTPS_IXR_RBRK is applicable only for Zynq Ultrascale+ MP */
	if ((IsrStatus & ((u32)XUARTPS_IXR_OVER | (u32)XUARTPS_IXR_FRAMING |
			(u32)XUARTPS_IXR_PARITY | (u32)XUARTPS_IXR_RBRK)) != (u32)0) {
		u32 EventData;
		u32 Event;

		this->UartInst.is_rxbs_error = 0;

		if ((this->UartInst.Platform == XPLAT_ZYNQ_ULTRA_MP) &&
			(IsrStatus & ((u32)XUARTPS_IXR_PARITY | (u32)XUARTPS_IXR_RBRK
						| (u32)XUARTPS_IXR_FRAMING))) {
			this->UartInst.is_rxbs_error = 1;
		}

		/* Received Error Status interrupt */
		/*
		 * If there are bytes still to be received in the specified buffer
		 * go ahead and receive them. Removing bytes from the RX FIFO will
		 * clear the interrupt.
		 */

		(void)XUartPs_ReceiveBuffer(&(this->UartInst));

		if (!(this->UartInst.is_rxbs_error)) {
			Event = XUARTPS_EVENT_RECV_ERROR;
			EventData = this->UartInst.ReceiveBuffer.RequestedBytes -
					this->UartInst.ReceiveBuffer.RemainingBytes;

			/*
			 * Call the application handler to indicate that there is a receive
			 * error or a break interrupt, if the application cares about the
			 * error it call a function to get the last errors.
			 */
			this->UartInst.Handler(this->UartInst.CallBackRef,
						Event,
						EventData);
		}
	}

	if((IsrStatus & ((u32)XUARTPS_IXR_DMS)) != (u32)0) {
		// Modem status interrupt not supported, just read to clean status
		XUartPs_ReadReg(this->UartInst.Config.BaseAddress, XUARTPS_MODEMSR_OFFSET);
	}

	// Clear the interrupt status
	XUartPs_WriteReg(this->UartInst.Config.BaseAddress, XUARTPS_ISR_OFFSET,
		IsrStatus);
}

/**
 * Instantiate a PS_UART driver.
 *
 * \note This performs hardware setup (mainly interrupt configuration).
 *
 * \param DeviceId    The DeviceId, used for XUartPs_LookupConfig(), etc
 * \param IntrId      The interrupt ID, for configuring the GIC.
 * \param ibufsize    The size of the input buffer to allocate.
 * \param obufsize    The size of the output buffer to allocate.
 * \param oblocksize  The maximum block size for output operations.  Relevant to wait times when the queue is full.
 */
PS_UART::PS_UART(u32 DeviceId, u32 IntrId, u32 ibufsize, u32 obufsize,
		u32 oblocksize) : InterruptBasedDriver(IntrId),
		error_mask(0), inbuf(RingBuffer<u8>(ibufsize)), outbuf(
				RingBuffer<u8>(obufsize)), oblocksize(oblocksize) {

	XUartPs_Config *Config = XUartPs_LookupConfig(DeviceId);
	configASSERT(XST_SUCCESS == XUartPs_CfgInitialize(&this->UartInst, Config, Config->BaseAddress));
	XUartPs_SetInterruptMask(&this->UartInst, 0);
	XUartPs_SetHandler(&this->UartInst, reinterpret_cast<XUartPs_Handler>(PS_UART_InterruptPassthrough), reinterpret_cast<void*>(this));
	XUartPs_SetRecvTimeout(&this->UartInst, 8); // My example says this is u32s, the comments say its nibbles, the TRM says its baud_sample_clocks.


	XUartPs_EnableInterruptMask(&this->UartInst, IXR_RECV_ENABLE);
}

PS_UART::~PS_UART() {
	XUartPs_Send(&this->UartInst);
	XUartPs_Recv(&this->UartInst);
	XUartPs_DisableInterruptMask(&this->UartInst, IXR_RECV_ENABLE);
}

/**
 * Read from the PS_UART.
 *
 * \param buf The buffer to read into.
 * \param len The maximum number of bytes to read.
 * \param timeout The timeout for this read, in standard FreeRTOS format.
 * \param data_timeout A second timeout used to shorten `timeout` if data is present.
 *
 * \note This function is interrupt and critical safe if timeout=0.
 */
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
			taskENTER_CRITICAL();
		size_t batch_bytesread = this->inbuf.read(buf+bytesread, len-bytesread);
		if (batch_bytesread) {
			/* We have retrieved SOMETHING from the buffer.  Re-enable receive
			 * interrupts, in case they were disabled due to a full buffer.
			 */
			XUartPs_EnableInterruptMask(&this->UartInst, IXR_RECV_ENABLE);
		}
		bytesread += batch_bytesread;
		if (!IN_INTERRUPT())
			taskEXIT_CRITICAL();
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

/**
 * Write to the PS_UART.
 *
 * \param buf The buffer to write from.
 * \param len The maximum number of bytes to write.
 * \param timeout The timeout for this read, in standard FreeRTOS format.
 *
 * \note This function is interrupt and critical safe if timeout=0.
 */
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
			taskENTER_CRITICAL();
		size_t batch_byteswritten = this->outbuf.write(buf+byteswritten, len-byteswritten);
		if (batch_byteswritten) {
			// Trigger a send if the TX FIFO is empty
			u32 ImrRegister = XUartPs_ReadReg(this->UartInst.Config.BaseAddress, XUARTPS_IMR_OFFSET);

			if (!(ImrRegister & (XUARTPS_IXR_TXEMPTY))) {
				XUartPs_SendBuffer(&(this->UartInst));
			}
		}
		byteswritten += batch_byteswritten;
		if (!IN_INTERRUPT())
			taskEXIT_CRITICAL();
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
	while (!this->inbuf.empty()) {
		u8 tmp;
		this->inbuf.read(&tmp, 1);
	}
	return true;
}

void PS_UART::_HandleInterrupt(u32 Event, u32 EventData) {
	//#define XUARTPS_EVENT_RECV_DATA			1U /**< Data receiving done */
	//#define XUARTPS_EVENT_RECV_TOUT			2U /**< A receive timeout occurred */
	//#define XUARTPS_EVENT_SENT_DATA			3U /**< Data transmission done */
	//#define XUARTPS_EVENT_RECV_ERROR		4U /**< A receive error detected */
	//#define XUARTPS_EVENT_MODEM				5U /**< Modem status changed */
	//#define XUARTPS_EVENT_PARE_FRAME_BRKE	6U /**< A receive parity, frame, break error detected */
	//#define XUARTPS_EVENT_RECV_ORERR		7U /**< A receive overrun error detected */

	if (Event == XUARTPS_EVENT_RECV_DATA || Event == XUARTPS_EVENT_RECV_TOUT
			|| Event == XUARTPS_EVENT_RECV_ERROR) {

		/*if (EventData > 0) {
			this->inbuf.notify_dma_input_occurred(EventData);
			this->readwait.wake(); // We received something.
			u8 *dma_inbuf;
			size_t maxitems;
			this->inbuf.setup_dma_input(&dma_inbuf, &maxitems);
			if (maxitems)
				XUartPs_Recv(&this->UartInst, dma_inbuf, maxitems); // Set next input
			else
				XUartPs_DisableInterruptMask(&this->UartInst, IXR_RECV_ENABLE); // Turn off receive until our buffer drains.
		}*/
	}
	// if (Event == XUARTPS_EVENT_RECV_TOUT) { } // Recv data event.  Handled above.
	if (Event == XUARTPS_EVENT_SENT_DATA) {
		/* This event indicates data transfer is COMPLETE.
		 * It is safe to enqueue more data now.
		 */

		/*this->outbuf.notify_dma_output_occurred(EventData);
		u8 *dma_outbuf = NULL;
		size_t maxitems;
		this->outbuf.setup_dma_output(&dma_outbuf, &maxitems);
		if (maxitems > this->oblocksize)
			maxitems = this->oblocksize;
		if (maxitems) {
			XUartPs_Send(&this->UartInst, dma_outbuf, maxitems);
			this->write_running = true;
			this->writewait.wake();
		}
		else {
			this->write_running = false;
		}*/
	}
	if (Event == XUARTPS_EVENT_RECV_ERROR) {
		this->error_mask |= (1<<XUARTPS_EVENT_RECV_ERROR); // Set error mask flag.  This is probably an overrun error.
		// This appears to also be a recv data event.  Handled above.
	}
	// if (Event == XUARTPS_EVENT_MODEM) { } // Not relevant to UW IPMC.
	if (Event == XUARTPS_EVENT_PARE_FRAME_BRKE) {
		this->error_mask |= (1<<XUARTPS_EVENT_PARE_FRAME_BRKE); // Not really relevant to us, but will include it for now.
	}
	if (Event == XUARTPS_EVENT_RECV_ORERR) {
		this->error_mask |= (1<<XUARTPS_EVENT_RECV_ORERR); // Set error mask flag.  The xuartps driver does not appear to send this event.
	}
}
