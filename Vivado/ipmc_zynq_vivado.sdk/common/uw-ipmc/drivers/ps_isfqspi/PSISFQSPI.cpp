/*
 * PSisfqspi->cpp
 *
 *  Created on: Feb 13, 2018
 *      Author: mpv
 */

// This driver is heavily based on Xilinx ISF STM INTR example.

#include <drivers/ps_isfqspi/PSISFQSPI.h>
#include <libs/printf.h>
#include <libs/except.h>
#include <libs/ThreadingPrimitives.h>
#include "libs/XilinxImage.h"

#include "xparameters.h"

/**
 * The following defines are for dual flash stacked mode interface.
 */
#define LQSPI_CR_FAST_QUAD_READ		0x0000006B // Fast Quad Read output
#define LQSPI_CR_1_DUMMY_BYTE		0x00000100 // 1 Dummy Byte between address and return data

#define DUAL_STACK_CONFIG_WRITE		(XQSPIPS_LQSPI_CR_TWO_MEM_MASK | \
					 LQSPI_CR_1_DUMMY_BYTE | \
					 LQSPI_CR_FAST_QUAD_READ) //  Fast Quad Read output

#define DUAL_QSPI_CONFIG_WRITE		(XQSPIPS_LQSPI_CR_TWO_MEM_MASK | \
					 XQSPIPS_LQSPI_CR_SEP_BUS_MASK | \
					 LQSPI_CR_1_DUMMY_BYTE | \
					 LQSPI_CR_FAST_QUAD_READ) // Fast Quad Read output

#define FAST_READ_NUM_DUMMY_BYTES	1 // Number Dummy Bytes for Fast Read
#define DUAL_READ_NUM_DUMMY_BYTES	1 // Number Dummy Bytes for Dual Read
#define QUAD_READ_NUM_DUMMY_BYTES	1 // Number Dummy Bytes for Quad Read

/**
 * The following constants define the offsets within a FlashBuffer data
 * type for each kind of data.  Note that the read data offset is not the
 * same as the write data because the QSPI driver is designed to allow full
 * duplex transfers such that the number of bytes received is the number
 * sent and received.
 */
#define DATA_OFFSET		4 /**< Start of Data for Read/Write */
#define DUMMY_OFFSET	4 /**< Dummy byte offset for fast, dual and quad reads */
#define DUMMY_SIZE		1 /**< Number of dummy bytes for fast, dual and quad reads */

/**
 * The following constant defines the slave select signal that is used to
 * to select the FLASH device on the QSPI bus, this signal is typically
 * connected to the chip select of the device
 */
#define FLASH_QSPI_SELECT	0x00

static void PS_ISFQSPI_InterruptPassthrough(PS_ISFQSPI *ps_isfqspi, u32 event_status, u32 byte_count) {
	ps_isfqspi->_HandleInterrupt(event_status, byte_count);
}

/**
 * Instantiate a PS_ISFQSPI driver.
 *
 * \note This performs hardware setup (mainly interrupt configuration).
 *
 * \param DeviceId             The DeviceId, used for XIicPs_LookupConfig(), etc
 * \param IntrId               The interrupt ID, for configuring the GIC.
 */
