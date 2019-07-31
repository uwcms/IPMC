/*
 * This file is part of the ZYNQ-IPMC Framework.
 *
 * The ZYNQ-IPMC Framework is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The ZYNQ-IPMC Framework is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the ZYNQ-IPMC Framework.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <core.h>
#include <drivers/ipmb/ipmb_pair.h>
#include <drivers/ipmb/ps_ipmb.h>
#include <drivers/ps_gpio/ps_gpio.h>
#include <drivers/ps_spi/ps_spi.h>
#include <drivers/spi_flash/spi_flash.h>
#include <libs/printf.h>
#include <services/console/uartconsolesvc.h>
#include <services/ipmi/commands/ipmicmd_index.h>
#include <zynqipmc_config.h>

#include "../../board_payload_manager.h"

#define REBOOT_STATUS_REG (XPS_SYS_CTRL_BASEADDR + 0x258)

/**
 * A FreeRTOS EventGroup initialized by main() before the scheduler starts.
 *
 * [1] 1b = ipmc_service_init() has exited.
 * [0] 1b = driver_init() has exited.
 */
EventGroupHandle_t init_complete;

/**
 * Global variables
 */
LogTree LOG("ipmc");

PSWDT   *swdt    = nullptr;
PSUART  *psuart0 = nullptr;
Flash   *qspiflash = nullptr;

SPIEEPROM *eeprom_mac;
PersistentStorage *persistent_storage;
uint8_t mac_address[6];

uint8_t IPMC_HW_REVISION = 0;
uint16_t IPMC_SERIAL = 0xffff;
uint8_t IMAGE_LOADED = 0;

IPMBSvc *ipmb0;
IPMBSvc::EventReceiver ipmi_event_receiver;
IPMICommandParser *ipmi_command_parser;

MStateMachine *mstatemachine = nullptr;

SensorDataRepository sdr_repo;
SensorDataRepository device_sdr_repo;
SensorSet ipmc_sensors(&device_sdr_repo);

SemaphoreHandle_t fru_data_mutex;
std::vector<uint8_t> fru_data;

PayloadManager *payload_manager = nullptr;

// Core-specific variables
LogTree::Filter *console_log_filter = nullptr;
CommandParser console_command_parser;

static std::shared_ptr<UARTConsoleSvc> console_service;
static uint8_t tracebuffer_contents[TRACEBUFFER_SIZE];
static TraceBuffer *trace_buffer = nullptr;
static uint8_t tracebuffer_object_memory[sizeof(TraceBuffer)];

std::vector<IPMILED*> ipmi_leds;  // Blue, Red, Green, Amber

static void console_log_handler(LogTree &logtree, const std::string &message, enum LogTree::LogLevel level) {
	std::string logmsg = consoleSvcLogFormat(message, level);

	/* We write with 0 timeout, because we'd rather lose lines than hang on UART
	* output.  That's what the tracebuffer is for anyway.
	*/
	if (!console_service || IN_INTERRUPT() || IN_CRITICAL()) {
		// Still early startup.
		windows_newline(logmsg);
		psuart0->write((const u8*)logmsg.data(), logmsg.size(), 0);
	}
	else {
		// We have to use a short timeout here, rather than none, due to the mutex involved.
		// TODO: Maybe there's a better way?
		console_service->write(logmsg, 1);
	}
}

__attribute__((weak)) void initFruData(bool reinit) {
	LOG.log("initFruData is not implemented in the user code, this might be a bug if IPMI operation is desired!", LogTree::LOG_WARNING);
}

__attribute__((weak)) void initDeviceSDRs(bool reinit) {
	LOG.log("initFruData is not implemented in the user code, this might be a bug if IPMI operation is desired!", LogTree::LOG_WARNING);
}

