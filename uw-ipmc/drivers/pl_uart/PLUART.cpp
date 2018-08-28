/*
 * PLUART.cpp
 *
 *  Created on: Jul 3, 2018
 *      Author: mpv
 */

#include "PLUART.h"
#include <xscugic.h>
#include <stdio.h>
#include "xil_exception.h"
#include "xuartlite_l.h"

extern XScuGic xInterruptController;

void PL_UART::_InterruptHandler(PL_UART *uart)
{
	// Note: Interrupts won't be disabled while running this interrupt.
	// If nesting is enabled that this cause problems.

	// Read status register
	uint8_t StatusRegister = XUartLite_GetStatusReg(uart->UartLite.RegBaseAddress);
	uint8_t data = 0;

	while ((StatusRegister & (XUL_SR_RX_FIFO_FULL | XUL_SR_RX_FIFO_VALID_DATA)) != 0) {
		data = XUartLite_ReadReg(uart->UartLite.RegBaseAddress, XUL_RX_FIFO_OFFSET);
		xStreamBufferSendFromISR(uart->recvstream, &data, 1, NULL);

		StatusRegister = XUartLite_GetStatusReg(uart->UartLite.RegBaseAddress);
	}

	while (((StatusRegister & XUL_SR_TX_FIFO_EMPTY) != 0) &&
			(xStreamBufferReceiveFromISR(uart->sendstream, &data, 1, NULL) > 0)) {
		XUartLite_WriteReg(uart->UartLite.RegBaseAddress, XUL_TX_FIFO_OFFSET, data);

		StatusRegister = XUartLite_GetStatusReg(uart->UartLite.RegBaseAddress);
	}

	// TODO: Do error detection, inc. streambuffer full
}

PL_UART::PL_UART(uint16_t DeviceId, uint32_t IntrId, size_t ibufsize, size_t obufsize) :
IntrId(IntrId) {
	// Initialize the UartLite driver so that it's ready to use.
	configASSERT(XST_SUCCESS == XUartLite_Initialize(&(this->UartLite), DeviceId));

	// Perform a self-test to ensure that the hardware was built correctly.
	configASSERT(XST_SUCCESS == XUartLite_SelfTest(&(this->UartLite)));

	// UartLite interrupt works differently than expected, adjust trigger type.
	uint8_t priority, trigger;
	XScuGic_GetPriorityTriggerType(&xInterruptController, this->IntrId, &priority, &trigger);
	XScuGic_SetPriorityTriggerType(&xInterruptController, this->IntrId, priority, 0x3);

	// Connect the driver to the interrupt subsystem.
	configASSERT(XST_SUCCESS ==
			XScuGic_Connect(&xInterruptController, this->IntrId, (Xil_InterruptHandler)PL_UART::_InterruptHandler, (void*)this));
	XScuGic_Enable(&xInterruptController, this->IntrId);

	this->recvstream = xStreamBufferCreate(ibufsize, 0);
	configASSERT(this->recvstream);
	this->sendstream = xStreamBufferCreate(obufsize, 0);
	configASSERT(this->sendstream);

	// Enable the interrupt of UartLite so that interrupts will occur.
	XUartLite_EnableInterrupt(&(this->UartLite));
}

PL_UART::~PL_UART() {
	// Disable the interrupts associated with UartLite
	XScuGic_Disable(&xInterruptController, this->IntrId);
	XScuGic_Disconnect(&xInterruptController, this->IntrId);

	// Destroy streams
	vStreamBufferDelete(this->recvstream);
	vStreamBufferDelete(this->sendstream);
}

size_t PL_UART::read(u8 *buf, size_t len, TickType_t timeout, TickType_t data_timeout) {
	// By setting the trigger level on the buffer stream it allows to set a proper timeout
	xStreamBufferSetTriggerLevel(this->recvstream, len);
	return xStreamBufferReceive(this->recvstream, buf, len, timeout);
}

size_t PL_UART::write(const u8 *buf, size_t len, TickType_t timeout) {
	// Sanity check
	if (len <= 0) return 0;

	// Fill the stream buffer with as much data as possible and return right away
	size_t count = xStreamBufferSend(this->sendstream, buf, len, 0);

	// If the UART driver was idle (TX fifo empty) then send a single byte to jolt the interrupt routine
	// Note: Not multithread safe needs to be wrapped in mutexes
	portENTER_CRITICAL();
	{
		uint8_t StatusRegister = XUartLite_GetStatusReg(UartLite.RegBaseAddress);
		if ((StatusRegister & XUL_SR_TX_FIFO_EMPTY) != 0) {
			// Just write a single byte to trigger the UART interrupt
			uint8_t d = 0;
			xStreamBufferReceive(this->sendstream, &d, 1, 0);
			XUartLite_WriteReg(this->UartLite.RegBaseAddress, XUL_TX_FIFO_OFFSET, d);
		}
	}
	portEXIT_CRITICAL();

	// Not all data has been sent, take care of that, this time with a timeout
	if (len > count) {
		count += xStreamBufferSend(this->sendstream, buf+count, len-count, timeout);
	}

	return count;
}

bool PL_UART::clear() {
	return (xStreamBufferReset(this->recvstream) == pdPASS);
}

