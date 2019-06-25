/*
 * ipmc.c
 *
 *  Created on: Apr 4, 2019
 *      Author: mpv
 */

#include "ipmc.h"
#include "fsbl.h"
#include "image_mover.h"

#include "xparameters.h"
#include "xgpiops.h"
#include "xspips.h"

#define MAC_EEPROM_CS 1
#define TARGET_IMAGE_ADDRESS 2

u8 get_ipmc_hw_rev() {
	int Status, DataRead;
	XGpioPs Gpio;
	XGpioPs_Config *ConfigPtr;

	/* Initialize the GPIO driver. */
	ConfigPtr = XGpioPs_LookupConfig(XPAR_XGPIOPS_0_DEVICE_ID);
	Status = XGpioPs_CfgInitialize(&Gpio, ConfigPtr, ConfigPtr->BaseAddr);
	if (Status != XST_SUCCESS) {
		return 0xff; // error
	}

	/* Set the direction for the specified pin to be input. */
	XGpioPs_SetDirectionPin(&Gpio, 0, 0);

	/* Read the state of the data so that it can be  verified. */
	DataRead = XGpioPs_ReadPin(&Gpio, 0);

	return (DataRead == 0)? 1 : 0;
}

TargetRecord get_eeprom_boot_record() {
	int Status;
	u8 Buffer[3];
	XSpiPs_Config *SpiConfig;
	XSpiPs SpiInstance;

	/*
	 * Initialize the SPI driver so that it's ready to use
	 */
	SpiConfig = XSpiPs_LookupConfig(XPAR_XSPIPS_0_DEVICE_ID);
	if (NULL == SpiConfig) {
		return 0xff; // error
	}

	Status = XSpiPs_CfgInitialize(&SpiInstance, SpiConfig, SpiConfig->BaseAddress);
	if (Status != XST_SUCCESS) {
		return 0xff; // error
	}

	/*
	 * Set the Spi device as a master. External loopback is required.
	 */
	XSpiPs_SetOptions(&SpiInstance, XSPIPS_MASTER_OPTION | XSPIPS_FORCE_SSELECT_OPTION);

	XSpiPs_SetClkPrescaler(&SpiInstance, XSPIPS_CLK_PRESCALE_64);

	/*
	 * Assert the EEPROM chip select
	 */
	XSpiPs_SetSlaveSelect(&SpiInstance, MAC_EEPROM_CS);

	/*
	 * Setup the write command with the specified address and data for the
	 * EEPROM
	 */
	Buffer[0] = 0x03;
	Buffer[1] = TARGET_IMAGE_ADDRESS;

	/*
	 * Send the read command to the EEPROM to read the specified number
	 * of bytes from the EEPROM, send the read command and address and
	 * receive the specified number of bytes of data in the data buffer
	 */
	Status = XSpiPs_PolledTransfer(&SpiInstance, Buffer, Buffer, sizeof(Buffer));
	if (Status != XST_SUCCESS) {
		return 0xff; // error
	}

	return Buffer[2];
}

u8 get_bootreg_tag() {
	u32 RebootStatusRegister = 0;

	RebootStatusRegister = Xil_In32(REBOOT_STATUS_REG);

	return (RebootStatusRegister >> 24) & 0xF;
}

void set_bootreg_tag(u8 force_boot, u8 force_test_image, u8 target_image) {
	u32 RebootStatusRegister = 0, BootTag = 0;

	force_boot = (force_boot > 0)?1:0;
	force_test_image = (force_test_image > 0)?1:0;
	target_image &= 0x3;

	BootTag = (force_boot << 3) | (force_test_image << 2) | target_image;

	RebootStatusRegister = Xil_In32(REBOOT_STATUS_REG);
	RebootStatusRegister &= ~(0x0F000000);

	RebootStatusRegister |= ((BootTag << 24) & 0x0F000000);

	Xil_Out32(REBOOT_STATUS_REG, RebootStatusRegister);
}