PS_ISFQSPI::PS_ISFQSPI(u16 DeviceId, u16 IntrId) :
IntrId(IntrId), error_not_done(0), error_byte_count(0),
IsfWriteBuffer(NULL), IsfReadBuffer(NULL) {
	// Make sure class variables are zeroed before initialization
	memset(&(this->QspiInst), 0, sizeof(XQspiPs));
	memset(&(this->IsfInst), 0, sizeof(XIsf));

	// Look up for the device configuration
	XQspiPs_Config *ConfigPtr = XQspiPs_LookupConfig(DeviceId);
	if (!ConfigPtr)
		throw except::hardware_error(stdsprintf("Unable locate hardware config for PS_ISFQSPI(%hu, %hu)", DeviceId, IntrId));
	if (XST_SUCCESS !=
			XQspiPs_CfgInitialize(&(this->QspiInst), ConfigPtr, ConfigPtr->BaseAddress))
		throw except::hardware_error(stdsprintf("Unable to initialize config for PS_ISFQSPI(%hu, %hu)", DeviceId, IntrId));

	// Run a self test
	if (XST_SUCCESS != XQspiPs_SelfTest(&(this->QspiInst)))
		throw except::hardware_error(stdsprintf("Self-test failed for PS_ISFQSPI(%hu, %hu)", DeviceId, IntrId));

	// Reset the PS core
	XQspiPs_Reset(&(this->QspiInst));

	// Create a new Queue for the interrupt
	this->irq_sync_q = xQueueCreate(1, sizeof(trans_st_t));
	configASSERT(this->irq_sync_q);

	// Create a new Mutex for the transfer functions
	this->mutex = xSemaphoreCreateMutex();
	configASSERT(this->mutex);

	// Set the QSPI options
	u32 Options =
			XQSPIPS_FORCE_SSELECT_OPTION |
			XQSPIPS_MANUAL_START_OPTION |
			XQSPIPS_HOLD_B_DRIVE_OPTION;

	if (XST_SUCCESS !=
			XIsf_SetSpiConfiguration(&(this->IsfInst), &(this->QspiInst), Options, XISF_SPI_PRESCALER))
		throw except::hardware_error(stdsprintf("Unable to SetSpiConfiguration for PS_ISFQSPI(%hu, %hu)", DeviceId, IntrId));

	if (ConfigPtr->ConnectionMode == XQSPIPS_CONNECTION_MODE_STACKED) {
		XQspiPs_SetLqspiConfigReg(&(this->QspiInst), DUAL_STACK_CONFIG_WRITE);
	}

	if(ConfigPtr->ConnectionMode == XQSPIPS_CONNECTION_MODE_PARALLEL) {
		XQspiPs_SetLqspiConfigReg(&(this->QspiInst), DUAL_QSPI_CONFIG_WRITE);
	}

	const u32 tempPageSize = 256; // Only until the actual page size gets figured out

	// Initialize the XILISF Library
	this->IsfWriteBuffer = (u8*)malloc(sizeof(u8) * (tempPageSize + XISF_CMD_SEND_EXTRA_BYTES_4BYTE_MODE));
	if (this->IsfWriteBuffer == NULL)
		throw std::runtime_error(stdsprintf("Unable to allocate %lu byte IsfWriteBuffer.", sizeof(u8) * (tempPageSize + XISF_CMD_SEND_EXTRA_BYTES_4BYTE_MODE)));

	if (XST_SUCCESS !=
			XIsf_Initialize(&(this->IsfInst), &(this->QspiInst), FLASH_QSPI_SELECT, this->IsfWriteBuffer))
		throw except::hardware_error(stdsprintf("Unable to initialize XIsf for PS_ISFQSPI(%hu, %hu)", DeviceId, IntrId));

	if (tempPageSize != this->GetPageSize()) {
		// Actual page size turned to be different then tempPageSize, resize the write buffer
		free(this->IsfWriteBuffer);

		this->IsfWriteBuffer = (u8*)malloc(sizeof(u8) * (this->GetPageSize() + XISF_CMD_SEND_EXTRA_BYTES_4BYTE_MODE));
		if (this->IsfWriteBuffer == NULL)
			throw std::runtime_error(stdsprintf("Unable to allocate %lu byte IsfWriteBuffer.", sizeof(u8) * (this->GetPageSize() + XISF_CMD_SEND_EXTRA_BYTES_4BYTE_MODE)));

		this->IsfInst.WriteBufPtr = this->IsfWriteBuffer;
	}

	// Set the interrupt mode
	XIsf_SetTransferMode(&(this->IsfInst), XISF_INTERRUPT_MODE);

	// Connect interrupt handler to GIC
	if (XST_SUCCESS !=
			XScuGic_Connect(&xInterruptController, this->IntrId,
					(Xil_InterruptHandler)XQspiPs_InterruptHandler,
					(void*)&(this->QspiInst)))
		throw except::hardware_error(stdsprintf("Unable connect PS_ISFQSPI(%hu, %hu) to the GIC", DeviceId, IntrId));

	// Enable the interrupt
	XScuGic_Enable(&xInterruptController, this->IntrId);

	// Set the status handle
	XIsf_SetStatusHandler(&(this->IsfInst), &(this->QspiInst), reinterpret_cast<void*>(this), reinterpret_cast<XIsf_StatusHandler>(PS_ISFQSPI_InterruptPassthrough));

	// Create the read buffer
	this->IsfReadBuffer = (u8*)malloc(sizeof(u8) * (this->GetPageSize() + DATA_OFFSET + DUMMY_SIZE));
	if (this->IsfReadBuffer == NULL)
		throw std::runtime_error(stdsprintf("Unable to allocate %lu byte IsfReadBuffer.", sizeof(u8) * (this->GetPageSize() + DATA_OFFSET + DUMMY_SIZE)));
}

