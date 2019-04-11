/**
 * This file is intended to contain foundational IPMC structures.  This mainly
 * consists of allocations of common extern driver instances, and the core init
 * functions.
 */

/* Include system related */
#include <FreeRTOS.h>
#include <IPMC.h>
#include <task.h>
#include <semphr.h>
#include <event_groups.h>
#include <algorithm>
#include <functional>
#include <alloca.h>
#include <sys/time.h>

/* Include Xilinx related */
#include "xparameters.h"
#include "xscutimer.h"
#include "xscugic.h"
#include "xil_exception.h"
#include "xgpiops.h"

/* Include libs */
#include <libs/LogTree.h>
#include <libs/printf.h>
#include "libs/Utils.h"
#include "libs/XilinxImage.h"
#include "libs/Authentication.h"
#include "libs/base64/base64.h"
#include <libs/BackTrace.h>

/* Include drivers */
#include <drivers/ps_gpio/PSGPIO.h>
#include <drivers/ps_xadc/PSXADC.h>
#include <drivers/ps_uart/PSUART.h>
#include <drivers/ps_spi/PSSPI.h>
#include <drivers/ps_qspi/PSQSPI.h>
#include <drivers/pl_uart/PLUART.h>
#include <drivers/pl_gpio/PLGPIO.h>
#include <drivers/pl_spi/PLSPI.h>
#include <drivers/watchdog/PSWDT.h>
#include <drivers/network/Network.h>
#include <drivers/spi_flash/SPIFLASH.h>
#include <drivers/spi_eeprom/SPIEEPROM.h>
#include <drivers/tracebuffer/TraceBuffer.h>
#include <drivers/pim400/PIM400.h>
#include <drivers/esm/ESM.h>
#include <drivers/elm/ELM.h>
#include <drivers/ad7689/AD7689.h>
#include <drivers/pl_led/PLLED.h>
#include <drivers/ltc2654f/LTC2654F.h>

/* Include services */
#include <services/persistentstorage/PersistentStorage.h>
#include <services/console/UARTConsoleSvc.h>
#include <services/influxdb/InfluxDB.h>
#include <services/telnet/Telnet.h>
#include <services/lwiperf/Lwiperf.h>
#include <services/xvcserver/XVCServer.h>
#include <services/sntp/sntp.h>
#include <services/ftp/FTPServer.h>


u8 IPMC_HW_REVISION = 0;
uint16_t IPMC_SERIAL = 0xffff;
u8 IMAGE_LOADED = 0;

/**
 * A FreeRTOS EventGroup initialized by main() before the scheduler starts.
 *
 * [1] 1b = ipmc_service_init() has exited.
 * [0] 1b = driver_init() has exited.
 */
EventGroupHandle_t init_complete;

PS_WDT *SWDT = NULL;
PS_UART *uart_ps0;
PS_QSPI *psqspi;

LogTree LOG("ipmc");
LogTree::Filter *console_log_filter;

SPI_EEPROM *eeprom_mac;
SPI_EEPROM *eeprom_data;
PersistentStorage *persistent_storage;

CommandParser console_command_parser;
std::shared_ptr<UARTConsoleSvc> console_service;

PL_GPIO *handle_gpio;

u8 mac_address[6];
Network *network;
TelnetServer *telnet;

AD7689 *adc[3];
PS_XADC *xadc;

GPIO *gpio[6] = {0};

Flash *qspiflash = nullptr;
bool wasFlashUpgradeSuccessful = true;

static void tracebuffer_log_handler(LogTree &logtree, const std::string &message, enum LogTree::LogLevel level);
static void register_core_console_commands(CommandParser &parser);
static void console_log_handler(LogTree &logtree, const std::string &message, enum LogTree::LogLevel level);
static void watchdog_ontrip();

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

	// Initialize the watchdog.
	SWDT = new PS_WDT(XPAR_PS7_WDT_0_DEVICE_ID, 8, LOG["watchdog"], watchdog_ontrip);

	// Initialize the UART console.
	/* We use a large outbuf to prevent bursts of log messages (such as IPMI
	 * logs from FRU Data reads) from overflowing.
	 */
	uart_ps0 = new PS_UART(XPAR_PS7_UART_0_DEVICE_ID, XPAR_PS7_UART_0_INTR, 4096, 1<<16);
	console_log_filter = new LogTree::Filter(LOG, console_log_handler, LogTree::LOG_NOTICE);
	console_log_filter->register_console_commands(console_command_parser);
	LOG["console_log_command"].register_console_commands(console_command_parser);

	psqspi = new PS_QSPI(XPAR_PS7_QSPI_0_DEVICE_ID, XPAR_PS7_QSPI_0_INTR);
#ifdef INCLUDE_DRIVER_COMMAND_SUPPORT
	psqspi->register_console_commands(console_command_parser, "psqspi.");
#endif

	// Retrieve the hardware revision number
	PS_GPIO gpio_hwrev(XPAR_PS7_GPIO_0_DEVICE_ID, {0}); // Only pin 0
	IPMC_HW_REVISION = (gpio_hwrev.getBus() == 0)? 1 : 0; // Pull-down on revB

