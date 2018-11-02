/**
 * This file is intended to contain foundational IPMC structures.  This mainly
 * consists of allocations of common extern driver instances, and the core init
 * functions.
 */

/* Include system related */
#include <FreeRTOS.h>
#include <IPMC.h>
#include <task.h>
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
#include "libs/Utils.h"
#include "libs/XilinxImage.h"
#include "libs/Authentication.h"
#include <services/ipmi/IPMIFormats.h>

/* Include drivers */
#include <drivers/ps_xadc/PSXADC.h>
#include <drivers/ps_uart/PSUART.h>
#include <drivers/ps_spi/PSSPI.h>
#include <drivers/ps_isfqspi/PSISFQSPI.h>
#include <drivers/pl_uart/PLUART.h>
#include <drivers/pl_gpio/PLGPIO.h>
#include <drivers/pl_spi/PLSPI.h>
#include <drivers/watchdog/PSWDT.h>
#include <drivers/network/Network.h>
#include <drivers/spi_flash/SPIFLASH.h>
#include <drivers/spi_eeprom/SPIEEPROM.h>
#include <drivers/tracebuffer/TraceBuffer.h>
#include <drivers/ipmb/IPMBPair.h>
#include <drivers/ipmb/PSIPMB.h>
#include <drivers/mgmt_zone/MGMTZone.h>
#include <drivers/pim400/PIM400.h>
#include <drivers/esm/ESM.h>
#include <drivers/ad7689/AD7689.h>
extern "C" {
#include "led_controller.h"
}

/* Include services */
#include <services/ipmi/ipmbsvc/IPMBSvc.h>
#include <services/ipmi/ipmbsvc/IPMICommandParser.h>
#include <services/ipmi/commands/IPMICmd_Index.h>
#include <services/ipmi/sdr/SensorDataRepository.h>
#include <services/ipmi/sdr/SensorDataRecord01.h>
#include <services/ipmi/sdr/SensorDataRecord02.h>
#include <services/ipmi/sdr/SensorDataRecord12.h>
#include <services/persistentstorage/PersistentStorage.h>
#include <services/console/UARTConsoleSvc.h>
#include <services/influxdb/InfluxDB.h>
#include <services/telnet/Telnet.h>
#include <services/lwiperf/Lwiperf.h>
#include <services/xvcserver/XVCServer.h>
#include <services/sntp/sntp.h>
#include <services/ftp/FTPServer.h>


u8 IPMC_HW_REVISION = 1; // TODO: Detect, Update, etc
uint16_t IPMC_SERIAL = 0xffff;

PS_WDT *SWDT;
PS_UART *uart_ps0;
MGMT_Zone *mgmt_zones[XPAR_MGMT_ZONE_CTRL_0_MZ_CNT];
PS_ISFQSPI *isfqspi;
IPMBSvc *ipmb0;
EventReceiver ipmi_event_receiver;
Network *network;
IPMICommandParser *ipmi_command_parser;
SensorDataRepository sdr_repo;
SensorDataRepository device_sdr_repo;
XGpioPs gpiops;
LogTree LOG("ipmc");
LogTree::Filter *console_log_filter;
#define TRACEBUFFER_SIZE (1*1024*1024)
static uint8_t tracebuffer_contents[TRACEBUFFER_SIZE];
TraceBuffer TRACE(tracebuffer_contents, TRACEBUFFER_SIZE);
SPI_EEPROM *eeprom_mac;
SPI_EEPROM *eeprom_data;
PersistentStorage *persistent_storage;
u8 mac_address[6];
CommandParser console_command_parser;
std::shared_ptr<UARTConsoleSvc> console_service;
InfluxDB *influxdbclient;

TelnetServer *telnet;

ESM *esm;
PL_GPIO *handle_gpio;
LED_Controller atcaLEDs;

AD7689 *adc[2];

PS_XADC *xadc;

bool firmwareUpdateFailed = false;

// External static variables
extern uint8_t mac_address[6]; ///< The MAC address of the board, read by persistent storage statically

