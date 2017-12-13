/**
 * This file is intended to contain foundational IPMC structures.  This mainly
 * consists of allocations of common extern driver instances, and the core init
 * functions.
 */

#include <FreeRTOS.h>
#include <task.h>
#include <IPMC.h>
#include <drivers/ps_uart/PSUART.h>
#include <drivers/ps_ipmb/PSIPMB.h>
#include <drivers/ipmb0/IPMB0.h>

/* Xilinx includes. */
#include "xparameters.h"
#include "xscutimer.h"
#include "xscugic.h"
#include "xil_exception.h"
#include "xgpiops.h"

PS_UART *uart_ps0;
IPMB0 *ipmb0;
XGpioPs gpiops;

/** Stage 1 driver initialization.
 *
 * This function contains initialization for base hardware drivers.  It may or
 * may not activate or enable features.  It should not depend on any service,
 * nor make any service connections.  This will be called in the bootloader
 * application project as well, where most IPMC services will not be run.
 *
 * \param use_pl  Select whether or not the PL is loaded and PL drivers should
 *                be initialized.
 *
 * \note This function is called before the FreeRTOS scheduler has been started.
 */
void driver_init(bool use_pl) {
	// Initialize the UART console.
	uart_ps0 = new PS_UART(XPAR_PS7_UART_0_DEVICE_ID, XPAR_PS7_UART_0_INTR);

	XGpioPs_Config* gpiops_config = XGpioPs_LookupConfig(XPAR_PS7_GPIO_0_DEVICE_ID);
	configASSERT(XST_SUCCESS == XGpioPs_CfgInitialize(&gpiops, gpiops_config, gpiops_config->BaseAddr));

	// Initialize test IPMB_[AB]
	PS_IPMB *ps_ipmb[2];
	ps_ipmb[0] = new PS_IPMB(XPAR_PS7_I2C_0_DEVICE_ID, XPAR_PS7_I2C_0_INTR, 0x72);
	ps_ipmb[1] = new PS_IPMB(XPAR_PS7_I2C_1_DEVICE_ID, XPAR_PS7_I2C_1_INTR, 0x72);
	const int hwaddr_gpios[] = {39,40,41,45,47,48,49,50};
	uint8_t hwaddr = IPMB0::lookup_ipmb_address(hwaddr_gpios);
	printf("Our IPMB0 hardware address is %02X\r\n", hwaddr);
	ipmb0 = new IPMB0(ps_ipmb[0], ps_ipmb[1], hwaddr);
}

/** IPMC service initialization.
 *
 * This function contains the initialization for IPMC services, and is
 * responsible for connecting and enabling/activating drivers and IPMC related
 * services.  It will not be called from the bootloader or non-IPMC application
 * projects, and the PL is assumed to be loaded.
 *
 * \note This function is called before the FreeRTOS scheduler has been started.
 */
void ipmc_service_init() {

}

/**
 * Calculate an IPMI checksum of an array of bytes.
 *
 * \note You can verify a checksum by ensuring that the computed checksum of the
 *       data buffer with the checksum included, is zero.
 *
 * @param buf The buffer to compute the checksum of.
 * @param len The length of the buffer to compute the checksum of.
 * @return The one-byte IPMI checksum.
 */
u8 ipmi_checksum(const u8* buf, u32 len) {
  u8 sum = 0;

  while (len) {
    sum += *buf++;
    len--;
  }
  return (~sum) + 1;
}


void* operator new(std::size_t n) {
	return pvPortMalloc(n);
}
void operator delete(void* p) throw() {
	vPortFree(p);
}