#define REBOOT_STATUS_REG (XPS_SYS_CTRL_BASEADDR + 0x258)
	u32 reboot_status = Xil_In32(REBOOT_STATUS_REG) >> 24;
	if (reboot_status & 0x4) IMAGE_LOADED = 3;
	else IMAGE_LOADED = reboot_status & 0x3;
	if (IPMC_HW_REVISION == 0) IMAGE_LOADED = 0;

	// Retrieve the IPMB address
	PS_GPIO gpio_ipmbaddr(XPAR_PS7_GPIO_0_DEVICE_ID, {39,40,41,45,47,48,49,50});
	uint8_t ipmbaddr = gpio_ipmbaddr.getBus(), parity;
	for (size_t i = 0; i < 8; ++i) parity ^= ((ipmbaddr >> i) & 0x1);
	// TOOD: Validate parity
	ipmbaddr = (ipmbaddr & 0x7f) << 1; // The high HA bit on the Zone 1 connector is parity.  IPMB addr is HWaddr<<1

	PS_SPI *ps_spi0 = new PS_SPI(XPAR_PS7_SPI_0_DEVICE_ID, XPAR_PS7_SPI_0_INTR);
	eeprom_data = new SPI_EEPROM(*ps_spi0, 0, 0x8000, 64);
	eeprom_mac = new SPI_EEPROM(*ps_spi0, 1, 0x100, 16);
	persistent_storage = new PersistentStorage(*eeprom_data, LOG["persistent_storage"], SWDT);
	persistent_storage->register_console_commands(console_command_parser, "eeprom.");
	configASSERT(eeprom_mac->read(250, mac_address, 6));
	LOG["network"].log(stdsprintf("Our MAC address is %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
			mac_address[0], mac_address[1], mac_address[2], mac_address[3], mac_address[4], mac_address[5]), LogTree::LOG_NOTICE);
	configASSERT(eeprom_mac->read(0, reinterpret_cast<uint8_t*>(&IPMC_SERIAL), sizeof(IPMC_SERIAL)));

	xadc = new PS_XADC(XPAR_XADCPS_0_DEVICE_ID);

	// Initialize the QSPI flash driver
	qspiflash = new SPIFlash(*psqspi, 0);
	qspiflash->initialize();

	gpio[4] = new PS_GPIO(XPAR_PS7_GPIO_0_DEVICE_ID, {10,11,12,13});
	gpio[5] = new PS_GPIO(XPAR_PS7_GPIO_0_DEVICE_ID, {39,40,41,45,47,48,49,50});

	// TODO: Clean up this part
	if (use_pl) {
		for (int i = 0; i < 2; i++) {
			adc[i] = new AD7689(XPAR_AD7689_S_0_DEVICE_ID + i, 0);
			adc[i]->register_console_commands(console_command_parser, stdsprintf("adc%d.", i));
		}

		for (int i = 0; i < 4; i++) {
			gpio[i] = new PL_GPIO(PL_GPIO::CHANNEL1, XPAR_AXI_GPIO_0_DEVICE_ID + i);
		}

		XVCServer *xvc = new XVCServer(XPAR_AXI_JTAG_0_BASEADDR);
	}

	for (int i = 0; i < 6; i++) {
		gpio[i]->register_console_commands(console_command_parser, stdsprintf("gpio%d.", i));
	}
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
	console_service = UARTConsoleSvc::create(*uart_ps0, console_command_parser, "console", LOG["console"]["uart"], true);

	// Last services should be network related
	network = new Network(LOG["network"], mac_address, [](Network *network) {
		// Network Ready callback, start primary services
		sntp_init();

		telnet = new TelnetServer(LOG["telnetd"]);

		// Start iperf server
		new Lwiperf(5001);

		// Start FTP server

#define MB * (1024 * 1024)

		if (qspiflash->getTotalSize() == (64 MB)) {
			VFS::addFile("virtual/fallback.bin", qspiflash->createFlashFile(0 MB, 16 MB));
			VFS::addFile("virtual/A.bin", qspiflash->createFlashFile(16 MB, 16 MB));
			VFS::addFile("virtual/B.bin", qspiflash->createFlashFile(32 MB, 16 MB));
			VFS::addFile("virtual/test.bin", qspiflash->createFlashFile(48 MB, 16 MB));
		} else if (qspiflash->getTotalSize() == (16 MB)) {
			VFS::addFile("virtual/A.bin", qspiflash->createFlashFile(0 MB, 16 MB));
		} else {
			throw std::runtime_error("Unsupported QSPI flash size detected");
		}

#undef MB

		new FTPServer(Auth::ValidateCredentials);

	});
	network->register_console_commands(console_command_parser, "network.");

	// This has to be lower, so the serial number has been read by the time we register (or not register) set_serial.
	register_core_console_commands(console_command_parser);
}

void* operator new(std::size_t n) {
	return malloc(n);
}
void operator delete(void* p) throw() {
	free(p);
}