static void init_device_sdrs(bool reinit=false);
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

	/* Initialize the UART console.
	 *
	 * We use a largeish output buffer to avoid overruns during the startup
	 * sequence, since it can't be flushed properly until interrupts are enabled
	 * when the FreeRTOS scheduler starts.  We've got the space.
	 */
	uart_ps0 = new PS_UART(XPAR_PS7_UART_0_DEVICE_ID, XPAR_PS7_UART_0_INTR, 4096, 32768);
	console_log_filter = new LogTree::Filter(LOG, console_log_handler, LogTree::LOG_NOTICE);
	console_log_filter->register_console_commands(console_command_parser);
	LOG["console_log_command"].register_console_commands(console_command_parser);

	isfqspi = new PS_ISFQSPI(XPAR_PS7_QSPI_0_DEVICE_ID, XPAR_PS7_QSPI_0_INTR);

	PS_SPI *ps_spi0 = new PS_SPI(XPAR_PS7_SPI_0_DEVICE_ID, XPAR_PS7_SPI_0_INTR);
	eeprom_data = new SPI_EEPROM(*ps_spi0, 0, 0x8000, 64);
	eeprom_mac = new SPI_EEPROM(*ps_spi0, 1, 0x100, 16);
	persistent_storage = new PersistentStorage(*eeprom_data, LOG["persistent_storage"], SWDT);
	persistent_storage->register_console_commands(console_command_parser, "eeprom.");
	configASSERT(eeprom_mac->read(250, mac_address, 6));
	LOG["network"].log(stdsprintf("Our MAC address is %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
			mac_address[0], mac_address[1], mac_address[2], mac_address[3], mac_address[4], mac_address[5]), LogTree::LOG_NOTICE);
	configASSERT(eeprom_mac->read(0, reinterpret_cast<uint8_t*>(&IPMC_SERIAL), sizeof(IPMC_SERIAL)));

	// This has to be lower, so the serial number has been read by the time we register (or not register) set_serial.
	register_core_console_commands(console_command_parser);

	UWTaskCreate("init_sdr", TASK_PRIORITY_SERVICE, std::bind(init_device_sdrs, false));

	XGpioPs_Config* gpiops_config = XGpioPs_LookupConfig(XPAR_PS7_GPIO_0_DEVICE_ID);
	configASSERT(XST_SUCCESS == XGpioPs_CfgInitialize(&gpiops, gpiops_config, gpiops_config->BaseAddr));

	const int hwaddr_gpios[] = {39,40,41,45,47,48,49,50};
	uint8_t ipmbaddr = IPMBSvc::lookup_ipmb_address(hwaddr_gpios);
	LogTree &log_ipmb0 = LOG["ipmi"]["ipmb"]["ipmb0"];
	log_ipmb0.log(stdsprintf("Our IPMB0 address is %02Xh", ipmbaddr), LogTree::LOG_NOTICE);
	PS_IPMB *ps_ipmb[2];
	ps_ipmb[0] = new PS_IPMB(XPAR_PS7_I2C_0_DEVICE_ID, XPAR_PS7_I2C_0_INTR, ipmbaddr);
	ps_ipmb[1] = new PS_IPMB(XPAR_PS7_I2C_1_DEVICE_ID, XPAR_PS7_I2C_1_INTR, ipmbaddr);
	IPMBPair *ipmb0pair = new IPMBPair(ps_ipmb[0], ps_ipmb[1], &(log_ipmb0["outgoing_messages"]));
	ipmi_command_parser = new IPMICommandParser(ipmicmd_default, *ipmicmd_index);
	ipmb0 = new IPMBSvc(ipmb0pair, ipmbaddr, ipmi_command_parser, log_ipmb0, "ipmb0", SWDT);
	ipmb0->register_console_commands(console_command_parser, "ipmb0.");
	ipmi_event_receiver.ipmb = NULL;
	ipmi_event_receiver.lun = 0;
	ipmi_event_receiver.addr = 0;

	// TODO: Clean up this part
	PL_I2C *i2c = new PL_I2C(XPAR_AXI_IIC_PIM400_DEVICE_ID, XPAR_FABRIC_AXI_IIC_PIM400_IIC2INTC_IRPT_INTR);
	(new PIM400(*i2c, 0x5E))->register_console_commands(console_command_parser, "pim400");

	LED_Controller_Initialize(&atcaLEDs, XPAR_AXI_ATCA_LED_CTRL_DEVICE_ID);

	for (int i = 0; i < 2; i++) {
		adc[i] = new AD7689(XPAR_AD7689_S_0_DEVICE_ID + i);
	}

	xadc = new PS_XADC(XPAR_XADCPS_0_DEVICE_ID);

	for (int i = 0; i < XPAR_MGMT_ZONE_CTRL_0_MZ_CNT; ++i)
		mgmt_zones[i] = new MGMT_Zone(XPAR_MGMT_ZONE_CTRL_0_DEVICE_ID, i);

	std::vector<MGMT_Zone::OutputConfig> pen_config;
	uint64_t hf_mask = 0
			| (1<<0) // PGOOD_2V5ETH
			| (1<<1) // PGOOD_1V0ETH
			| (1<<2) // PGOOD_3V3PYLD
			| (1<<3) // PGOOD_5V0PYLD
			| (1<<4);// PGOOD_1V2PHY
#warning "Hardfault Safety is Disabled (Hacked out)"
	hf_mask = 0; // XXX Disable Hardfault Safety
	mgmt_zones[0]->set_hardfault_mask(hf_mask, 140);
	mgmt_zones[0]->get_pen_config(pen_config);
	for (int i = 0; i < 6; ++i) {
		pen_config[i].active_high = true;
		pen_config[i].drive_enabled = true;
	}
	// +12VPYLD
	pen_config[0].enable_delay = 10;
	// +2V5ETH
	pen_config[1].enable_delay = 200;
	// +1V0ETH
	pen_config[2].enable_delay = 20;
	// +3V3PYLD
	// +1V8PYLD
	// +3V3FFTX_TX
	// +3V3FFTX_RX
	// +3V3FFRX_TX
	// +3V3FFRX_RX
	pen_config[3].enable_delay = 30;
	// +5V0PYLD
	pen_config[4].enable_delay = 30;
	// +1V2PHY
	pen_config[5].enable_delay = 40;
	mgmt_zones[0]->set_pen_config(pen_config);
	mgmt_zones[0]->set_power_state(MGMT_Zone::ON); // Immediately power up.  Xil ethernet driver asserts otherwise.

	hf_mask = 0
			| (1<<5);// ELM_PFAIL
#warning "Hardfault Safety is Disabled (Hacked out)"
	hf_mask = 0; // XXX Disable Hardfault Safety
	mgmt_zones[1]->set_hardfault_mask(hf_mask, 150);
	mgmt_zones[1]->get_pen_config(pen_config);
	// ELM_PWR_EN_I
	pen_config[6].active_high = true;
	pen_config[6].drive_enabled = true;
	pen_config[6].enable_delay = 50;
	mgmt_zones[1]->set_pen_config(pen_config);
}

// TODO: Will be moved
size_t flash_read(uint8_t *buf, size_t size) {
	size_t totalsize = isfqspi->GetTotalSize();
	size_t pagesize = isfqspi->GetPageSize();
	int pagecount = totalsize / pagesize;

	for (int i = 0; i < pagecount; i++) {
		size_t addr = i * pagesize;
		uint8_t* ptr = isfqspi->ReadPage(addr);
		memcpy(buf + addr, ptr, pagesize);
	}

	return totalsize;
}

