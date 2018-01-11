/*
 * PSSPI.cpp
 *
 *  Created on: Jan 2, 2018
 *      Author: jtikalsky
 */

#include <drivers/ps_spi/PSSPI.h>

static void PS_SPI_InterruptPassthrough(PS_SPI *ps_spi, u32 event_status, u32 byte_count) {
	ps_spi->_HandleInterrupt(event_status, byte_count);
}

/**
 * An interrupt-based driver for the PS SPI.
 *
 * \note This performs hardware setup (including interrupt configuration).
 *
 * \param DeviceId    The DeviceId, used for XUartPs_LookupConfig(), etc
 * \param IntrId      The interrupt ID, for configuring the GIC.
 */
PS_SPI::PS_SPI(u16 DeviceId, u32 IntrId) {

	this->mutex = xSemaphoreCreateMutex();
	configASSERT(this->mutex);

	this->irq_sync_q = xQueueCreate(1, sizeof(trans_st_t));
	configASSERT(this->irq_sync_q);

	XSpiPs_Config *Config = XSpiPs_LookupConfig(DeviceId);
	configASSERT(
			XST_SUCCESS == XSpiPs_CfgInitialize(&this->SpiInst, Config, Config->BaseAddress));

	configASSERT(XST_SUCCESS == XSpiPs_SelfTest(&this->SpiInst));
	XSpiPs_Reset(&this->SpiInst);

	XScuGic_Connect(&xInterruptController, IntrId,
			reinterpret_cast<Xil_InterruptHandler>(XSpiPs_InterruptHandler),
			(void*) &this->SpiInst);
	XScuGic_Enable(&xInterruptController, IntrId);

	XSpiPs_SetStatusHandler(&this->SpiInst, reinterpret_cast<void*>(this),
			reinterpret_cast<XSpiPs_StatusHandler>(PS_SPI_InterruptPassthrough));

	XSpiPs_SetOptions(&this->SpiInst, XSPIPS_MANUAL_START_OPTION |
			XSPIPS_MASTER_OPTION |
			XSPIPS_FORCE_SSELECT_OPTION);

	XSpiPs_SetClkPrescaler(&this->SpiInst, XSPIPS_CLK_PRESCALE_64);

	this->error_not_done = 0;
	this->error_byte_count = 0;

	this->transfer_running = false;
}

PS_SPI::~PS_SPI() {

	XScuGic_Disable(&xInterruptController, this->IntrId);
	XScuGic_Disconnect(&xInterruptController, this->IntrId);
}

/**
 * This function will perform a SPI transfer in a blocking manner.
 *
 * \param chip     The chip select to enable.
 * \param sendbuf  The data to send.
 * \param recvbuf  A buffer for received data.
 * \param bytes    The number of bytes to transfer
 * \return false on error, else true
 */
bool PS_SPI::transfer(u8 chip, u8 *sendbuf, u8 *recvbuf, size_t bytes) {

	xSemaphoreTake(this->mutex, portMAX_DELAY);

	// Assert the EEPROM chip select
	XSpiPs_SetSlaveSelect(&this->SpiInst, chip);

	// Start SPI transfer
	XSpiPs_Transfer(&this->SpiInst, sendbuf, recvbuf, bytes);
	this->transfer_running = true;

	// Block on the queue, waiting for irq to signal transfer completion
	trans_st_t trans_st;
	xQueueReceive(this->irq_sync_q, &(trans_st), portMAX_DELAY);

	bool rc = true;

	//If the event was not transfer done, then track it as an error
	if (trans_st.event_status != XST_SPI_TRANSFER_DONE) {
		this->error_not_done++;
		rc = false;
	}

	//If the last transfer completed with, then track it as an error
	if (trans_st.event_status != bytes) {
		this->error_byte_count++;
#if 0
		rc = false;  //TODO: not clear yet why xilinx driver reports byte_count = 0 for all successful transfers
#endif
	}

	xSemaphoreGive(this->mutex);

	return rc;
}

void PS_SPI::_HandleInterrupt(u32 event_status, u32 byte_count) {

	trans_st_t trans_st;
	trans_st.event_status = event_status;
	trans_st.byte_count = byte_count;

	xQueueSendFromISR(this->irq_sync_q, &trans_st, pdFALSE);

	this->transfer_running = false;
}