std::string generate_banner() {
	std::string bannerstr;
	bannerstr += "********************************************************************************\n";
	bannerstr += "\n";
	bannerstr += std::string("ZYNQ-IPMC - Open-source IPMC hardware and software framework\n");
	bannerstr += std::string("HW revision : rev") + (char)('A'+IPMC_HW_REVISION) + "\n";
	bannerstr += std::string("SW revision : ") + GIT_DESCRIBE + " (" + GIT_BRANCH + ")\n";
	if (IPMC_SERIAL != 0xffff & IPMC_SERIAL != 0)
		bannerstr += std::string("HW serial   : ") + std::to_string(IPMC_SERIAL) + "\n";
	else
		bannerstr += std::string("HW serial   : unset\n");
	bannerstr += std::string("Build date  : ") + COMPILE_DATE + "\n";
	bannerstr += std::string("Build host  : ") + COMPILE_HOST + "\n";
	bannerstr += std::string("Build conf  : ") + BUILD_CONFIGURATION + "\n";
	bannerstr += std::string("OS version  : FreeRTOS ") + tskKERNEL_VERSION_NUMBER + "\n";

	const char* imageNames[] = {"fallback", "A", "B", "test"};
	bannerstr += std::string("Flash image : ") + ((IMAGE_LOADED > 3)?"Unknown":imageNames[IMAGE_LOADED]) + " (" + std::to_string(IMAGE_LOADED) + ")\n";

	if (GIT_STATUS[0] != '\0')
		bannerstr += std::string("\n") + GIT_STATUS; // contains a trailing \n
	bannerstr += "\n";
	bannerstr += "********************************************************************************\n";
	return bannerstr;
}

/**
 * This handler copies log messages to the tracebuffer.
 */
static void tracebuffer_log_handler(LogTree &logtree, const std::string &message, enum LogTree::LogLevel level) {
	TRACE.log(logtree.path.data(), logtree.path.size(), level, message.data(), message.size());
}

#include "core_console_commands/date.h"
#include <core_console_commands/flash.h>
#include "core_console_commands/ps.h"
#include "core_console_commands/restart.h"
#include "core_console_commands/setauth.h"
#include "core_console_commands/set_serial.h"
#include "core_console_commands/throw.h"
#include "core_console_commands/upload.h"
#include "core_console_commands/uptime.h"
#include "core_console_commands/version.h"
#include "core_console_commands/boottarget.h"

#include "blade_console_commands/adc.h"

static void register_core_console_commands(CommandParser &parser) {
	console_command_parser.register_command("uptime", std::make_shared<ConsoleCommand_uptime>());
	console_command_parser.register_command("date", std::make_shared<ConsoleCommand_date>());
	console_command_parser.register_command("version", std::make_shared<ConsoleCommand_version>());
	console_command_parser.register_command("ps", std::make_shared<ConsoleCommand_ps>());
	console_command_parser.register_command("restart", std::make_shared<ConsoleCommand_restart>());
	console_command_parser.register_command("flash.info", std::make_shared<ConsoleCommand_flash_info>());
	console_command_parser.register_command("flash.verify", std::make_shared<ConsoleCommand_flash_verify>(*qspiflash));
	console_command_parser.register_command("setauth", std::make_shared<ConsoleCommand_setauth>());
	if (IPMC_SERIAL == 0 || IPMC_SERIAL == 0xFFFF) // The serial is settable only if unset.  This implements lock on write (+reboot).
		console_command_parser.register_command("set_serial", std::make_shared<ConsoleCommand_set_serial>());
	console_command_parser.register_command("upload", std::make_shared<ConsoleCommand_upload>());
	console_command_parser.register_command("throw", std::make_shared<ConsoleCommand_throw>());
	console_command_parser.register_command("trace", std::make_shared<ConsoleCommand_trace>());
	console_command_parser.register_command("boottarget", std::make_shared<ConsoleCommand_boottarget>(eeprom_mac));

	console_command_parser.register_command("adc", std::make_shared<ConsoleCommand_adc>());
	StatCounter::register_console_commands(parser);
}

static void console_log_handler(LogTree &logtree, const std::string &message, enum LogTree::LogLevel level) {
	std::string logmsg = ConsoleSvc_log_format(message, level);

	/* We write with 0 timeout, because we'd rather lose lines than hang on UART
	* output.  That's what the tracebuffer is for anyway.
	*/
	if (!console_service || IN_INTERRUPT() || IN_CRITICAL()) {
		// Still early startup.
		windows_newline(logmsg);
		uart_ps0->write((const u8*)logmsg.data(), logmsg.size(), 0);
	}
	else {
		// We have to use a short timeout here, rather than none, due to the mutex involved.
		// TODO: Maybe there's a better way?
		console_service->write(logmsg, 1);
	}
}

static void watchdog_ontrip() {
	LOG["watchdog"].log(std::string("\n")+ConsoleCommand_ps::get_ps_string(), LogTree::LOG_NOTICE);
}