// TODO: Will be moved
size_t flash_write(uint8_t *buf, size_t size) {
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

	size_t rem = size % isfqspi->GetPageSize();
	size_t pages = size / isfqspi->GetPageSize() + (size? 1 : 0);

	for (size_t i = 0; i < pages; i++) {
		size_t addr = i * isfqspi->GetPageSize();
		if (addr % isfqspi->GetSectorSize() == 0) {
			// This is the first page of a new sector, so erase the sector
			printf("Erasing sector 0x%08x..", addr + baseaddr);
			if (!isfqspi->SectorErase(addr + baseaddr)) {
				// Failed to erase
				printf("Failed to erase 0x%08x. Write to flash failed.", addr + baseaddr);
				firmwareUpdateFailed = true;
				return addr;
			}
		}

		if ((i == (pages-1)) && (rem != 0)) {
			// Last page, avoid a memory leak
			uint8_t tmpbuf[isfqspi->GetPageSize()];
			memset(tmpbuf, 0xFFFFFFFF, isfqspi->GetPageSize());

			memcpy(tmpbuf, buf + addr, rem);

			if (!isfqspi->WritePage(addr + baseaddr, tmpbuf)) {
				// Failed to write page
				printf("Failed to write page 0x%08x. Write to flash failed.", addr + baseaddr);
				firmwareUpdateFailed = true;
				return addr;
			}
		} else {
			if (!isfqspi->WritePage(addr + baseaddr, buf + addr)) {
				// Failed to write page
				printf("Failed to write page 0x%08x. Write to flash failed.", addr + baseaddr);
				firmwareUpdateFailed = true;
				return addr;
			}
		}
	}

	// Verify
	for (int i = 0; i < pages; i++) {
		size_t addr = i * isfqspi->GetPageSize();
		uint8_t* ptr = isfqspi->ReadPage(addr + baseaddr);
		int ret = 0;
		if ((i == (pages-1)) && (rem != 0)) {
			// Last page
			ret = memcmp(ptr, buf + addr, rem);
		} else {
			ret = memcmp(ptr, buf + addr, isfqspi->GetPageSize());
		}
		if (ret != 0) {
			printf("Page 0x%08x is different. Verification failed.", addr + baseaddr);
			firmwareUpdateFailed = true;
			return addr;
		}
	}

	printf("Flash image updated and verified successfully.");
	firmwareUpdateFailed = false;
	return size;
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

	UART* esm_uart = new PL_UART(XPAR_ESM_AXI_UARTLITE_ESM_DEVICE_ID, XPAR_FABRIC_ESM_AXI_UARTLITE_ESM_INTERRUPT_INTR);
	PL_GPIO* esm_gpio = new PL_GPIO(XPAR_ESM_AXI_GPIO_ESM_DEVICE_ID);
	NegResetPin* esm_reset = new NegResetPin(*esm_gpio, 0);

	PL_SPI* esm_spi = new PL_SPI(XPAR_ESM_AXI_QUAD_SPI_ESM_DEVICE_ID, XPAR_FABRIC_ESM_AXI_QUAD_SPI_ESM_IP2INTC_IRPT_INTR);
	SPIFlash* esm_flash = new SPIFlash(*esm_spi, 0);
	NegResetPin* esm_flash_reset = new NegResetPin(*esm_gpio, 1);

	esm = new ESM(esm_uart, esm_reset, esm_flash, esm_flash_reset);
	esm->register_console_commands(console_command_parser, "esm.");

	handle_gpio = new PL_GPIO(XPAR_AXI_GPIO_0_DEVICE_ID, XPAR_FABRIC_AXI_GPIO_0_IP2INTC_IRPT_INTR);
	handle_gpio->setIRQCallback([](uint32_t pin) -> void {
		// This is an IRQ, so act fast!
		bool isPressed = handle_gpio->isPinSet(0);

		LED_Controller_SetOnOff(&atcaLEDs, 0, isPressed);

		xTimerPendFunctionCallFromISR([]( void *ptr, uint32_t x) -> void {
			if (x) printf("Handle is pressed!");
			else printf("Handle is released!");
		}, NULL, isPressed, NULL);
	});

	// Last services should be network related
	network = new Network(LOG["network"], mac_address, [](Network *network) {
		// Network Ready callback, start primary services
		sntp_init();

		// Start secondary services
		influxdbclient = new InfluxDB(LOG["influxdb"]);
		influxdbclient->register_console_commands(console_command_parser, "influxdb.");

		telnet = new TelnetServer(LOG["telnetd"]);

		// Start iperf server
		new Lwiperf(5001);

		// Start XVC server
		new XVCServer(XPAR_AXI_JTAG_0_BASEADDR);

		// Start FTP server
		FTPServer::addFile("virtual/flash.bin", FTPFile(flash_read, flash_write, 16 * 1024 * 1024));
		if (esm->isFlashPresent())
			FTPServer::addFile("virtual/esm.bin", esm->createFlashFile());
		new FTPServer(Auth::ValidateCredentials);

		// Start the sensor gathering thread
		// TODO: Move at some point
		UWTaskCreate("statd", TASK_PRIORITY_BACKGROUND, []() -> void {
			const std::string ipmc_mac = stdsprintf("%02x:%02x:%02x:%02x:%02x:%02x",
					mac_address[0], mac_address[1], mac_address[2], mac_address[3], mac_address[4], mac_address[5]);

			ADC::Channel::Callback tmp36 = [](float r) -> float  { return (r - 0.5) * 100.0; };
			ADC::Channel v12pyld  (*adc[0], 0, 5.640);
			ADC::Channel v5pyld   (*adc[0], 4, 2.400);
			ADC::Channel v3p3pyld (*adc[0], 6, 1.600);
			ADC::Channel v3p3mp   (*adc[0], 7, 1.600);
			ADC::Channel tCDBbot  (*adc[1], 0, tmp36);
			ADC::Channel v1p0eth  (*adc[1], 2);
			ADC::Channel v2p5eth  (*adc[1], 4, 1.216);
			ADC::Channel v1p2phy  (*adc[1], 5);
			ADC::Channel tCDBtop  (*adc[1], 7, tmp36);

			while (1) {
				vTaskDelay(pdMS_TO_TICKS(10000));

				const InfluxDB::Timestamp timestamp = InfluxDB::getCurrentTimestamp();

				const size_t heapBytes = configTOTAL_HEAP_SIZE - xPortGetFreeHeapSize();

				influxdbclient->write("ipmc.system", {{"id", ipmc_mac}, {"name", "heap.free"}}, {{"value", std::to_string(heapBytes)}}, timestamp);
				influxdbclient->write("ipmc.temperature", {{"id", ipmc_mac}, {"name", "xadc"}}, {{"value", std::to_string(xadc->getTemperature())}}, timestamp);
				influxdbclient->write("ipmc.temperature", {{"id", ipmc_mac}, {"name", "adc0"}}, {{"value", std::to_string(adc[0]->getTemperature())}}, timestamp);
				influxdbclient->write("ipmc.temperature", {{"id", ipmc_mac}, {"name", "adc1"}}, {{"value", std::to_string(adc[1]->getTemperature())}}, timestamp);
				influxdbclient->write("ipmc.voltage", {{"id", ipmc_mac}, {"name", "vccaux"}}, {{"value", std::to_string(xadc->getVccAux())}}, timestamp);
				influxdbclient->write("ipmc.voltage", {{"id", ipmc_mac}, {"name", "vbram"}}, {{"value", std::to_string(xadc->getVbram())}}, timestamp);
				influxdbclient->write("ipmc.voltage", {{"id", ipmc_mac}, {"name", "vccpint"}}, {{"value", std::to_string(xadc->getVccPInt())}}, timestamp);
				influxdbclient->write("ipmc.voltage", {{"id", ipmc_mac}, {"name", "vccpaux"}}, {{"value", std::to_string(xadc->getVccPAux())}}, timestamp);
				influxdbclient->write("ipmc.voltage", {{"id", ipmc_mac}, {"name", "vccpdro"}}, {{"value", std::to_string(xadc->getVccPdro())}}, timestamp);

				influxdbclient->write("cdb.temperature", {{"id", ipmc_mac}, {"name", "top"}}, {{"value", std::to_string((float)tCDBtop)}}, timestamp);
				influxdbclient->write("cdb.temperature", {{"id", ipmc_mac}, {"name", "bot"}}, {{"value", std::to_string((float)tCDBbot)}}, timestamp);
				influxdbclient->write("cdb.voltage", {{"id", ipmc_mac}, {"name", "v12pyld"}}, {{"value", std::to_string((float)v12pyld)}}, timestamp);
				influxdbclient->write("cdb.voltage", {{"id", ipmc_mac}, {"name", "v5pyld"}}, {{"value", std::to_string((float)v5pyld)}}, timestamp);
				influxdbclient->write("cdb.voltage", {{"id", ipmc_mac}, {"name", "v3p3pyld"}}, {{"value", std::to_string((float)v3p3pyld)}}, timestamp);
				influxdbclient->write("cdb.voltage", {{"id", ipmc_mac}, {"name", "v3p3mp"}}, {{"value", std::to_string((float)v3p3mp)}}, timestamp);
				influxdbclient->write("cdb.voltage", {{"id", ipmc_mac}, {"name", "v1p0eth"}}, {{"value", std::to_string((float)v1p0eth)}}, timestamp);
				influxdbclient->write("cdb.voltage", {{"id", ipmc_mac}, {"name", "v2p5eth"}}, {{"value", std::to_string((float)v2p5eth)}}, timestamp);
				influxdbclient->write("cdb.voltage", {{"id", ipmc_mac}, {"name", "v1p2phy"}}, {{"value", std::to_string((float)v1p2phy)}}, timestamp);
			}
		});
	});
	network->register_console_commands(console_command_parser, "network.");
}

