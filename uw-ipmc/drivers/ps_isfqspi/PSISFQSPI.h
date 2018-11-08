/*
 * PSISFQSPI.h
 *
 *  Created on: Feb 13, 2018
 *      Author: mpv
 */

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_PS_ISFQSPI_PSISFQSPI_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_PS_ISFQSPI_PSISFQSPI_H_

#include <IPMC.h>
#include <xilisf.h>

/**
 * An interrupt-based driver for the PS In-System Flash QSPI.
 */
class PS_ISFQSPI {
public:
	PS_ISFQSPI(u16 DeviceId, u16 IntrId);
	virtual ~PS_ISFQSPI();

	u8* ReadPage(u32 Address);
	bool WritePage(u32 Address, u8 *WriteBuf);
	bool BulkErase();
	bool SectorErase(u32 Address);

	std::string GetManufacturerName();

	/**
	 * Return the flash's page size.
	 * @return Page size in bytes
	 */
	inline u32 GetPageSize() { return this->IsfInst.BytesPerPage; };

	/**
	 * Return the number of dies in flash.
	 * @return Number of dies
	 */
	inline u32 GetNumDies() { return this->IsfInst.NumDie; };

	/**
	 * Return the flash's sector size.
	 * @return Sector size in bytes
	 */
	inline u32 GetSectorSize() { return this->IsfInst.SectorSize; };

	/**
	 * Return the number of sectors in flash.
	 * @return Page size in bytes
	 */
	inline u32 GetNumSectors() { return this->IsfInst.NumSectors; };

	/**
	 * Return the flash's total size.
	 * @return Total size in bytes
	 */
	inline u32 GetTotalSize() { return this->GetSectorSize() * this->GetNumSectors(); };

	void _HandleInterrupt(u32 Event, u32 EventData); ///< \protected Internal.

private:
	u32 IntrId;
	XQspiPs QspiInst;
	XIsf IsfInst;

	u32 error_not_done;             ///< Error containing accumulated errors of non completed transfers. TODO: allow read.
	u32 error_byte_count;           ///< Error containing accumulated errors of byte count mismatches. TODO: allow read.
	SemaphoreHandle_t mutex;        ///< A mutex serializing bus access requests.
	QueueHandle_t irq_sync_q;       ///< IRQ-task synchronization queue

	u8 *IsfWriteBuffer;
	u8 *IsfReadBuffer;

	/**
	 * irq transfer status
	 */
	struct trans_st_t
	{
		u32 byte_count;    ///< transfer byte count provided to irq handler by xilinx xilisf drive
		u32 event_status;  ///< transfer status provided to irq handler by xilinx xilisf driver
	};
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_PS_ISFQSPI_PSISFQSPI_H_ */