__attribute__((weak)) std::string generateBanner() {
	std::string bannerstr;
	bannerstr += "********************************************************************************\n";
	bannerstr += "\n";
	bannerstr += std::string("ZYNQ-IPMC - Open-source IPMC hardware and software framework\n");
	bannerstr += std::string("HW revision : rev") + (char)('A'+IPMC_HW_REVISION) + "\n";
	bannerstr += std::string("SW revision : ") + GIT_DESCRIBE + " (" + GIT_BRANCH + ")\n";
	if ((IPMC_SERIAL != 0xffff) & (IPMC_SERIAL != 0)) {
		bannerstr += std::string("HW serial   : ") + std::to_string(IPMC_SERIAL) + "\n";
	} else {
		bannerstr += std::string("HW serial   : unset\n");
	}
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

void startInitTask() {
	init_complete = xEventGroupCreate();

	runTask("init", TASK_PRIORITY_WATCHDOG, []() -> void {
		core_driver_init();
		xEventGroupSetBits(init_complete, 0x01);
		core_service_init();
		xEventGroupSetBits(init_complete, 0x02);
		LOG.log(std::string("\n") + generateBanner(), LogTree::LOG_NOTICE); // This is the ONLY place that should EVER log directly to LOG rather than a subtree.
	});
}

// TODO: Check if new returns are not nullptr
void core_driver_init() {
	u32 reboot_status = Xil_In32(REBOOT_STATUS_REG) >> 24;
	if (reboot_status & 0x4) IMAGE_LOADED = 3;
	else IMAGE_LOADED = reboot_status & 0x3;
	if (IPMC_HW_REVISION == 0) IMAGE_LOADED = 0;

	/* Connect the TraceBuffer to the log system
	 *
	 * We don't need to keep a reference.  This will never require adjustment.
	 */
	new LogTree::Filter(LOG, tracebufferLogHandler, LogTree::LOG_TRACE);

#ifdef ENABLE_WATCHDOGTIMER
	// Initialize the watchdog.
	swdt = new PSWDT(XPAR_PS7_WDT_0_DEVICE_ID, 8, LOG["watchdog"], watchdogOnTrip);
#endif

	// Initialize the UART console.
	/* We use a large outbuf to prevent bursts of log messages (such as IPMI
	 * logs from FRU Data reads) from overflowing.
	 */
	psuart0 = new PSUART(XPAR_PS7_UART_0_DEVICE_ID, XPAR_PS7_UART_0_INTR, 4096, 1<<16);
	console_log_filter = new LogTree::Filter(LOG, console_log_handler, LogTree::LOG_NOTICE);
	console_log_filter->registerConsoleCommands(console_command_parser);
	LOG["console_log_command"].registerConsoleCommands(console_command_parser);

	// QSPI interface to Flash
	PSQSPI *psqspi = new PSQSPI(XPAR_PS7_QSPI_0_DEVICE_ID, XPAR_PS7_QSPI_0_INTR);
#ifdef ENABLE_DRIVER_COMMAND_SUPPORT
	psqspi->registerConsoleCommands(console_command_parser, "psqspi.");
#endif

	// Initialize QSPI flash
	qspiflash = new SPIFlash(*psqspi, 0, LOG["flash"]);
	qspiflash->initialize();

	// Configuration and MAC EEPROM
	PSSPI *ps_spi0 = new PSSPI(XPAR_PS7_SPI_0_DEVICE_ID, XPAR_PS7_SPI_0_INTR);
	SPIEEPROM *eeprom_data = new SPIEEPROM(*ps_spi0, 0, 0x8000, 64);
	eeprom_mac = new SPIEEPROM(*ps_spi0, 1, 0x100, 16);
	persistent_storage = new PersistentStorage(*eeprom_data, LOG["persistent_storage"], swdt);
	persistent_storage->registerConsoleCommands(console_command_parser, "eeprom.");
	configASSERT(eeprom_mac->read(250, mac_address, 6));
	LOG["network"].log(stdsprintf("Our MAC address is %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
			mac_address[0], mac_address[1], mac_address[2], mac_address[3], mac_address[4], mac_address[5]), LogTree::LOG_NOTICE);
	configASSERT(eeprom_mac->read(0, reinterpret_cast<uint8_t*>(&IPMC_SERIAL), sizeof(IPMC_SERIAL)));

	// Retrieve the hardware revision number
	PSGPIO gpio_hwrev(XPAR_PS7_GPIO_0_DEVICE_ID, {0}); // Only pin 0
	IPMC_HW_REVISION = (gpio_hwrev.getBusValue() == 0)? 1 : 0; // Pull-down on revB

	// Configure the XVC pins which are assigned differently from revA to revB
	PSGPIO gpio_xvc_config(XPAR_PS7_GPIO_0_DEVICE_ID, {54, 55});
	gpio_xvc_config.setBusDirection(0);
	if (IPMC_HW_REVISION == 0) gpio_xvc_config.setBusValue(0x3);
	else gpio_xvc_config.setBusValue(0x2);

#ifdef ENABLE_IPMI
	// Retrieve the IPMB address
	PSGPIO gpio_ipmbaddr(XPAR_PS7_GPIO_0_DEVICE_ID, {39,40,41,45,47,48,49,50});
	uint8_t ipmbaddr = gpio_ipmbaddr.getBusValue(), parity;
	for (size_t i = 0; i < 8; ++i) parity ^= ((ipmbaddr >> i) & 0x1);
	// TOOD: Validate parity
	ipmbaddr = (ipmbaddr & 0x7f) << 1; // The high HA bit on the Zone 1 connector is parity.  IPMB addr is HWaddr<<1

	/* SDRs must be initialized here so sensors are available to link up with
	 * their drivers.  FRU Data will be done later, once the PayloadManager is
	 * initialized.  The IPMBSvc thread does not proceed until service init is
	 * done.  SDRs will not be reloaded from EEPROM and will remain in their
	 * default state until the sdr_init thread has time to run.
	 */
	initDeviceSDRs(false);

	LogTree &log_ipmb0 = LOG["ipmi"]["ipmb"]["ipmb0"];
	log_ipmb0.log(stdsprintf("Our IPMB0 address is %02Xh", ipmbaddr), LogTree::LOG_NOTICE);
	PSIPMB *ps_ipmb[2];
	ps_ipmb[0] = new PSIPMB(XPAR_PS7_I2C_0_DEVICE_ID, XPAR_PS7_I2C_0_INTR, ipmbaddr);
	ps_ipmb[1] = new PSIPMB(XPAR_PS7_I2C_1_DEVICE_ID, XPAR_PS7_I2C_1_INTR, ipmbaddr);
	IPMBPair *ipmb0pair = new IPMBPair(ps_ipmb[0], ps_ipmb[1], &(log_ipmb0["outgoing_messages"]));
	ipmi_command_parser = new IPMICommandParser(ipmicmd_default, *ipmicmd_index);
	ipmb0 = new IPMBSvc(*ipmb0pair, ipmbaddr, ipmi_command_parser, log_ipmb0, "ipmb0", swdt);
	ipmb0->registerConsoleCommands(console_command_parser, "ipmb0.");
	ipmi_event_receiver.ipmb = ipmb0;
	ipmi_event_receiver.lun = 0;
	ipmi_event_receiver.addr = 0x20; // Should be `0xFF "Disabled"`, maybe?
#endif

	// Run application specific driver initialization
	driverInit();
}

void core_service_init() {
	console_service = UARTConsoleSvc::create(*psuart0, console_command_parser, "console", LOG["console"]["uart"], true);

#ifdef ENABLE_IPMI
	mstatemachine = new MStateMachine(std::dynamic_pointer_cast<HotswapSensor>(ipmc_sensors.findByName("Hotswap")), *ipmi_leds[0], LOG["mstatemachine"]);
	mstatemachine->registerConsoleCommands(console_command_parser, "");

	// TODO: Change this?
	payload_manager = new BoardPayloadManager(mstatemachine, LOG["payload_manager"]);
	payload_manager->config();

	payload_manager->registerConsoleCommands(console_command_parser, "payload.");
	// IPMC Sensors have been instantiated already, so we can do this linkage now.
	payload_manager->refreshSensorLinkage();

	/* SDRs must be initialized earlier so sensors are available to link up with
	 * their drivers.  FRU Data will be done here, once the PayloadManager is
	 * initialized.  The IPMBSvc thread does not proceed until service init is
	 * done.
	 *
	 * If the `reinit` parameter is set to true, changes to the FRU Data area
	 * stored in persistent storage will be replaced on startup, otherwise
	 * FRU Data will be created only if it is absent.  In that case it is the
	 * system operator's responsibility to ensure FRU Data is reinitialized or
	 * updated as necessary.
	 */
	initFruData(true);
#endif

#define MB * (1024 * 1024)

	// Set the virtual file system with default flash partitions
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

	// Run application specific service initialization
	serviceInit();
}

// TODO
#include <core_console_commands/ps.h>

__attribute__((weak)) void watchdogOnTrip() {
	LOG["watchdog"].log(std::string("\n")+ConsoleCommand_ps::get_ps_string(), LogTree::LOG_NOTICE);
}

__attribute__((weak)) void tracebufferLogHandler(LogTree &logtree, const std::string &message, enum LogTree::LogLevel level) {
	TRACE.log(logtree.getPath().data(), logtree.getPath().size(), level, message.data(), message.size());
}

/* We will want this to run as a very early constructor regardless, so that no
 * matter how early in the boot process we crash, chances are the tracebuffer
 * data is still valid, even if it is empty.
 *
 * 101 is the earliest non-reserved constructor priority value, according to my
 * understanding.
 */
TraceBuffer& getTraceBuffer() __attribute__((constructor(101)));

/**
 * Instantiate (if required) and retrieve the TraceBuffer.
 * @return A functional TraceBuffer by reference.
 */
TraceBuffer& getTraceBuffer() {
	// Hopefully this will all just be done already and ready to use.
	if (trace_buffer)
		return *trace_buffer;

	CriticalGuard critical(true);
	/* Sadly can't get away with doing this in advance, because it'll modify the
	 * static array.  Luckily it's a quick operation.
	 *
	 * Unfortunately we probably also can't call new, because this might even
	 * be happening in an ISR.  Therefore we will use placement new on a
	 * preallocated buffer.
	 */
	if (!trace_buffer)
		trace_buffer = new (tracebuffer_object_memory) TraceBuffer(tracebuffer_contents, TRACEBUFFER_SIZE);
	return *trace_buffer;
}