/**
 * Initialize Device SDRs for this controller.
 */
static void init_device_sdrs(bool reinit) {
	uint8_t reservation = device_sdr_repo.reserve();
#define ADD_TO_REPO(sdr) \
	while (!device_sdr_repo.add(sdr, reservation)) { reservation = device_sdr_repo.reserve(); }

	{
		// Management Controller Device Locator Record for ourself.
		SensorDataRecord12 mcdlr;
		mcdlr.initialize_blank("UW ZYNQ IPMC");
		mcdlr.device_slave_address(ipmb0->ipmb_address);
		mcdlr.channel(0);
		mcdlr.acpi_device_power_state_notification_required(false);
		mcdlr.acpi_system_power_state_notification_required(false);
		mcdlr.is_static(false);
		mcdlr.init_agent_logs_errors(false);
		mcdlr.init_agent_log_errors_accessing_this_controller(false);
		mcdlr.init_agent_init_type(SensorDataRecord12::INIT_ENABLE_EVENTS);
		mcdlr.cap_chassis_device(false);
		mcdlr.cap_bridge(false);
		mcdlr.cap_ipmb_event_generator(true);
		mcdlr.cap_ipmb_event_receiver(true); // Possibly not required.  See also Get PICMG Properties code.
		mcdlr.cap_fru_inventory_device(true);
		mcdlr.cap_sel_device(false);
		mcdlr.cap_sdr_repository_device(true);
		mcdlr.cap_sensor_device(true);
		mcdlr.entity_id(0xA0);
		mcdlr.entity_instance(0x60);
		ADD_TO_REPO(mcdlr);
	}

	{
		// Hotswap Sensor
		SensorDataRecord02 hotswap;
		hotswap.initialize_blank("Hotswap");
		hotswap.entity_id(0xA0);
		hotswap.entity_instance(0x60);
		hotswap.events_enabled_default(true);
		hotswap.scanning_enabled_default(true);
		hotswap.sensor_auto_rearm(true);
		hotswap.sensor_hysteresis_support(SensorDataRecordReadableSensor::ACCESS_READWRITE);
		hotswap.sensor_threshold_access_support(SensorDataRecordReadableSensor::ACCESS_READWRITE);
		hotswap.sensor_event_message_control_support(SensorDataRecordReadableSensor::EVTCTRL_GRANULAR);
		hotswap.sensor_type_code(0xf0); // Hotswap
		hotswap.event_type_reading_code(0x6f); // Sensor-specific discrete
		hotswap.assertion_lower_threshold_reading_mask(0x00ff); // M7:M0
		hotswap.deassertion_upper_threshold_reading_mask(0); // M7:M0
		hotswap.discrete_reading_setable_threshold_reading_mask(0x00ff); // M7:M0
		// I don't need to specify unit type codes for this sensor.
		ADD_TO_REPO(hotswap);
	}

	{
		SensorDataRecord01 sensor;
		sensor.initialize_blank("Payload 3.3V");
		sensor.entity_id(0x0); // TODO
		sensor.entity_instance(0x60); // TODO
		//sensor.sensor_setable(false); // Default, Unsupported
		//sensor.initialize_scanning_enabled(false); // Default (An Init Agent is not required.)
		//sensor.initialize_events_enabled(false); // Default (An Init Agent is not required.)
		//sensor.initialize_thresholds(false); // Default (An Init Agent is not required.)
		//sensor.initialize_hysteresis(false); // Default (An Init Agent is not required.)
		//sensor.initialize_sensor_type(false); // Default (An Init Agent is not required.)
		sensor.ignore_if_entity_absent(true);
		sensor.events_enabled_default(true);
		sensor.scanning_enabled_default(true);
		sensor.sensor_auto_rearm(true);
		sensor.sensor_hysteresis_support(SensorDataRecordReadableSensor::ACCESS_READWRITE);
		sensor.sensor_threshold_access_support(SensorDataRecordReadableSensor::ACCESS_READWRITE);
		sensor.sensor_event_message_control_support(SensorDataRecordReadableSensor::EVTCTRL_GRANULAR);
		sensor.sensor_type_code(0x02); // Voltage
		sensor.event_type_reading_code(SensorDataRecordReadableSensor::EVENT_TYPE_THRESHOLD_SENSOR);
		sensor.assertion_lower_threshold_reading_mask(0x7fff); // All events supported & LNR, LCR, LNC, UNC, UCR, UNR assertions enabled.
		sensor.deassertion_upper_threshold_reading_mask(0x7000); // All events supported & no threshold deassertions enabled.
		sensor.discrete_reading_setable_threshold_reading_mask(0x3fff); // All thresholds are configurable.
		sensor.units_numeric_format(SensorDataRecord01::UNITS_UNSIGNED);
		sensor.units_rate_unit(SensorDataRecordReadableSensor::RATE_UNIT_NONE);
		sensor.units_base_unit(4); // Volts
		sensor.units_modifier_unit(0); // unspecified
		sensor.units_modifier_unit_method(SensorDataRecordReadableSensor::MODIFIER_UNIT_NONE);
		sensor.linearization(SensorDataRecord01::LIN_LINEAR);
		// IPMI Specifies a linearization function of: y = L[(Mx + (B * 10^(Bexp) ) ) * 10^(Rexp) ]
		// Our settings produce a valid range of 2 (Volts) to 4.55 (Volts) with 0.01 Volts granularity.
		sensor.conversion_m(1);
		sensor.conversion_b(2);
		sensor.conversion_b_exp(2);
		sensor.conversion_r_exp(-2);
		sensor.sensor_direction(SensorDataRecordReadableSensor::DIR_INPUT);
		//sensor.normal_min_specified(false); // Default
		//sensor.normal_min_rawvalue(0); // Unspecified
		//sensor.normal_max_specified(false); // Default
		//sensor.normal_max_rawvalue(0); // Unspecified
		sensor.nominal_reading_specified(true);
		sensor.nominal_reading_rawvalue(130); // 3.3 Volts
		sensor.threshold_unr_rawvalue(147); // 3.47 Volts
		sensor.threshold_ucr_rawvalue(142); // 3.42 Volts
		sensor.threshold_unc_rawvalue(137); // 3.37 Volts
		sensor.threshold_lnc_rawvalue(123); // 3.23 Volts
		sensor.threshold_lcr_rawvalue(118); // 3.18 Volts
		sensor.threshold_lnr_rawvalue(113); // 3.13 Volts
		sensor.hysteresis_high(2); // +0.02 Volts
		sensor.hysteresis_low(2); // -0.02 Volts
		ADD_TO_REPO(sensor);
	}

#undef ADD_TO_REPO

	VariablePersistentAllocation sdr_persist(*persistent_storage, PersistentStorageAllocations::WISC_SDR_REPOSITORY);
	// If not reinitializing, merge in saved configuration, overwriting matching records.
	if (!reinit)
		device_sdr_repo.u8import(sdr_persist.get_data());

	// Store the newly initialized Device SDRs
	sdr_persist.set_data(device_sdr_repo.u8export());

	// I think these needta be imported to the main SDR repo too?
	sdr_repo.add(device_sdr_repo, 0);
}