PS_ISFQSPI::~PS_ISFQSPI() {
	abort(); // Destruction not fully supported (no Mutex/Queue cleanup)

	// Disconnect from the interrupt controller
	XScuGic_Disable(&xInterruptController, this->IntrId);
	XScuGic_Disconnect(&xInterruptController, this->IntrId);

	if (this->IsfWriteBuffer) free(this->IsfWriteBuffer);
	if (this->IsfReadBuffer) free(this->IsfReadBuffer);
}

/**
 * This function will perform a flash read in a non-blocking manner.
 *
 * \param Address    The address where the read should occur.
 * \return Pointer to a buffer with the data of a page.
 */
u8* PS_ISFQSPI::ReadPage(u32 Address)
{
	XIsf_ReadParam ReadParam;
	const XIsf_ReadOperation Command = XISF_QUAD_OP_FAST_READ; // If the flash changes, this might need to change too.

	MutexGuard<false> lock(this->mutex, true);

	/*
	 * Set the
	 * - Address in the Serial Flash where the data is to be read from.
	 * - Number of bytes to be read from the Serial Flash.
	 * - Read Buffer to which the data is to be read.
	 */
	ReadParam.Address = Address;
	ReadParam.NumBytes = this->GetPageSize();
	ReadParam.ReadPtr = this->IsfReadBuffer;
	if(Command == XISF_FAST_READ){
		ReadParam.NumDummyBytes = FAST_READ_NUM_DUMMY_BYTES;
	}
	else if(Command == XISF_DUAL_OP_FAST_READ){
		ReadParam.NumDummyBytes = DUAL_READ_NUM_DUMMY_BYTES;
	}
	else{
		ReadParam.NumDummyBytes = QUAD_READ_NUM_DUMMY_BYTES;
	}

	// Perform the Read operation
	if (XIsf_Read(&(this->IsfInst), Command, (void*) &ReadParam) != XST_SUCCESS)
		return NULL;

	// Block on the queue, waiting for irq to signal transfer completion
	trans_st_t trans_st;
	xQueueReceive(this->irq_sync_q, &(trans_st), portMAX_DELAY);

	bool rc = true;

	// If the event was not transfer done, then track it as an error
	if (trans_st.event_status != XST_SPI_TRANSFER_DONE) {
		this->error_not_done++;
		rc = false;
	}

	// If the last transfer completed with, then track it as an error
	if (trans_st.byte_count != 0) {
		this->error_byte_count++;
		rc = false;
	}

	if (rc)
		return &(this->IsfReadBuffer[ReadParam.NumDummyBytes + XISF_CMD_SEND_EXTRA_BYTES]);

	return NULL;
}

/**
 * This function will perform a flash write in a non-blocking manner.
 *
 * \param Address    The address where the write should occur.
 * \param WriteBuf   A buffer with the data to write. Buffer needs to be the size of a page.
 * \return false on error, else true.
 */