TargetRecord get_bootreg_record() {
	return (get_bootreg_tag() & 0x07);
}

u8 is_record_valid(TargetRecord record) {
	/* 5-highest bits need to be zero and the target image cannot be 3 (out of range) */
	if ((record & 0xf8) || ((record & 0x3) == 3)) return 0;

	return 1;
}

u8 is_forced_boot() {
	return (get_bootreg_tag() >> 3) == 1;
}

u8 is_test_image(TargetRecord record) {
	return (record >> 2) == 1;
}

u8 get_regular_boot_target(TargetRecord record) {
	return (record & 0x3);
}

u8 get_final_boot_target(TargetRecord record) {
	if (is_test_image(record)) return 3;
	else return get_regular_boot_target(record);
}

u32 verify_image(u8 image) {
	u32 Status = 0;
	u32 FsblLength = 0;
	u32 PartitionHeaderOffset = 0;
	u32 PartitionCount = 0;
	u32 PartitionIdx = 0;

	u32 ImageBaseAddress = (u32)image << 24;

	PartHeader PartitionHeader[MAX_PARTITION_NUMBER] = {0};

    /*
     * Get the length of the FSBL from BootHeader
     */
    Status = GetFsblLength(ImageBaseAddress, &FsblLength);
    if (Status != XST_SUCCESS) {
    	fsbl_printf(DEBUG_GENERAL, "[IPMC-VERIFY]: Get Header Start Address Failed\r\n");
    	return XST_FAILURE;
    }

    /*
    * Get the start address of the partition header table
    */
    Status = GetPartitionHeaderStartAddr(ImageBaseAddress, &PartitionHeaderOffset);
    if (Status != XST_SUCCESS) {
    	fsbl_printf(DEBUG_GENERAL, "[IPMC-VERIFY]: Get Header Start Address Failed\r\n");
    	return XST_FAILURE;
    }

    /*
     * Header offset on flash
     */
    PartitionHeaderOffset += ImageBaseAddress;

    fsbl_printf(DEBUG_INFO,"[IPMC-VERIFY]: Partition Header Offset:0x%08lx\r\n", PartitionHeaderOffset);

    /*
     * Load all partitions header data in to global variable
     */
    Status = LoadPartitionsHeaderInfo(PartitionHeaderOffset, &PartitionHeader[0]);
    if (Status != XST_SUCCESS) {
    	fsbl_printf(DEBUG_GENERAL, "[IPMC-VERIFY]: Header Information Load Failed\r\n");
    	return XST_FAILURE;
    }

    /*
     * Get partitions count from partitions header information
     */
	PartitionCount = GetPartitionCount(&PartitionHeader[0]);
    fsbl_printf(DEBUG_INFO, "Partition Count: %lu\r\n", PartitionCount);

    /*
     * A valid IPMC partition will have at least 3 images: FSBL, PL, ELF
     */
    if (PartitionCount >= MAX_PARTITION_NUMBER) {
    	fsbl_printf(DEBUG_GENERAL, "[IPMC-VERIFY]: Invalid number of partitions in image\r\n");
    } else if (PartitionCount < 3) {
    	fsbl_printf(DEBUG_GENERAL, "[IPMC-VERIFY]: Image has less than 3 partitions\r\n");
    	return XST_FAILURE;
    }

    for (PartitionIdx = 0; PartitionIdx < PartitionCount; PartitionIdx++) {
    	PartHeader *HeaderPtr = &PartitionHeader[PartitionIdx];

		/*
		 * Validate partition header
		 */
		Status = ValidateHeader(HeaderPtr);
		if (Status != XST_SUCCESS) {
			fsbl_printf(DEBUG_GENERAL, "[IPMC-VERIFY]: Header in partition %d failed verification\r\n", PartitionIdx);
			return XST_FAILURE;
		}
    }

    return XST_SUCCESS;
}