void init_fru_area() {
	std::vector<uint8_t> tlstring;

	std::vector<uint8_t> board_info;
	board_info.push_back(0x01); // Format Version
	board_info.push_back(0x00); // Length Placeholder
	board_info.push_back(25);   // Language Code (English)
	board_info.push_back(0x00); // Mfg Date/Time (Unspecified
	board_info.push_back(0x00); // Mfg Date/Time (Unspecified
	board_info.push_back(0x00); // Mfg Date/Time (Unspecified
	tlstring = encode_ipmi_type_length_field("University of Wisconsin");
	board_info.insert(board_info.end(), tlstring.begin(), tlstring.end()); // Board Mfgr.
	tlstring = encode_ipmi_type_length_field("ZYNQ IPMC");
	board_info.insert(board_info.end(), tlstring.begin(), tlstring.end()); // Board Product Name
	tlstring = encode_ipmi_type_length_field(std::to_string(IPMC_SERIAL));
	board_info.insert(board_info.end(), tlstring.begin(), tlstring.end()); // Board Serial
	tlstring = encode_ipmi_type_length_field(std::string("ELM1 Rev") + std::to_string(IPMC_HW_REVISION));
	board_info.insert(board_info.end(), tlstring.begin(), tlstring.end()); // Board Part Number
	tlstring = encode_ipmi_type_length_field(GIT_DESCRIBE);
	board_info.insert(board_info.end(), tlstring.begin(), tlstring.end()); // FRU File ID (in our case generating software)
	board_info.push_back(0xC1); // End of T/L Records.
	board_info.push_back(0); // Ensure at least one pad, to be used for checksum.
	while (board_info.size() % 8)
		board_info.push_back(0); // Pad.
	board_info[1] = board_info.size()/8; // Update length
	board_info.pop_back(); // Remove one pad for checksum.
	board_info.push_back(ipmi_checksum(board_info));


	std::vector<uint8_t> product_info;
	product_info.push_back(0x01); // Format Version
	product_info.push_back(0x00); // Length Placeholder
	product_info.push_back(25);   // Language Code (English)
	tlstring = encode_ipmi_type_length_field("University of Wisconsin");
	product_info.insert(product_info.end(), tlstring.begin(), tlstring.end()); // Mfgr Name
	tlstring = encode_ipmi_type_length_field("ZYNQ IPMC");
	product_info.insert(product_info.end(), tlstring.begin(), tlstring.end()); // Product Name
	tlstring = encode_ipmi_type_length_field(std::string("ELM1 Rev") + std::to_string(IPMC_HW_REVISION));
	product_info.insert(product_info.end(), tlstring.begin(), tlstring.end()); // Product Part/Model Number
	tlstring = encode_ipmi_type_length_field(std::to_string(IPMC_HW_REVISION));
	product_info.insert(product_info.end(), tlstring.begin(), tlstring.end()); // Product Version
		tlstring = encode_ipmi_type_length_field(std::to_string(IPMC_SERIAL));
	product_info.insert(product_info.end(), tlstring.begin(), tlstring.end()); // Product Serial
	product_info.push_back(0xC0); // Asset Tag (NULL)
	tlstring = encode_ipmi_type_length_field(GIT_DESCRIBE);
	product_info.insert(product_info.end(), tlstring.begin(), tlstring.end()); // FRU File ID (in our case generating software)
	product_info.push_back(0xC1); // End of T/L Records.
	product_info.push_back(0); // Ensure at least one pad, to be used for checksum.
	while (product_info.size() % 8)
		product_info.push_back(0); // Pad.
	product_info[1] = product_info.size()/8; // Update length
	product_info.pop_back(); // Remove one pad for checksum.
	product_info.push_back(ipmi_checksum(product_info));


	std::vector<uint8_t> frudata;
	frudata.resize(8);
	frudata[0] = 0x01; // Common Header Format Version
	frudata[1] = 0x00; // Internal Use Area Offset (multiple of 8 bytes)
	frudata[2] = 0x00; // Chassis Info Area Offset (multiple of 8 bytes)
	frudata[3] = 0x01; // Board Area Offset (multiple of 8 bytes)
	frudata[4] = 0x01 + board_info.size()/8; // Product Info Area Offset (multiple of 8 bytes)
	frudata[5] = 0x01 + board_info.size()/8 + product_info.size()/8; // Multi-Record Area Offset (multiple of 8 bytes)
	frudata[6] = 0x00; // PAD, write as 00h
	frudata[7] = 0x00;
	frudata[7] = ipmi_checksum(frudata); // Checksum

	frudata.insert(frudata.end(), board_info.begin(), board_info.end());
	frudata.insert(frudata.end(), product_info.begin(), product_info.end());

	std::vector<uint8_t> multirecord;
	multirecord.clear();
	multirecord.push_back(0xC0); // "OEM", specified
	multirecord.push_back(0x02); // Not end of list, format 02 (specified)
	multirecord.push_back(0); // length (placeholder)
	multirecord.push_back(0); // record checksum (placeholder)
	multirecord.push_back(0); // header checksum (placeholder)
	multirecord.push_back(0x5A); // PICMG, specified
	multirecord.push_back(0x31); // PICMG, specified
	multirecord.push_back(0x00); // PICMG, specified
	multirecord.push_back(0x16); // PICMG Record ID, specified
	multirecord.push_back(0); // Version, specified
	multirecord.push_back(30); // ceil(35/1.2) // Current Draw, 35W
	multirecord[2] = multirecord.size();
	multirecord[3] = ipmi_checksum(std::vector<uint8_t>(std::next(multirecord.begin(), 5), multirecord.end()));
	multirecord[4] = ipmi_checksum(std::vector<uint8_t>(multirecord.begin(), std::next(multirecord.begin(), 5)));
	frudata.insert(frudata.end(), multirecord.begin(), multirecord.end());
}