bool PS_ISFQSPI::WritePage(u32 Address, u8 *WriteBuf)
{
	XIsf_WriteParam WriteParam;
	const XIsf_WriteOperation Command = XISF_QUAD_IP_PAGE_WRITE; // If the flash changes, this might need to change too.

	MutexGuard<false> lock(this->mutex, true);

	/*
	 * Set the
	 * - Address in the Serial Flash where the data is to be read from.
	 * - Number of bytes to be read from the Serial Flash.
	 * - Read Buffer to which the data is to be read.
	 */
	WriteParam.Address = Address;
	WriteParam.NumBytes = this->GetPageSize();
	WriteParam.WritePtr = WriteBuf;

	// Perform the Write operation
	if (XIsf_Write(&(this->IsfInst), Command, (void*) &WriteParam) != XST_SUCCESS) {
		return false;
	}

	// Block on the queue, waiting for irq to signal transfer completion
	trans_st_t trans_st;
	xQueueReceive(this->irq_sync_q, &(trans_st), portMAX_DELAY);

	bool rc = true;

	// If the event was not transfer done, then track it as an error
	if (trans_st.event_status != XST_SPI_TRANSFER_DONE) {
		this->error_not_done++;
		rc = false;
	}

	// If the last transfer completed with, then track it as an error
	if (trans_st.byte_count != 0) {
		this->error_byte_count++;
		rc = false;
	}

	return rc;
}

/**
 * Delete all flash content by performing a bulk erase.
 *
 * \note SectorErase is recommended because BulkErase doesn't wait until
 *       all bytes have been erased.
 *
 * \return false on error, else true.
 */
bool PS_ISFQSPI::BulkErase()
{
	MutexGuard<false> lock(this->mutex, true);

	// Perform the bulk Erase operation
	if (XIsf_Erase(&(this->IsfInst), XISF_BULK_ERASE, 0) != XST_SUCCESS)
		return false;

	// Block on the queue, waiting for irq to signal transfer completion
	trans_st_t trans_st;
	xQueueReceive(this->irq_sync_q, &(trans_st), portMAX_DELAY);

	bool rc = true;

	// If the event was not transfer done, then track it as an error
	if (trans_st.event_status != XST_SPI_TRANSFER_DONE) {
		this->error_not_done++;
		rc = false;
	}

	// If the last transfer completed with, then track it as an error
	if (trans_st.byte_count != 0) {
		this->error_byte_count++;
		rc = false;
	}

	return rc;
}

/**
 * This function will perform a flash sector erase in a non-blocking manner.
 *
 * \param Address    The address of the start of the sector.
 * \return false on error, else true.
 */
bool PS_ISFQSPI::SectorErase(u32 Address)
{
	MutexGuard<false> lock(this->mutex, true);

	// Perform the bulk Erase operation
	if (XIsf_Erase(&(this->IsfInst), XISF_SECTOR_ERASE, Address) != XST_SUCCESS)
		return false;

	// Block on the queue, waiting for irq to signal transfer completion
	trans_st_t trans_st;
	xQueueReceive(this->irq_sync_q, &(trans_st), portMAX_DELAY);

	bool rc = true;

	// If the event was not transfer done, then track it as an error
	if (trans_st.event_status != XST_SPI_TRANSFER_DONE) {
		this->error_not_done++;
		rc = false;
	}

	// If the last transfer completed with, then track it as an error
	if (trans_st.byte_count != 0) {
		this->error_byte_count++;
		rc = false;
	}

	return rc;
}

/**
 * Return the flash manufacturer's name.
 *
 * \return String with the manufacturer's name.
 */
std::string PS_ISFQSPI::GetManufacturerName() {
	switch (this->IsfInst.ManufacturerID) {
	case (XISF_MANUFACTURER_ID_ATMEL): return "Atmel"; break;
	case (XISF_MANUFACTURER_ID_INTEL): return "Intel"; break;
	case (XISF_MANUFACTURER_ID_WINBOND): return "Winbond"; break;
	case (XISF_MANUFACTURER_ID_SPANSION): return "Spansion"; break;
	case (XISF_MANUFACTURER_ID_SST): return "SST"; break;
	case (XISF_MANUFACTURER_ID_MICRON): return "Micron/STM"; break;
	default: return "Unknown"; break;
	}

	return "Unknown";
}

void PS_ISFQSPI::_HandleInterrupt(u32 event_status, u32 byte_count) {
	trans_st_t trans_st;
	trans_st.event_status = event_status;
	trans_st.byte_count = byte_count;

	xQueueSendFromISR(this->irq_sync_q, &trans_st, pdFALSE);
}

PS_ISFQSPI* PS_ISFQSPI::isfqspi = nullptr;
bool PS_ISFQSPI::firmwareUpdateFailed = false;

