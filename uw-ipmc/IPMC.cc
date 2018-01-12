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
#include <services/ipmi/ipmbsvc/IPMBSvc.h>
#include <services/ipmi/ipmbsvc/IPMICommandParser.h>
#include <drivers/ps_spi/PSSPI.h>
#include <drivers/spi_eeprom/SPIEEPROM.h>
#include <services/persistentstorage/PersistentStorage.h>
#include <drivers/tracebuffer/TraceBuffer.h>
#include <libs/LogTree.h>

/* Xilinx includes. */
#include "xparameters.h"
#include "xscutimer.h"
#include "xscugic.h"
#include "xil_exception.h"
#include "xgpiops.h"

PS_UART *uart_ps0;
IPMBSvc *ipmb0;
IPMICommandParser ipmi_command_parser;
XGpioPs gpiops;
LogTree LOG("ipmc");
LogTree::Filter *console_log_filter;
#define TRACEBUFFER_SIZE (1*1024*1024)
static uint8_t tracebuffer_contents[TRACEBUFFER_SIZE];
TraceBuffer TRACE(tracebuffer_contents, TRACEBUFFER_SIZE);
SPI_EEPROM *eeprom_mac;
SPI_EEPROM *eeprom_data;
PersistentStorage *persistent_storage;

static void console_log_handler(LogTree &logtree, const std::string &message, enum LogTree::LogLevel level);
static void tracebuffer_log_handler(LogTree &logtree, const std::string &message, enum LogTree::LogLevel level);

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
	/* Connect the TraceBuffer to the log system
	 *
	 * We don't need to keep a reference.  This will never require adjustment.
	 */
	new LogTree::Filter(LOG, tracebuffer_log_handler, LogTree::LOG_TRACE);

	/* Initialize the UART console.
	 *
	 * We use a largeish output buffer to avoid overruns during the startup
	 * sequence, since it can't be flushed properly until interrupts are enabled
	 * when the FreeRTOS scheduler starts.  We've got the space.
	 */
	uart_ps0 = new PS_UART(XPAR_PS7_UART_0_DEVICE_ID, XPAR_PS7_UART_0_INTR, 4096, 128*1024);
	console_log_filter = new LogTree::Filter(LOG, console_log_handler, LogTree::LOG_NOTICE);

	PS_SPI *ps_spi0 = new PS_SPI(XPAR_PS7_SPI_0_DEVICE_ID, XPAR_PS7_SPI_0_INTR);
	eeprom_data = new SPI_EEPROM(*ps_spi0, 0, 0x8000, 64);
	eeprom_mac = new SPI_EEPROM(*ps_spi0, 1, 0x100, 16);
	persistent_storage = new PersistentStorage(*eeprom_data, LOG["persistent_storage"]);

	XGpioPs_Config* gpiops_config = XGpioPs_LookupConfig(XPAR_PS7_GPIO_0_DEVICE_ID);
	configASSERT(XST_SUCCESS == XGpioPs_CfgInitialize(&gpiops, gpiops_config, gpiops_config->BaseAddr));

	const int hwaddr_gpios[] = {39,40,41,45,47,48,49,50};
	uint8_t hwaddr = IPMBSvc::lookup_ipmb_address(hwaddr_gpios);
	LogTree &log_ipmb0 = LOG["ipmi"]["ipmb"]["ipmb0"];
	log_ipmb0.log(stdsprintf("Our IPMB0 hardware address is %02Xh", hwaddr), LogTree::LOG_NOTICE);
	PS_IPMB *ps_ipmb[2];
	ps_ipmb[0] = new PS_IPMB(XPAR_PS7_I2C_0_DEVICE_ID, XPAR_PS7_I2C_0_INTR, hwaddr);
	ps_ipmb[1] = new PS_IPMB(XPAR_PS7_I2C_1_DEVICE_ID, XPAR_PS7_I2C_1_INTR, hwaddr);
	ipmb0 = new IPMBSvc(ps_ipmb[0], ps_ipmb[1], hwaddr, &ipmi_command_parser, log_ipmb0, "ipmb0");
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


void* operator new(std::size_t n) {
	return pvPortMalloc(n);
}
void operator delete(void* p) throw() {
	vPortFree(p);
}

/**
 * This handler copies log messages to the UART.
 */
static void console_log_handler(LogTree &logtree, const std::string &message,
		enum LogTree::LogLevel level) {
	/* We use this silly method of concatenation to avoid printf length limits.
	 * See libwrap.cc
	 */
	std::string logmsg = stdsprintf("[%4.4s] ", LogTree::LogLevel_strings[level]) + message + "\n";
	/* We write with 0 timeout, because we'd rather lose lines than hang on UART
	 * output.  That's what the tracebuffer is for anyway
	 */
	windows_newline(logmsg);
	uart_ps0->write(logmsg.data(), logmsg.size(), 0);
}

/**
 * This handler copies log messages to the tracebuffer.
 */
static void tracebuffer_log_handler(LogTree &logtree, const std::string &message, enum LogTree::LogLevel level) {
	TRACE.log(logtree.path.data(), logtree.path.size(), level, message.data(), message.size());
}