void* operator new(std::size_t n) {
	return pvPortMalloc(n);
}
void operator delete(void* p) throw() {
	vPortFree(p);
}

std::string generate_banner() {
	std::string bannerstr;
	bannerstr += "********************************************************************************\n";
	bannerstr += "\n";
	bannerstr += std::string("ZYNQ-IPMC - Open-source IPMC hardware and software framework\n");
	bannerstr += std::string("HW revision : ") + std::to_string(IPMC_HW_REVISION) + "\n"; // TODO
	bannerstr += std::string("SW revision : ") + GIT_DESCRIBE + "\n";
	if (IPMC_SERIAL != 0xffff & IPMC_SERIAL != 0)
		bannerstr += std::string("HW serial   : ") + std::to_string(IPMC_SERIAL) + "\n";
	else
		bannerstr += std::string("HW serial   : unset\n");
	bannerstr += std::string("Build date  : ") + COMPILE_DATE + "\n";
	bannerstr += std::string("Build host  : ") + COMPILE_HOST + "\n";
	bannerstr += std::string("Build conf  : ") + BUILD_CONFIGURATION + "\n";
	bannerstr += std::string("OS version  : FreeRTOS ") + tskKERNEL_VERSION_NUMBER + "\n";

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

namespace {
/// A "uptime" console command.
class ConsoleCommand_uptime : public CommandParser::Command {
public:
	virtual std::string get_helptext(const std::string &command) const {
		return stdsprintf(
				"%s\n"
				"\n"
				"Print the current system uptime.\n", command.c_str());
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		uint64_t now64 = get_tick64();
		//u16 ms = now64 % 1000;
		u16 s  = (now64 / 1000) % 60;
		u16 m  = (now64 / (60*1000)) % 60;
		u16 h  = (now64 / (60*60*1000)) % 24;
		u32 d  = (now64 / (24*60*60*1000));
		std::string out = "Up for ";
		if (d)
			out += stdsprintf("%lud", d);
		if (d||h)
			out += stdsprintf("%huh", h);
		if (d||h||m)
			out += stdsprintf("%hum", m);
		out += stdsprintf("%hus", s);
		console->write(out + "\n");
	}

	//virtual std::vector<std::string> complete(const CommandParser::CommandParameters &parameters) const { };
};

/// A "date" console command.
class ConsoleCommand_date : public CommandParser::Command {
public:
	virtual std::string get_helptext(const std::string &command) const {
		return stdsprintf(
				"%s\n"
				"\n"
				"Print the current system time. Updated by SNTP and kept by FreeRTOS.\n", command.c_str());
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		timeval tv;
		gettimeofday(&tv, NULL);
		if (tv.tv_sec == 0) {
			console->write("Time information unavailable.\n");
		} else {
			struct tm *timeinfo = localtime(&tv.tv_sec);
			console->write(std::string(asctime(timeinfo))); // GMT
		}
	}
};

/// A "version" console command.
class ConsoleCommand_version : public CommandParser::Command {
public:
	virtual std::string get_helptext(const std::string &command) const {
		return stdsprintf(
				"%s\n"
				"\n"
				"Print the current system version information.\n", command.c_str());
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		console->write(generate_banner());
	}

	//virtual std::vector<std::string> complete(const CommandParser::CommandParameters &parameters) const { };
};

/// A "ps" console command.
class ConsoleCommand_ps : public CommandParser::Command {
public:
	virtual std::string get_helptext(const std::string &command) const {
		return stdsprintf(
				"%s\n"
				"\n"
				"Print the system process listing & statistics.\n", command.c_str());
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		console->write(this->get_ps_string());
	}

	static std::string get_ps_string() {
		std::string out;
		UBaseType_t task_count = uxTaskGetNumberOfTasks();
		TaskStatus_t taskinfo[task_count+2];
		UBaseType_t total_runtime;
		if (!uxTaskGetSystemState(taskinfo, task_count+2, &total_runtime)) {
			return "Failed to generate process listing.\n";
		}

		// Runtime stats are accurate only if they havent rolled over.  It seems to be a tad under 666 per tick.
		bool runstats = get_tick64() < portMAX_DELAY/666;

		std::vector<TaskStatus_t> tasks;
		tasks.reserve(task_count);
		for (UBaseType_t i = 0; i < task_count; ++i)
			tasks.push_back(taskinfo[i]);
		if (runstats)
			std::sort(tasks.begin(), tasks.end(), [](const TaskStatus_t &a, const TaskStatus_t &b) -> bool { return a.ulRunTimeCounter > b.ulRunTimeCounter; });
		else
			std::sort(tasks.begin(), tasks.end(), [](const TaskStatus_t &a, const TaskStatus_t &b) -> bool { return (a.uxCurrentPriority > b.uxCurrentPriority) || (a.uxCurrentPriority == b.uxCurrentPriority && a.xTaskNumber < b.xTaskNumber); });

		out += "PID Name             BasePrio CurPrio StackHW State";
		if (runstats)
			out += " CPU% CPU";
		out += "\n";
		for (auto it = tasks.begin(), eit = tasks.end(); it != eit; ++it) {
			if (it->eCurrentState < 0 || it->eCurrentState > eInvalid)
				it->eCurrentState = eInvalid;
			static const char *taskstates[] = {
				"*Running*",
				"Ready",
				"Blocked",
				"Suspended",
				"Deleted",
				"Invalid"
			};
			out += stdsprintf("%3lu %-16s %8lu %7lu %7hu %5c",
					it->xTaskNumber,
					it->pcTaskName,
					it->uxBasePriority,
					it->uxCurrentPriority,
					it->usStackHighWaterMark,
					taskstates[it->eCurrentState][0]);
			if (runstats) {
				UBaseType_t cpu_percent = it->ulRunTimeCounter / (total_runtime/100);
				if (it->ulRunTimeCounter && cpu_percent < 1)
					out += stdsprintf("  <1%% %lu", it->ulRunTimeCounter);
				else
					out += stdsprintf("  %2lu%% %lu", cpu_percent, it->ulRunTimeCounter);
			}
			out += "\n";
		}
		if (!runstats)
			out += "\nNote: Runtime stats were not displayed, as we are likely past the point\nof counter wrapping and they are no longer accurate.\n";
		return out;
	}

	//virtual std::vector<std::string> complete(const CommandParser::CommandParameters &parameters) const { };
};


/// A temporary "backend_power" command
class ConsoleCommand_backend_power : public CommandParser::Command {
public:
	virtual std::string get_helptext(const std::string &command) const {
		return stdsprintf(
				"%s [(on|off)]\n"
				"\n"
				"Enable/Disable MZs\n"
				"Without parameters, returns power status.\n", command.c_str());
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		if (parameters.nargs() == 1) {
			std::string out;
			bool transitioning;
			bool enabled = mgmt_zones[1]->get_power_state(&transitioning);
			if (transitioning)
				out += stdsprintf("ELM power status is (transitioning to) %s\n", (enabled?"on":"off"));
			else
				out += stdsprintf("ELM power status is %s\n", (enabled?"on":"off"));
			out += "\n";
			uint32_t pen_state = mgmt_zones[1]->get_pen_status(false);
			out += stdsprintf("The power enables are currently at 0x%08x\n", pen_state);
			console->write(out);
			return;
		}

		std::string action;
		if (!parameters.parse_parameters(1, true, &action)) {
			console->write("Invalid parameters.\n");
			return;
		}
		if (action == "on") {
			mgmt_zones[1]->set_power_state(MGMT_Zone::ON);
		}
		else if (action == "off") {
			mgmt_zones[1]->set_power_state(MGMT_Zone::OFF);
		}
		else {
			console->write("Unknown action.\n");
			return;
		}
	}

	//virtual std::vector<std::string> complete(const CommandParser::CommandParameters &parameters) const { };
};

/// A temporary "restart" command
class ConsoleCommand_restart : public CommandParser::Command {
public:
	virtual std::string get_helptext(const std::string &command) const {
		return stdsprintf(
				"%s\n"
				"\n"
				"Do a complete restart to the IPMC, loading firmware and software from flash.\n", command.c_str());
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		// See section 26.2.3 in UG585 - System Software Reset

		if (firmwareUpdateFailed == false) {
			console->write("Flushing persistent storage...\n");
			SemaphoreHandle_t psdone = xSemaphoreCreateBinary();
			persistent_storage->flush([&psdone]() -> void { xSemaphoreGive(psdone); });
			xSemaphoreTake(psdone, portMAX_DELAY);
			vSemaphoreDelete(psdone);

			console->write("Restarting...\n");
			vTaskDelay(pdMS_TO_TICKS(100));

			volatile uint32_t* slcr_unlock_reg = (uint32_t*)(0xF8000000 + 0x008); // SLCR is at 0xF8000000
			volatile uint32_t* pss_rst_ctrl_reg = (uint32_t*)(0xF8000000 + 0x200);

			*slcr_unlock_reg = 0xDF0D; // Unlock key
			*pss_rst_ctrl_reg = 1;
		} else {
			console->write("Restart is disabled due to a failed firmware attempt (flash may be corrupted).\n");
		}
	}

	//virtual std::vector<std::string> complete(const CommandParser::CommandParameters &parameters) const { };
};

class ConsoleCommand_flash_info : public CommandParser::Command {
public:
	virtual std::string get_helptext(const std::string &command) const {
		return stdsprintf(
				"%s\n"
				"\n"
				"info about the flash.\n", command.c_str());
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		std::string info = "Flash is a " +
				isfqspi->GetManufacturerName() +
				" IC with a total of " +
				std::to_string(isfqspi->GetTotalSize() / 1024 / 1024) +
				"MBytes.\n";

		info += "Sector size: " + std::to_string(isfqspi->GetSectorSize()) + "\n";
		info += "Page size: " + std::to_string(isfqspi->GetPageSize()) + "\n";

		console->write(info);
	}

	//virtual std::vector<std::string> complete(const CommandParser::CommandParameters &parameters) const { };
};

/// A "setauth" console command.
class ConsoleCommand_setauth: public CommandParser::Command {
public:
	virtual std::string get_helptext(const std::string &command) const {
		return stdsprintf(
				"%s $username $password\n"
				"\n"
				"Change network access username and password.\n", command.c_str());
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		std::string user, pass;
		if (parameters.nargs() != 2) {
			console->write("Invalid parameters, see help.\n");
			return;
		}

		parameters.parse_parameters(1, true, &user);
		parameters.parse_parameters(2, true, &pass);

		Auth::ChangeCredentials(user, pass);

		console->write("Password updated.\n");
	}

	//virtual std::vector<std::string> complete(const CommandParser::CommandParameters &parameters) const { };
};

/// A "set_serial" console command.
class ConsoleCommand_set_serial: public CommandParser::Command {
public:
	virtual std::string get_helptext(const std::string &command) const {
		return stdsprintf(
				"%s $serial\n"
				"\n"
				"Set the IPMC serial number.\n"
				"NOTE: This cannot be changed once set!\n", command.c_str());
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		uint16_t serial;
		if (!parameters.parse_parameters(1, true, &serial)) {
			console->write("Please provide a serial number.\n");
			return;
		}

		IPMC_SERIAL = serial;
		eeprom_mac->write(0, reinterpret_cast<uint8_t*>(&serial), sizeof(serial));
		console->write("Serial updated.  Reboot to lock.\n");
	}

	//virtual std::vector<std::string> complete(const CommandParser::CommandParameters &parameters) const { };
};

class ConsoleCommand_testin : public CommandParser::Command {
public:
	virtual std::string get_helptext(const std::string &command) const {
		return "test command, not intended for commit";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {

		uint32_t byteCount;

		if (!parameters.parse_parameters(1, false, &byteCount)) {
			console->write("Wrong arguments");
			return;
		}

		u8 *buf = new u8[byteCount];

		// Discard any incoming window size data, etc.
		uart_ps0->read(buf, byteCount, configTICK_RATE_HZ/10);
		size_t bytesread = uart_ps0->read(buf, byteCount, portMAX_DELAY, configTICK_RATE_HZ*(5 + (byteCount / 10000)));
		console->write(stdsprintf("read %d bytes\n", bytesread));

		u8 s256[SHA_VALBYTES] = {0};
		sha_256(buf, bytesread, s256);
		std::string shahex;
		shahex.reserve(SHA_VALBYTES*2);
		for (int i = 0; i < SHA_VALBYTES; i++)
			   shahex.append(stdsprintf("%02hhx", s256[i]));
		shahex.pop_back();
		console->write(shahex+"\n");

		console->write(formatedHexString(buf, 256));

		/*FTPFile *file = FTPServer::getFileFromPath("virtual/esm.bin");
		if (file) {
			if (file->write(buf, bytesread) != bytesread) {
				console->write("Failed to write");
			} else {
				console->write("Written");
			}
		}*/

		delete buf;
	}
};

} // anonymous namespace

static void register_core_console_commands(CommandParser &parser) {
	console_command_parser.register_command("uptime", std::make_shared<ConsoleCommand_uptime>());
	console_command_parser.register_command("date", std::make_shared<ConsoleCommand_date>());
	console_command_parser.register_command("version", std::make_shared<ConsoleCommand_version>());
	console_command_parser.register_command("ps", std::make_shared<ConsoleCommand_ps>());
	console_command_parser.register_command("backend_power", std::make_shared<ConsoleCommand_backend_power>());
	console_command_parser.register_command("restart", std::make_shared<ConsoleCommand_restart>());
	console_command_parser.register_command("flash_info", std::make_shared<ConsoleCommand_flash_info>());
	console_command_parser.register_command("setauth", std::make_shared<ConsoleCommand_setauth>());
	if (IPMC_SERIAL == 0 || IPMC_SERIAL == 0xFFFF)
		console_command_parser.register_command("set_serial", std::make_shared<ConsoleCommand_set_serial>());
	console_command_parser.register_command("testin", std::make_shared<ConsoleCommand_testin>());
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

/**
 * Modify a std::string in place to contain \r\n where it currently contains \n.
 * @param input The string to modify.
 * @param nlchar The newline character to detect, if not '\n'.
 */
void windows_newline(std::string &input, char nlchar) {
	for (unsigned int i = 0; i < input.size(); ++i) {
		if (input[i] == nlchar) {
			input.replace(i, 1, "\r\n");
			i++; // We don't want to hit that \n again on the (now) next character.
		}
	}
}

std::string stdsprintf(const char *fmt, ...) {
    va_list va;
    va_list va2;
    va_start(va, fmt);
    va_copy(va2, va);
    size_t s = vsnprintf(NULL, 0, fmt, va) + 1;
    char *str;
    if (s <= 128)
    	str = (char*)alloca(s);
    else
    	str = (char*)malloc(s);
    vsnprintf(str, s, fmt, va2);
    va_end(va);
    va_end(va2);
    std::string ret(str);
    if (s > 128)
    	free(str);
    return ret;
}