size_t PS_ISFQSPI::flash_read(uint8_t *buf, size_t size) {
	size_t totalsize = PS_ISFQSPI::isfqspi->GetTotalSize();
	size_t pagesize = PS_ISFQSPI::isfqspi->GetPageSize();
	int pagecount = totalsize / pagesize;

	for (int i = 0; i < pagecount; i++) {
		size_t addr = i * pagesize;
		uint8_t* ptr = PS_ISFQSPI::isfqspi->ReadPage(addr);
		memcpy(buf + addr, ptr, pagesize);
	}

	return totalsize;
}

size_t PS_ISFQSPI::flash_write(uint8_t *buf, size_t size) {
	// Validate the bin file before writing
	BootFileValidationReturn r = validateBootFile(buf, size);
	if (r != BFV_VALID) {
		// File is invalid!
		printf("Received bin file has errors: %s. Aborting firmware update.",
				getBootFileValidationErrorString(r));
		return 0;
	} else {
		printf("Bin file is valid, proceeding with update.");
	}

	// Write the buffer to flash
	const size_t baseaddr = 0x0;

	size_t rem = size % PS_ISFQSPI::isfqspi->GetPageSize();
	size_t pages = size / PS_ISFQSPI::isfqspi->GetPageSize() + (size? 1 : 0);

	for (size_t i = 0; i < pages; i++) {
		size_t addr = i * PS_ISFQSPI::isfqspi->GetPageSize();
		if (addr % PS_ISFQSPI::isfqspi->GetSectorSize() == 0) {
			// This is the first page of a new sector, so erase the sector
			printf("Erasing sector 0x%08x..", addr + baseaddr);
			if (!PS_ISFQSPI::isfqspi->SectorErase(addr + baseaddr)) {
				// Failed to erase
				printf("Failed to erase 0x%08x. Write to flash failed.", addr + baseaddr);
				PS_ISFQSPI::firmwareUpdateFailed = true;
				return addr;
			}
		}

		if ((i == (pages-1)) && (rem != 0)) {
			// Last page, avoid a memory leak
			uint8_t tmpbuf[PS_ISFQSPI::isfqspi->GetPageSize()];
			memset(tmpbuf, 0xFFFFFFFF, PS_ISFQSPI::isfqspi->GetPageSize());

			memcpy(tmpbuf, buf + addr, rem);

			if (!PS_ISFQSPI::isfqspi->WritePage(addr + baseaddr, tmpbuf)) {
				// Failed to write page
				printf("Failed to write page 0x%08x. Write to flash failed.", addr + baseaddr);
				PS_ISFQSPI::firmwareUpdateFailed = true;
				return addr;
			}
		} else {
			if (!PS_ISFQSPI::isfqspi->WritePage(addr + baseaddr, buf + addr)) {
				// Failed to write page
				printf("Failed to write page 0x%08x. Write to flash failed.", addr + baseaddr);
				PS_ISFQSPI::firmwareUpdateFailed = true;
				return addr;
			}
		}
	}

	// Verify
	for (size_t i = 0; i < pages; i++) {
		size_t addr = i * PS_ISFQSPI::isfqspi->GetPageSize();
		uint8_t* ptr = PS_ISFQSPI::isfqspi->ReadPage(addr + baseaddr);
		int ret = 0;
		if ((i == (pages-1)) && (rem != 0)) {
			// Last page
			ret = memcmp(ptr, buf + addr, rem);
		} else {
			ret = memcmp(ptr, buf + addr, PS_ISFQSPI::isfqspi->GetPageSize());
		}
		if (ret != 0) {
			printf("Page 0x%08x is different. Verification failed.", addr + baseaddr);
			PS_ISFQSPI::firmwareUpdateFailed = true;
			return addr;
		}
	}

	printf("Flash image updated and verified successfully.");
	PS_ISFQSPI::firmwareUpdateFailed = false;
	return size;
}

VFS::File PS_ISFQSPI::createFlashFile(PS_ISFQSPI *isfqspi, size_t bytes) {
	PS_ISFQSPI::isfqspi = isfqspi;
	return VFS::File(flash_read, flash_write, bytes);
}
