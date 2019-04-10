/*
 * ipmc.c
 *
 *  Created on: Apr 4, 2019
 *      Author: mpv
 */

#include "ipmc.h"
#include "fsbl.h"

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



u8 get_ipmc_target_image() {
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

void tag_image(u8 image) {
	u32 RebootStatusRegister = 0;

	RebootStatusRegister = Xil_In32(REBOOT_STATUS_REG);
	RebootStatusRegister &= 0x0F000000;

	image &= 0x0F;

	RebootStatusRegister |= (image << 24);

	Xil_Out32(REBOOT_STATUS_REG, RebootStatusRegister);
}
