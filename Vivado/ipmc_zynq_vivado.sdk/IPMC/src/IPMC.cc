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
#include <drivers/pim400/PIM400.h>
#include <drivers/esm/ESM.h>
#include <drivers/elm/ELM.h>
#include <drivers/ad7689/AD7689.h>
#include <drivers/pl_led/PLLED.h>
#include <drivers/ltc2654f/LTC2654F.h>

/* Include services */
#include <services/ipmi/ipmbsvc/IPMBSvc.h>
#include <services/ipmi/ipmbsvc/IPMICommandParser.h>
#include <services/ipmi/commands/IPMICmd_Index.h>
#include <services/ipmi/sdr/SensorDataRepository.h>
#include <services/ipmi/sdr/SensorDataRecord01.h>
#include <services/ipmi/sdr/SensorDataRecord02.h>
#include <services/ipmi/sdr/SensorDataRecord12.h>
#include <services/ipmi/sensor/SensorSet.h>
#include <services/ipmi/sensor/Sensor.h>
#include <services/ipmi/sensor/HotswapSensor.h>
#include <services/ipmi/sensor/ThresholdSensor.h>
#include <services/ipmi/MStateMachine.h>
#include <services/persistentstorage/PersistentStorage.h>
#include <services/console/UARTConsoleSvc.h>
#include <services/influxdb/InfluxDB.h>
#include <services/telnet/Telnet.h>
#include <services/lwiperf/Lwiperf.h>
#include <services/xvcserver/XVCServer.h>
#include <services/sntp/sntp.h>
#include <services/ftp/FTPServer.h>
#include <PayloadManager.h>


u8 IPMC_HW_REVISION = 1; // TODO: Detect, Update, etc
uint16_t IPMC_SERIAL = 0xffff;

/**
 * A FreeRTOS EventGroup initialized by main() before the scheduler starts.
 *
 * [1] 1b = ipmc_service_init() has exited.
 * [0] 1b = driver_init() has exited.
 */
EventGroupHandle_t init_complete;

PS_WDT *SWDT;
PS_UART *uart_ps0;
PS_ISFQSPI *isfqspi;
XGpioPs gpiops;

LogTree LOG("ipmc");
LogTree::Filter *console_log_filter;

SPI_EEPROM *eeprom_mac;
SPI_EEPROM *eeprom_data;
PersistentStorage *persistent_storage;

CommandParser console_command_parser;
std::shared_ptr<UARTConsoleSvc> console_service;

IPMBSvc *ipmb0;
EventReceiver ipmi_event_receiver;
IPMICommandParser *ipmi_command_parser;
SensorDataRepository sdr_repo;
SensorDataRepository device_sdr_repo;
SensorSet ipmc_sensors(&device_sdr_repo);
SemaphoreHandle_t fru_data_mutex;
std::vector<uint8_t> fru_data;
MStateMachine *mstatemachine = NULL;
PL_GPIO *handle_gpio;
PayloadManager *payload_manager = NULL;

u8 mac_address[6];
Network *network;
InfluxDB *influxdbclient;
TelnetServer *telnet;

ESM *esm;
ELM *elm;

AD7689 *adc[5];

LTC2654F *dac;

PL_GPIO *pl_gpio;
PL_GPIO *xvctarget_gpio;

PS_XADC *xadc;

PL_LED *atcaLEDs;
std::vector<IPMI_LED*> ipmi_leds;  // Blue, Red, Green, Amber

static void init_device_sdrs(bool reinit=false);
static void init_fru_data(bool reinit=false);
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

	/* SDRs must be initialized here so sensors are available to link up with
	 * their drivers.  FRU Data will be done later, once the PayloadManager is
	 * initialized.  The IPMBSvc thread does not proceed until service init is
	 * done.  SDRs will not be reloaded from EEPROM and will remain in their
	 * default state until the sdr_init thread has time to run.
	 */
	init_device_sdrs(false);

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
	ipmi_event_receiver.ipmb = ipmb0;
	ipmi_event_receiver.lun = 0;
	ipmi_event_receiver.addr = 0x20; // Should be `0xFF "Disabled"`, maybe?

	// TODO: Clean up this part
	if (use_pl) {
		PL_I2C *i2c = new PL_I2C(XPAR_AXI_IIC_PIM400_DEVICE_ID, XPAR_FABRIC_AXI_IIC_PIM400_IIC2INTC_IRPT_INTR);
		(new PIM400(*i2c, 0x56))->register_console_commands(console_command_parser, "pim400");

		atcaLEDs = new PL_LED(XPAR_AXI_ATCA_LED_CTRL_DEVICE_ID, 50000000);
		ipmi_leds.push_back(new IPMI_LED(*new LED(*atcaLEDs, 0))); // Blue LED
		ipmi_leds.push_back(new IPMI_LED(*new LED(*atcaLEDs, 1))); // Red LED
		ipmi_leds.push_back(new IPMI_LED(*new LED(*atcaLEDs, 2))); // Green LED
		ipmi_leds.push_back(new IPMI_LED(*new LED(*atcaLEDs, 3))); // Amber LED

		for (int i = 0; i < 2; i++) {
			adc[i] = new AD7689(XPAR_AD7689_S_0_DEVICE_ID + i, 0);
		}

		for (int i = 0; i < 3; i++) {
			adc[i+2] = new AD7689(XPAR_AD7689_S_2_DEVICE_ID, i);
		}

		xadc = new PS_XADC(XPAR_XADCPS_0_DEVICE_ID);

#define DACRSTn_PIN			0
#define LDACn_PIN			1
#define ELMRSTn_PIN			2
#define PWRENA_ACTVn		3

		pl_gpio = new PL_GPIO(XPAR_AXI_GPIO_0_DEVICE_ID);
		pl_gpio->setChannel((1 << ELMRSTn_PIN) | (1 << DACRSTn_PIN) | (1 << LDACn_PIN));
		pl_gpio->setDirection(0);

		xvctarget_gpio = new PL_GPIO(XPAR_AXI_GPIO_XVCTARGET_DEVICE_ID);
		configASSERT(xvctarget_gpio);

		SPIMaster *dac_spi = new PL_SPI(XPAR_AXI_QUAD_SPI_DAC_DEVICE_ID, XPAR_FABRIC_AXI_QUAD_SPI_DAC_IP2INTC_IRPT_INTR);
		dac = new LTC2654F(*dac_spi, 0, true);

		// Set DACs
		pl_gpio->setPin(DACRSTn_PIN);
		vTaskDelay(pdMS_TO_TICKS(100));
		dac->sendCommand(LTC2654F::ALL_DACS, LTC2654F::WRITE_AND_UPDATE_REG, 0x7ff);

		handle_gpio = new PL_GPIO(XPAR_AXI_GPIO_HNDL_SW_DEVICE_ID, XPAR_FABRIC_AXI_GPIO_HNDL_SW_IP2INTC_IRPT_INTR);
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

	// ESM
	UART* esm_uart = new PL_UART(XPAR_ESM_AXI_UARTLITE_ESM_DEVICE_ID, XPAR_FABRIC_ESM_AXI_UARTLITE_ESM_INTERRUPT_INTR);
	PL_GPIO* esm_gpio = new PL_GPIO(XPAR_ESM_AXI_GPIO_ESM_DEVICE_ID);
	NegResetPin* esm_reset = new NegResetPin(*esm_gpio, 0);

	PL_SPI* esm_spi = new PL_SPI(XPAR_ESM_AXI_QUAD_SPI_ESM_DEVICE_ID, XPAR_FABRIC_ESM_AXI_QUAD_SPI_ESM_IP2INTC_IRPT_INTR);
	SPIFlash* esm_flash = new SPIFlash(*esm_spi, 0);
	NegResetPin* esm_flash_reset = new NegResetPin(*esm_gpio, 1);

	esm = new ESM(esm_uart, esm_reset, esm_flash, esm_flash_reset);
	esm->register_console_commands(console_command_parser, "esm.");

	// ELM
	UART* elm_uart = new PL_UART(XPAR_ELM_AXI_UARTLITE_0_DEVICE_ID, XPAR_FABRIC_ELM_AXI_UARTLITE_0_INTERRUPT_INTR);
	PL_GPIO* elm_gpio = new PL_GPIO(XPAR_ELM_AXI_GPIO_0_DEVICE_ID, XPAR_FABRIC_ELM_AXI_GPIO_0_IP2INTC_IRPT_INTR);
	elm = new ELM(elm_uart, elm_gpio);
	elm->register_console_commands(console_command_parser, "elm.");

	{
		mstatemachine = new MStateMachine(std::dynamic_pointer_cast<HotswapSensor>(ipmc_sensors.find_by_name("Hotswap")), *ipmi_leds[0], LOG["mstatemachine"]);
		mstatemachine->register_console_commands(console_command_parser, "");

		// Since we can't do this processing in the ISR itself, we'll have to settle for this.
		SemaphoreHandle_t handle_isr_sem = xSemaphoreCreateBinary();
		UWTaskCreate("handle_switch", TASK_PRIORITY_SERVICE, [handle_isr_sem]() -> void {
			// Wait for IPMC initialization to complete.
			// The first time we update the physical handle state, the MStateMachine startup lock is cleared.
			xEventGroupWaitBits(init_complete, 0x03, pdFALSE, pdTRUE, portMAX_DELAY);

			while (1) {
				/* This mechanism functions by interrupt signaling, but has a
				 * backup polling mechanism.  When an interrupt is triggered,
				 * this semaphore will ready immediately.  If no interrupt is
				 * processed, we will update anyway, every 100ms.
				 */
				xSemaphoreTake(handle_isr_sem, pdMS_TO_TICKS(100));

				bool isPressed = !(handle_gpio->isPinSet(0));
				mstatemachine->physical_handle_state(isPressed ? MStateMachine::HANDLE_CLOSED : MStateMachine::HANDLE_OPEN);
			}
		});
		handle_gpio->setIRQCallback([handle_isr_sem](uint32_t pin) -> void { xSemaphoreGiveFromISR(handle_isr_sem, NULL); });
	}
	payload_manager = new PayloadManager(mstatemachine, LOG["payload_manager"]);
	payload_manager->register_console_commands(console_command_parser, "payload.");
	// IPMC Sensors have been instantiated already, so we can do this linkage now.
	payload_manager->refresh_sensor_linkage();

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
	init_fru_data(true);

	// Last services should be network related
	network = new Network(LOG["network"], mac_address, [](Network *network) {
		// Network Ready callback, start primary services
		sntp_init();

		// Start secondary services
		//influxdbclient = new InfluxDB(LOG["influxdb"]);
		//influxdbclient->register_console_commands(console_command_parser, "influxdb.");

		telnet = new TelnetServer(LOG["telnetd"]);

		// Start iperf server
		new Lwiperf(5001);

		// Start XVC server
		new XVCServer(XPAR_AXI_JTAG_0_BASEADDR);

		// Start FTP server
		VFS::addFile("virtual/flash.bin", PS_ISFQSPI::createFlashFile(isfqspi, 16 * 1024 * 1024));
		VFS::addFile("virtual/esm.bin", esm->createFlashFile());
		new FTPServer(Auth::ValidateCredentials);

		// Start the sensor gathering thread
		// TODO: Move at some point
		/*UWTaskCreate("statd", TASK_PRIORITY_BACKGROUND, []() -> void {
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

				float esmTemp = 0.0;
				if (esm->getTemperature(esmTemp)) {
					influxdbclient->write("esm.temperature", {{"id", ipmc_mac}, {"name", "temp"}}, {{"value", std::to_string(esmTemp)}}, timestamp);
				}
			}
		});*/
	});
	network->register_console_commands(console_command_parser, "network.");

	// This has to be lower, so the serial number has been read by the time we register (or not register) set_serial.
	register_core_console_commands(console_command_parser);
}

static void add_to_sdr_repo(SensorDataRepository &repo, const SensorDataRecord &sdr, SensorDataRepository::reservation_t &reservation) {
	while (true) {
		try {
			repo.add(sdr, reservation);
			return;
		}
		catch (SensorDataRepository::reservation_cancelled_error) {
			reservation = repo.reserve();
		}
	}
}

/**
 * Initialize Device SDRs for this controller.
 */
static void init_device_sdrs(bool reinit) {
	SensorDataRepository::reservation_t reservation = device_sdr_repo.reserve();

#define ADD_TO_REPO(sdr) add_to_sdr_repo(device_sdr_repo, sdr, reservation)

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
		SensorDataRecord02 hotswap;
		hotswap.initialize_blank("Hotswap");
		hotswap.sensor_owner_id(0); // Tag as "self". This will be auto-calculated in "Get SDR" commands.
		hotswap.sensor_owner_channel(0); // See above.
		hotswap.sensor_owner_lun(0); // See above.
		hotswap.sensor_number(1);
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
		if (!ipmc_sensors.get(hotswap.sensor_number()))
			ipmc_sensors.add(std::make_shared<HotswapSensor>(hotswap.record_key(), LOG["sensors"]["Hotswap"]));
	}

	{
		SensorDataRecord01 sensor;
		// SDR Calculator.html#precision=4&s-na=%2B0.85VDD&s-no=2&s-t=0x02&s-u-p=4&lnrf=0.765&lcrf=0.7863&lncf=0.8075&uncf=0.8925&ucrf=0.9137&unrf=0.935&nominalf=0.85&minf=0&granularity=0.0039
		sensor.initialize_blank("+0.85VDD");
		sensor.sensor_owner_id(0); // Tag as "self". This will be auto-calculated in "Get SDR" commands.
		sensor.sensor_owner_channel(0); // See above.
		sensor.sensor_owner_lun(0); // Generally zero
		sensor.sensor_number(2);
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
		sensor.deassertion_upper_threshold_reading_mask(0x7fff); // All events supported & LNR, LCR, LNC, UNC, UCR, UNR deassertions enabled.
		sensor.discrete_reading_setable_threshold_reading_mask(0x3fff); // All thresholds are configurable.
		sensor.units_numeric_format(SensorDataRecord01::UNITS_UNSIGNED);
		sensor.units_rate_unit(SensorDataRecordReadableSensor::RATE_UNIT_NONE);
		sensor.units_base_unit(4); // Volts
		sensor.units_modifier_unit(0); // unspecified
		sensor.units_modifier_unit_method(SensorDataRecordReadableSensor::MODIFIER_UNIT_NONE);
		sensor.linearization(SensorDataRecord01::LIN_LINEAR);
		// IPMI Specifies a linearization function of: y = L[(Mx + (B * 10^(Bexp) ) ) * 10^(Rexp) ]
		// Our settings produce a valid range of 0 (Volts) to 0.9945 (Volts) with 0.0039 Volts granularity.
		sensor.conversion_m(39);
		sensor.conversion_b(0);
		sensor.conversion_b_exp(0);
		sensor.conversion_r_exp(-4);
		sensor.sensor_direction(SensorDataRecordReadableSensor::DIR_UNSPECIFIED);
		//sensor.normal_min_specified(false); // Default
		//sensor.normal_min_rawvalue(0); // Unspecified
		//sensor.normal_max_specified(false); // Default
		//sensor.normal_max_rawvalue(0); // Unspecified
		sensor.nominal_reading_specified(true);
		sensor.nominal_reading_rawvalue(217); // 0.85 Volts
		sensor.threshold_unr_rawvalue(239); // 0.935 Volts
		sensor.threshold_ucr_rawvalue(234); // 0.9137 Volts
		sensor.threshold_unc_rawvalue(228); // 0.8925 Volts
		sensor.threshold_lnc_rawvalue(207); // 0.8075 Volts
		sensor.threshold_lcr_rawvalue(201); // 0.7863 Volts
		sensor.threshold_lnr_rawvalue(196); // 0.765 Volts
		sensor.hysteresis_high(0); // +0 Volts
		sensor.hysteresis_low(0); // -0 Volts
		ADD_TO_REPO(sensor);
		if (!ipmc_sensors.get(sensor.sensor_number()))
			ipmc_sensors.add(std::make_shared<ThresholdSensor>(sensor.record_key(), LOG["sensors"]["+0.85VDD"]));
	}

	{
		SensorDataRecord01 sensor;
		// SDR Calculator.html#precision=4&s-na=%2B0.9VMGTB&s-no=3&s-t=0x02&s-u-p=4&lnrf=0.81&lcrf=0.8325&lncf=0.855&uncf=0.945&ucrf=0.9675&unrf=0.99&nominalf=0.9&minf=0&granularity=0.0041
		sensor.initialize_blank("+0.9VMGTB");
		sensor.sensor_owner_id(0); // Tag as "self". This will be auto-calculated in "Get SDR" commands.
		sensor.sensor_owner_channel(0); // See above.
		sensor.sensor_owner_lun(0); // Generally zero
		sensor.sensor_number(3);
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
		sensor.deassertion_upper_threshold_reading_mask(0x7fff); // All events supported & LNR, LCR, LNC, UNC, UCR, UNR deassertions enabled.
		sensor.discrete_reading_setable_threshold_reading_mask(0x3fff); // All thresholds are configurable.
		sensor.units_numeric_format(SensorDataRecord01::UNITS_UNSIGNED);
		sensor.units_rate_unit(SensorDataRecordReadableSensor::RATE_UNIT_NONE);
		sensor.units_base_unit(4); // Volts
		sensor.units_modifier_unit(0); // unspecified
		sensor.units_modifier_unit_method(SensorDataRecordReadableSensor::MODIFIER_UNIT_NONE);
		sensor.linearization(SensorDataRecord01::LIN_LINEAR);
		// IPMI Specifies a linearization function of: y = L[(Mx + (B * 10^(Bexp) ) ) * 10^(Rexp) ]
		// Our settings produce a valid range of 0 (Volts) to 1.0455 (Volts) with 0.0041 Volts granularity.
		sensor.conversion_m(41);
		sensor.conversion_b(0);
		sensor.conversion_b_exp(0);
		sensor.conversion_r_exp(-4);
		sensor.sensor_direction(SensorDataRecordReadableSensor::DIR_UNSPECIFIED);
		//sensor.normal_min_specified(false); // Default
		//sensor.normal_min_rawvalue(0); // Unspecified
		//sensor.normal_max_specified(false); // Default
		//sensor.normal_max_rawvalue(0); // Unspecified
		sensor.nominal_reading_specified(true);
		sensor.nominal_reading_rawvalue(219); // 0.9 Volts
		sensor.threshold_unr_rawvalue(241); // 0.99 Volts
		sensor.threshold_ucr_rawvalue(235); // 0.9675 Volts
		sensor.threshold_unc_rawvalue(230); // 0.945 Volts
		sensor.threshold_lnc_rawvalue(208); // 0.855 Volts
		sensor.threshold_lcr_rawvalue(203); // 0.8325 Volts
		sensor.threshold_lnr_rawvalue(197); // 0.81 Volts
		sensor.hysteresis_high(0); // +0 Volts
		sensor.hysteresis_low(0); // -0 Volts
		ADD_TO_REPO(sensor);
		if (!ipmc_sensors.get(sensor.sensor_number()))
			ipmc_sensors.add(std::make_shared<ThresholdSensor>(sensor.record_key(), LOG["sensors"]["+0.9VMGTB"]));
	}

	{
		SensorDataRecord01 sensor;
		// SDR Calculator.html#precision=4&s-na=%2B0.9VMGTT&s-no=4&s-t=0x02&s-u-p=4&lnrf=0.81&lcrf=0.8325&lncf=0.855&uncf=0.945&ucrf=0.9675&unrf=0.99&nominalf=0.9&minf=0&granularity=0.0041
		sensor.initialize_blank("+0.9VMGTT");
		sensor.sensor_owner_id(0); // Tag as "self". This will be auto-calculated in "Get SDR" commands.
		sensor.sensor_owner_channel(0); // See above.
		sensor.sensor_owner_lun(0); // Generally zero
		sensor.sensor_number(4);
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
		sensor.deassertion_upper_threshold_reading_mask(0x7fff); // All events supported & LNR, LCR, LNC, UNC, UCR, UNR deassertions enabled.
		sensor.discrete_reading_setable_threshold_reading_mask(0x3fff); // All thresholds are configurable.
		sensor.units_numeric_format(SensorDataRecord01::UNITS_UNSIGNED);
		sensor.units_rate_unit(SensorDataRecordReadableSensor::RATE_UNIT_NONE);
		sensor.units_base_unit(4); // Volts
		sensor.units_modifier_unit(0); // unspecified
		sensor.units_modifier_unit_method(SensorDataRecordReadableSensor::MODIFIER_UNIT_NONE);
		sensor.linearization(SensorDataRecord01::LIN_LINEAR);
		// IPMI Specifies a linearization function of: y = L[(Mx + (B * 10^(Bexp) ) ) * 10^(Rexp) ]
		// Our settings produce a valid range of 0 (Volts) to 1.0455 (Volts) with 0.0041 Volts granularity.
		sensor.conversion_m(41);
		sensor.conversion_b(0);
		sensor.conversion_b_exp(0);
		sensor.conversion_r_exp(-4);
		sensor.sensor_direction(SensorDataRecordReadableSensor::DIR_UNSPECIFIED);
		//sensor.normal_min_specified(false); // Default
		//sensor.normal_min_rawvalue(0); // Unspecified
		//sensor.normal_max_specified(false); // Default
		//sensor.normal_max_rawvalue(0); // Unspecified
		sensor.nominal_reading_specified(true);
		sensor.nominal_reading_rawvalue(219); // 0.9 Volts
		sensor.threshold_unr_rawvalue(241); // 0.99 Volts
		sensor.threshold_ucr_rawvalue(235); // 0.9675 Volts
		sensor.threshold_unc_rawvalue(230); // 0.945 Volts
		sensor.threshold_lnc_rawvalue(208); // 0.855 Volts
		sensor.threshold_lcr_rawvalue(203); // 0.8325 Volts
		sensor.threshold_lnr_rawvalue(197); // 0.81 Volts
		sensor.hysteresis_high(0); // +0 Volts
		sensor.hysteresis_low(0); // -0 Volts
		ADD_TO_REPO(sensor);
		if (!ipmc_sensors.get(sensor.sensor_number()))
			ipmc_sensors.add(std::make_shared<ThresholdSensor>(sensor.record_key(), LOG["sensors"]["+0.9VMGTT"]));
	}

	{
		SensorDataRecord01 sensor;
		// SDR Calculator.html#precision=4&s-na=%2B1.05VMGTB&s-no=5&s-t=0x02&s-u-p=4&lnrf=0.945&lcrf=0.9712&lncf=0.9975&uncf=1.1025&ucrf=1.1287&unrf=1.155&nominalf=1.05&minf=0&granularity=0.0048
		sensor.initialize_blank("+1.05VMGTB");
		sensor.sensor_owner_id(0); // Tag as "self". This will be auto-calculated in "Get SDR" commands.
		sensor.sensor_owner_channel(0); // See above.
		sensor.sensor_owner_lun(0); // Generally zero
		sensor.sensor_number(5);
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
		sensor.deassertion_upper_threshold_reading_mask(0x7fff); // All events supported & LNR, LCR, LNC, UNC, UCR, UNR deassertions enabled.
		sensor.discrete_reading_setable_threshold_reading_mask(0x3fff); // All thresholds are configurable.
		sensor.units_numeric_format(SensorDataRecord01::UNITS_UNSIGNED);
		sensor.units_rate_unit(SensorDataRecordReadableSensor::RATE_UNIT_NONE);
		sensor.units_base_unit(4); // Volts
		sensor.units_modifier_unit(0); // unspecified
		sensor.units_modifier_unit_method(SensorDataRecordReadableSensor::MODIFIER_UNIT_NONE);
		sensor.linearization(SensorDataRecord01::LIN_LINEAR);
		// IPMI Specifies a linearization function of: y = L[(Mx + (B * 10^(Bexp) ) ) * 10^(Rexp) ]
		// Our settings produce a valid range of 0 (Volts) to 1.224 (Volts) with 0.0048 Volts granularity.
		sensor.conversion_m(48);
		sensor.conversion_b(0);
		sensor.conversion_b_exp(0);
		sensor.conversion_r_exp(-4);
		sensor.sensor_direction(SensorDataRecordReadableSensor::DIR_UNSPECIFIED);
		//sensor.normal_min_specified(false); // Default
		//sensor.normal_min_rawvalue(0); // Unspecified
		//sensor.normal_max_specified(false); // Default
		//sensor.normal_max_rawvalue(0); // Unspecified
		sensor.nominal_reading_specified(true);
		sensor.nominal_reading_rawvalue(218); // 1.05 Volts
		sensor.threshold_unr_rawvalue(240); // 1.155 Volts
		sensor.threshold_ucr_rawvalue(235); // 1.1287 Volts
		sensor.threshold_unc_rawvalue(229); // 1.1025 Volts
		sensor.threshold_lnc_rawvalue(207); // 0.9975 Volts
		sensor.threshold_lcr_rawvalue(202); // 0.9712 Volts
		sensor.threshold_lnr_rawvalue(196); // 0.945 Volts
		sensor.hysteresis_high(0); // +0 Volts
		sensor.hysteresis_low(0); // -0 Volts
		ADD_TO_REPO(sensor);
		if (!ipmc_sensors.get(sensor.sensor_number()))
			ipmc_sensors.add(std::make_shared<ThresholdSensor>(sensor.record_key(), LOG["sensors"]["+1.05VMGTB"]));
	}

	{
		SensorDataRecord01 sensor;
		// SDR Calculator.html#precision=4&s-na=%2B1.05VMGTT&s-no=6&s-t=0x02&s-u-p=4&lnrf=0.945&lcrf=0.9712&lncf=0.9975&uncf=1.1025&ucrf=1.1287&unrf=1.155&nominalf=1.05&minf=0&granularity=0.0048
		sensor.initialize_blank("+1.05VMGTT");
		sensor.sensor_owner_id(0); // Tag as "self". This will be auto-calculated in "Get SDR" commands.
		sensor.sensor_owner_channel(0); // See above.
		sensor.sensor_owner_lun(0); // Generally zero
		sensor.sensor_number(6);
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
		sensor.deassertion_upper_threshold_reading_mask(0x7fff); // All events supported & LNR, LCR, LNC, UNC, UCR, UNR deassertions enabled.
		sensor.discrete_reading_setable_threshold_reading_mask(0x3fff); // All thresholds are configurable.
		sensor.units_numeric_format(SensorDataRecord01::UNITS_UNSIGNED);
		sensor.units_rate_unit(SensorDataRecordReadableSensor::RATE_UNIT_NONE);
		sensor.units_base_unit(4); // Volts
		sensor.units_modifier_unit(0); // unspecified
		sensor.units_modifier_unit_method(SensorDataRecordReadableSensor::MODIFIER_UNIT_NONE);
		sensor.linearization(SensorDataRecord01::LIN_LINEAR);
		// IPMI Specifies a linearization function of: y = L[(Mx + (B * 10^(Bexp) ) ) * 10^(Rexp) ]
		// Our settings produce a valid range of 0 (Volts) to 1.224 (Volts) with 0.0048 Volts granularity.
		sensor.conversion_m(48);
		sensor.conversion_b(0);
		sensor.conversion_b_exp(0);
		sensor.conversion_r_exp(-4);
		sensor.sensor_direction(SensorDataRecordReadableSensor::DIR_UNSPECIFIED);
		//sensor.normal_min_specified(false); // Default
		//sensor.normal_min_rawvalue(0); // Unspecified
		//sensor.normal_max_specified(false); // Default
		//sensor.normal_max_rawvalue(0); // Unspecified
		sensor.nominal_reading_specified(true);
		sensor.nominal_reading_rawvalue(218); // 1.05 Volts
		sensor.threshold_unr_rawvalue(240); // 1.155 Volts
		sensor.threshold_ucr_rawvalue(235); // 1.1287 Volts
		sensor.threshold_unc_rawvalue(229); // 1.1025 Volts
		sensor.threshold_lnc_rawvalue(207); // 0.9975 Volts
		sensor.threshold_lcr_rawvalue(202); // 0.9712 Volts
		sensor.threshold_lnr_rawvalue(196); // 0.945 Volts
		sensor.hysteresis_high(0); // +0 Volts
		sensor.hysteresis_low(0); // -0 Volts
		ADD_TO_REPO(sensor);
		if (!ipmc_sensors.get(sensor.sensor_number()))
			ipmc_sensors.add(std::make_shared<ThresholdSensor>(sensor.record_key(), LOG["sensors"]["+1.05VMGTT"]));
	}

	{
		SensorDataRecord01 sensor;
		// SDR Calculator.html#precision=4&s-na=%2B1.2VMGTB&s-no=7&s-t=0x02&s-u-p=4&lnrf=1.08&lcrf=1.11&lncf=1.14&uncf=1.26&ucrf=1.29&unrf=1.32&nominalf=1.2&minf=0&granularity=0.0055
		sensor.initialize_blank("+1.2VMGTB");
		sensor.sensor_owner_id(0); // Tag as "self". This will be auto-calculated in "Get SDR" commands.
		sensor.sensor_owner_channel(0); // See above.
		sensor.sensor_owner_lun(0); // Generally zero
		sensor.sensor_number(7);
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
		sensor.deassertion_upper_threshold_reading_mask(0x7fff); // All events supported & LNR, LCR, LNC, UNC, UCR, UNR deassertions enabled.
		sensor.discrete_reading_setable_threshold_reading_mask(0x3fff); // All thresholds are configurable.
		sensor.units_numeric_format(SensorDataRecord01::UNITS_UNSIGNED);
		sensor.units_rate_unit(SensorDataRecordReadableSensor::RATE_UNIT_NONE);
		sensor.units_base_unit(4); // Volts
		sensor.units_modifier_unit(0); // unspecified
		sensor.units_modifier_unit_method(SensorDataRecordReadableSensor::MODIFIER_UNIT_NONE);
		sensor.linearization(SensorDataRecord01::LIN_LINEAR);
		// IPMI Specifies a linearization function of: y = L[(Mx + (B * 10^(Bexp) ) ) * 10^(Rexp) ]
		// Our settings produce a valid range of 0 (Volts) to 1.4025 (Volts) with 0.0055 Volts granularity.
		sensor.conversion_m(55);
		sensor.conversion_b(0);
		sensor.conversion_b_exp(0);
		sensor.conversion_r_exp(-4);
		sensor.sensor_direction(SensorDataRecordReadableSensor::DIR_UNSPECIFIED);
		//sensor.normal_min_specified(false); // Default
		//sensor.normal_min_rawvalue(0); // Unspecified
		//sensor.normal_max_specified(false); // Default
		//sensor.normal_max_rawvalue(0); // Unspecified
		sensor.nominal_reading_specified(true);
		sensor.nominal_reading_rawvalue(218); // 1.2 Volts
		sensor.threshold_unr_rawvalue(240); // 1.32 Volts
		sensor.threshold_ucr_rawvalue(234); // 1.29 Volts
		sensor.threshold_unc_rawvalue(229); // 1.26 Volts
		sensor.threshold_lnc_rawvalue(207); // 1.14 Volts
		sensor.threshold_lcr_rawvalue(201); // 1.11 Volts
		sensor.threshold_lnr_rawvalue(196); // 1.08 Volts
		sensor.hysteresis_high(0); // +0 Volts
		sensor.hysteresis_low(0); // -0 Volts
		ADD_TO_REPO(sensor);
		if (!ipmc_sensors.get(sensor.sensor_number()))
			ipmc_sensors.add(std::make_shared<ThresholdSensor>(sensor.record_key(), LOG["sensors"]["+1.2VMGTB"]));
	}

	{
		SensorDataRecord01 sensor;
		// SDR Calculator.html#precision=4&s-na=%2B1.2VMGTT&s-no=8&s-t=0x02&s-u-p=4&lnrf=1.08&lcrf=1.11&lncf=1.14&uncf=1.26&ucrf=1.29&unrf=1.32&nominalf=1.2&minf=0&granularity=0.0055
		sensor.initialize_blank("+1.2VMGTT");
		sensor.sensor_owner_id(0); // Tag as "self". This will be auto-calculated in "Get SDR" commands.
		sensor.sensor_owner_channel(0); // See above.
		sensor.sensor_owner_lun(0); // Generally zero
		sensor.sensor_number(8);
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
		sensor.deassertion_upper_threshold_reading_mask(0x7fff); // All events supported & LNR, LCR, LNC, UNC, UCR, UNR deassertions enabled.
		sensor.discrete_reading_setable_threshold_reading_mask(0x3fff); // All thresholds are configurable.
		sensor.units_numeric_format(SensorDataRecord01::UNITS_UNSIGNED);
		sensor.units_rate_unit(SensorDataRecordReadableSensor::RATE_UNIT_NONE);
		sensor.units_base_unit(4); // Volts
		sensor.units_modifier_unit(0); // unspecified
		sensor.units_modifier_unit_method(SensorDataRecordReadableSensor::MODIFIER_UNIT_NONE);
		sensor.linearization(SensorDataRecord01::LIN_LINEAR);
		// IPMI Specifies a linearization function of: y = L[(Mx + (B * 10^(Bexp) ) ) * 10^(Rexp) ]
		// Our settings produce a valid range of 0 (Volts) to 1.4025 (Volts) with 0.0055 Volts granularity.
		sensor.conversion_m(55);
		sensor.conversion_b(0);
		sensor.conversion_b_exp(0);
		sensor.conversion_r_exp(-4);
		sensor.sensor_direction(SensorDataRecordReadableSensor::DIR_UNSPECIFIED);
		//sensor.normal_min_specified(false); // Default
		//sensor.normal_min_rawvalue(0); // Unspecified
		//sensor.normal_max_specified(false); // Default
		//sensor.normal_max_rawvalue(0); // Unspecified
		sensor.nominal_reading_specified(true);
		sensor.nominal_reading_rawvalue(218); // 1.2 Volts
		sensor.threshold_unr_rawvalue(240); // 1.32 Volts
		sensor.threshold_ucr_rawvalue(234); // 1.29 Volts
		sensor.threshold_unc_rawvalue(229); // 1.26 Volts
		sensor.threshold_lnc_rawvalue(207); // 1.14 Volts
		sensor.threshold_lcr_rawvalue(201); // 1.11 Volts
		sensor.threshold_lnr_rawvalue(196); // 1.08 Volts
		sensor.hysteresis_high(0); // +0 Volts
		sensor.hysteresis_low(0); // -0 Volts
		ADD_TO_REPO(sensor);
		if (!ipmc_sensors.get(sensor.sensor_number()))
			ipmc_sensors.add(std::make_shared<ThresholdSensor>(sensor.record_key(), LOG["sensors"]["+1.2VMGTT"]));
	}

	{
		SensorDataRecord01 sensor;
		// SDR Calculator.html#precision=4&s-na=%2B1.2VPHY&s-no=9&s-t=0x02&s-u-p=4&lnrf=1.08&lcrf=1.11&lncf=1.14&uncf=1.26&ucrf=1.29&unrf=1.32&nominalf=1.2&minf=0&granularity=0.0055
		sensor.initialize_blank("+1.2VPHY");
		sensor.sensor_owner_id(0); // Tag as "self". This will be auto-calculated in "Get SDR" commands.
		sensor.sensor_owner_channel(0); // See above.
		sensor.sensor_owner_lun(0); // Generally zero
		sensor.sensor_number(9);
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
		sensor.deassertion_upper_threshold_reading_mask(0x7fff); // All events supported & LNR, LCR, LNC, UNC, UCR, UNR deassertions enabled.
		sensor.discrete_reading_setable_threshold_reading_mask(0x3fff); // All thresholds are configurable.
		sensor.units_numeric_format(SensorDataRecord01::UNITS_UNSIGNED);
		sensor.units_rate_unit(SensorDataRecordReadableSensor::RATE_UNIT_NONE);
		sensor.units_base_unit(4); // Volts
		sensor.units_modifier_unit(0); // unspecified
		sensor.units_modifier_unit_method(SensorDataRecordReadableSensor::MODIFIER_UNIT_NONE);
		sensor.linearization(SensorDataRecord01::LIN_LINEAR);
		// IPMI Specifies a linearization function of: y = L[(Mx + (B * 10^(Bexp) ) ) * 10^(Rexp) ]
		// Our settings produce a valid range of 0 (Volts) to 1.4025 (Volts) with 0.0055 Volts granularity.
		sensor.conversion_m(55);
		sensor.conversion_b(0);
		sensor.conversion_b_exp(0);
		sensor.conversion_r_exp(-4);
		sensor.sensor_direction(SensorDataRecordReadableSensor::DIR_UNSPECIFIED);
		//sensor.normal_min_specified(false); // Default
		//sensor.normal_min_rawvalue(0); // Unspecified
		//sensor.normal_max_specified(false); // Default
		//sensor.normal_max_rawvalue(0); // Unspecified
		sensor.nominal_reading_specified(true);
		sensor.nominal_reading_rawvalue(218); // 1.2 Volts
		sensor.threshold_unr_rawvalue(240); // 1.32 Volts
		sensor.threshold_ucr_rawvalue(234); // 1.29 Volts
		sensor.threshold_unc_rawvalue(229); // 1.26 Volts
		sensor.threshold_lnc_rawvalue(207); // 1.14 Volts
		sensor.threshold_lcr_rawvalue(201); // 1.11 Volts
		sensor.threshold_lnr_rawvalue(196); // 1.08 Volts
		sensor.hysteresis_high(0); // +0 Volts
		sensor.hysteresis_low(0); // -0 Volts
		ADD_TO_REPO(sensor);
		if (!ipmc_sensors.get(sensor.sensor_number()))
			ipmc_sensors.add(std::make_shared<ThresholdSensor>(sensor.record_key(), LOG["sensors"]["+1.2VPHY"]));
	}

	{
		SensorDataRecord01 sensor;
		// SDR Calculator.html#precision=4&s-na=%2B1.35VMGTB&s-no=10&s-t=0x02&s-u-p=4&lnrf=1.215&lcrf=1.2488&lncf=1.2825&uncf=1.4175&ucrf=1.4512&unrf=1.485&nominalf=1.35&minf=0&granularity=0.0061
		sensor.initialize_blank("+1.35VMGTB");
		sensor.sensor_owner_id(0); // Tag as "self". This will be auto-calculated in "Get SDR" commands.
		sensor.sensor_owner_channel(0); // See above.
		sensor.sensor_owner_lun(0); // Generally zero
		sensor.sensor_number(10);
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
		sensor.deassertion_upper_threshold_reading_mask(0x7fff); // All events supported & LNR, LCR, LNC, UNC, UCR, UNR deassertions enabled.
		sensor.discrete_reading_setable_threshold_reading_mask(0x3fff); // All thresholds are configurable.
		sensor.units_numeric_format(SensorDataRecord01::UNITS_UNSIGNED);
		sensor.units_rate_unit(SensorDataRecordReadableSensor::RATE_UNIT_NONE);
		sensor.units_base_unit(4); // Volts
		sensor.units_modifier_unit(0); // unspecified
		sensor.units_modifier_unit_method(SensorDataRecordReadableSensor::MODIFIER_UNIT_NONE);
		sensor.linearization(SensorDataRecord01::LIN_LINEAR);
		// IPMI Specifies a linearization function of: y = L[(Mx + (B * 10^(Bexp) ) ) * 10^(Rexp) ]
		// Our settings produce a valid range of 0 (Volts) to 1.5555 (Volts) with 0.0061 Volts granularity.
		sensor.conversion_m(61);
		sensor.conversion_b(0);
		sensor.conversion_b_exp(0);
		sensor.conversion_r_exp(-4);
		sensor.sensor_direction(SensorDataRecordReadableSensor::DIR_UNSPECIFIED);
		//sensor.normal_min_specified(false); // Default
		//sensor.normal_min_rawvalue(0); // Unspecified
		//sensor.normal_max_specified(false); // Default
		//sensor.normal_max_rawvalue(0); // Unspecified
		sensor.nominal_reading_specified(true);
		sensor.nominal_reading_rawvalue(221); // 1.35 Volts
		sensor.threshold_unr_rawvalue(243); // 1.485 Volts
		sensor.threshold_ucr_rawvalue(237); // 1.4512 Volts
		sensor.threshold_unc_rawvalue(232); // 1.4175 Volts
		sensor.threshold_lnc_rawvalue(210); // 1.2825 Volts
		sensor.threshold_lcr_rawvalue(204); // 1.2488 Volts
		sensor.threshold_lnr_rawvalue(199); // 1.215 Volts
		sensor.hysteresis_high(0); // +0 Volts
		sensor.hysteresis_low(0); // -0 Volts
		ADD_TO_REPO(sensor);
		if (!ipmc_sensors.get(sensor.sensor_number()))
			ipmc_sensors.add(std::make_shared<ThresholdSensor>(sensor.record_key(), LOG["sensors"]["+1.35VMGTB"]));
	}

	{
		SensorDataRecord01 sensor;
		// SDR Calculator.html#precision=4&s-na=%2B1.35VMGTT&s-no=11&s-t=0x02&s-u-p=4&lnrf=1.215&lcrf=1.2488&lncf=1.2825&uncf=1.4175&ucrf=1.4512&unrf=1.485&nominalf=1.35&minf=0&granularity=0.0061
		sensor.initialize_blank("+1.35VMGTT");
		sensor.sensor_owner_id(0); // Tag as "self". This will be auto-calculated in "Get SDR" commands.
		sensor.sensor_owner_channel(0); // See above.
		sensor.sensor_owner_lun(0); // Generally zero
		sensor.sensor_number(11);
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
		sensor.deassertion_upper_threshold_reading_mask(0x7fff); // All events supported & LNR, LCR, LNC, UNC, UCR, UNR deassertions enabled.
		sensor.discrete_reading_setable_threshold_reading_mask(0x3fff); // All thresholds are configurable.
		sensor.units_numeric_format(SensorDataRecord01::UNITS_UNSIGNED);
		sensor.units_rate_unit(SensorDataRecordReadableSensor::RATE_UNIT_NONE);
		sensor.units_base_unit(4); // Volts
		sensor.units_modifier_unit(0); // unspecified
		sensor.units_modifier_unit_method(SensorDataRecordReadableSensor::MODIFIER_UNIT_NONE);
		sensor.linearization(SensorDataRecord01::LIN_LINEAR);
		// IPMI Specifies a linearization function of: y = L[(Mx + (B * 10^(Bexp) ) ) * 10^(Rexp) ]
		// Our settings produce a valid range of 0 (Volts) to 1.5555 (Volts) with 0.0061 Volts granularity.
		sensor.conversion_m(61);
		sensor.conversion_b(0);
		sensor.conversion_b_exp(0);
		sensor.conversion_r_exp(-4);
		sensor.sensor_direction(SensorDataRecordReadableSensor::DIR_UNSPECIFIED);
		//sensor.normal_min_specified(false); // Default
		//sensor.normal_min_rawvalue(0); // Unspecified
		//sensor.normal_max_specified(false); // Default
		//sensor.normal_max_rawvalue(0); // Unspecified
		sensor.nominal_reading_specified(true);
		sensor.nominal_reading_rawvalue(221); // 1.35 Volts
		sensor.threshold_unr_rawvalue(243); // 1.485 Volts
		sensor.threshold_ucr_rawvalue(237); // 1.4512 Volts
		sensor.threshold_unc_rawvalue(232); // 1.4175 Volts
		sensor.threshold_lnc_rawvalue(210); // 1.2825 Volts
		sensor.threshold_lcr_rawvalue(204); // 1.2488 Volts
		sensor.threshold_lnr_rawvalue(199); // 1.215 Volts
		sensor.hysteresis_high(0); // +0 Volts
		sensor.hysteresis_low(0); // -0 Volts
		ADD_TO_REPO(sensor);
		if (!ipmc_sensors.get(sensor.sensor_number()))
			ipmc_sensors.add(std::make_shared<ThresholdSensor>(sensor.record_key(), LOG["sensors"]["+1.35VMGTT"]));
	}

	{
		SensorDataRecord01 sensor;
		// SDR Calculator.html#precision=4&s-na=%2B1.8VDD&s-no=12&s-t=0x02&s-u-p=4&lnrf=1.62&lcrf=1.665&lncf=1.71&uncf=1.89&ucrf=1.935&unrf=1.98&nominalf=1.8&minf=0&granularity=0.0082
		sensor.initialize_blank("+1.8VDD");
		sensor.sensor_owner_id(0); // Tag as "self". This will be auto-calculated in "Get SDR" commands.
		sensor.sensor_owner_channel(0); // See above.
		sensor.sensor_owner_lun(0); // Generally zero
		sensor.sensor_number(12);
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
		sensor.deassertion_upper_threshold_reading_mask(0x7fff); // All events supported & LNR, LCR, LNC, UNC, UCR, UNR deassertions enabled.
		sensor.discrete_reading_setable_threshold_reading_mask(0x3fff); // All thresholds are configurable.
		sensor.units_numeric_format(SensorDataRecord01::UNITS_UNSIGNED);
		sensor.units_rate_unit(SensorDataRecordReadableSensor::RATE_UNIT_NONE);
		sensor.units_base_unit(4); // Volts
		sensor.units_modifier_unit(0); // unspecified
		sensor.units_modifier_unit_method(SensorDataRecordReadableSensor::MODIFIER_UNIT_NONE);
		sensor.linearization(SensorDataRecord01::LIN_LINEAR);
		// IPMI Specifies a linearization function of: y = L[(Mx + (B * 10^(Bexp) ) ) * 10^(Rexp) ]
		// Our settings produce a valid range of 0 (Volts) to 2.091 (Volts) with 0.0082 Volts granularity.
		sensor.conversion_m(82);
		sensor.conversion_b(0);
		sensor.conversion_b_exp(0);
		sensor.conversion_r_exp(-4);
		sensor.sensor_direction(SensorDataRecordReadableSensor::DIR_UNSPECIFIED);
		//sensor.normal_min_specified(false); // Default
		//sensor.normal_min_rawvalue(0); // Unspecified
		//sensor.normal_max_specified(false); // Default
		//sensor.normal_max_rawvalue(0); // Unspecified
		sensor.nominal_reading_specified(true);
		sensor.nominal_reading_rawvalue(219); // 1.8 Volts
		sensor.threshold_unr_rawvalue(241); // 1.98 Volts
		sensor.threshold_ucr_rawvalue(235); // 1.935 Volts
		sensor.threshold_unc_rawvalue(230); // 1.89 Volts
		sensor.threshold_lnc_rawvalue(208); // 1.71 Volts
		sensor.threshold_lcr_rawvalue(203); // 1.665 Volts
		sensor.threshold_lnr_rawvalue(197); // 1.62 Volts
		sensor.hysteresis_high(0); // +0 Volts
		sensor.hysteresis_low(0); // -0 Volts
		ADD_TO_REPO(sensor);
		if (!ipmc_sensors.get(sensor.sensor_number()))
			ipmc_sensors.add(std::make_shared<ThresholdSensor>(sensor.record_key(), LOG["sensors"]["+1.8VDD"]));
	}

	{
		SensorDataRecord01 sensor;
		// SDR Calculator.html#precision=4&s-na=%2B1.8VFFLY1&s-no=13&s-t=0x02&s-u-p=4&lnrf=1.62&lcrf=1.665&lncf=1.71&uncf=1.89&ucrf=1.935&unrf=1.98&nominalf=1.8&minf=0&granularity=0.0082
		sensor.initialize_blank("+1.8VFFLY1");
		sensor.sensor_owner_id(0); // Tag as "self". This will be auto-calculated in "Get SDR" commands.
		sensor.sensor_owner_channel(0); // See above.
		sensor.sensor_owner_lun(0); // Generally zero
		sensor.sensor_number(13);
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
		sensor.deassertion_upper_threshold_reading_mask(0x7fff); // All events supported & LNR, LCR, LNC, UNC, UCR, UNR deassertions enabled.
		sensor.discrete_reading_setable_threshold_reading_mask(0x3fff); // All thresholds are configurable.
		sensor.units_numeric_format(SensorDataRecord01::UNITS_UNSIGNED);
		sensor.units_rate_unit(SensorDataRecordReadableSensor::RATE_UNIT_NONE);
		sensor.units_base_unit(4); // Volts
		sensor.units_modifier_unit(0); // unspecified
		sensor.units_modifier_unit_method(SensorDataRecordReadableSensor::MODIFIER_UNIT_NONE);
		sensor.linearization(SensorDataRecord01::LIN_LINEAR);
		// IPMI Specifies a linearization function of: y = L[(Mx + (B * 10^(Bexp) ) ) * 10^(Rexp) ]
		// Our settings produce a valid range of 0 (Volts) to 2.091 (Volts) with 0.0082 Volts granularity.
		sensor.conversion_m(82);
		sensor.conversion_b(0);
		sensor.conversion_b_exp(0);
		sensor.conversion_r_exp(-4);
		sensor.sensor_direction(SensorDataRecordReadableSensor::DIR_UNSPECIFIED);
		//sensor.normal_min_specified(false); // Default
		//sensor.normal_min_rawvalue(0); // Unspecified
		//sensor.normal_max_specified(false); // Default
		//sensor.normal_max_rawvalue(0); // Unspecified
		sensor.nominal_reading_specified(true);
		sensor.nominal_reading_rawvalue(219); // 1.8 Volts
		sensor.threshold_unr_rawvalue(241); // 1.98 Volts
		sensor.threshold_ucr_rawvalue(235); // 1.935 Volts
		sensor.threshold_unc_rawvalue(230); // 1.89 Volts
		sensor.threshold_lnc_rawvalue(208); // 1.71 Volts
		sensor.threshold_lcr_rawvalue(203); // 1.665 Volts
		sensor.threshold_lnr_rawvalue(197); // 1.62 Volts
		sensor.hysteresis_high(0); // +0 Volts
		sensor.hysteresis_low(0); // -0 Volts
		ADD_TO_REPO(sensor);
		if (!ipmc_sensors.get(sensor.sensor_number()))
			ipmc_sensors.add(std::make_shared<ThresholdSensor>(sensor.record_key(), LOG["sensors"]["+1.8VFFLY1"]));
	}

	{
		SensorDataRecord01 sensor;
		// SDR Calculator.html#precision=4&s-na=%2B1.8VFFLY2&s-no=14&s-t=0x02&s-u-p=4&lnrf=1.62&lcrf=1.665&lncf=1.71&uncf=1.89&ucrf=1.935&unrf=1.98&nominalf=1.8&minf=0&granularity=0.0082
		sensor.initialize_blank("+1.8VFFLY2");
		sensor.sensor_owner_id(0); // Tag as "self". This will be auto-calculated in "Get SDR" commands.
		sensor.sensor_owner_channel(0); // See above.
		sensor.sensor_owner_lun(0); // Generally zero
		sensor.sensor_number(14);
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
		sensor.deassertion_upper_threshold_reading_mask(0x7fff); // All events supported & LNR, LCR, LNC, UNC, UCR, UNR deassertions enabled.
		sensor.discrete_reading_setable_threshold_reading_mask(0x3fff); // All thresholds are configurable.
		sensor.units_numeric_format(SensorDataRecord01::UNITS_UNSIGNED);
		sensor.units_rate_unit(SensorDataRecordReadableSensor::RATE_UNIT_NONE);
		sensor.units_base_unit(4); // Volts
		sensor.units_modifier_unit(0); // unspecified
		sensor.units_modifier_unit_method(SensorDataRecordReadableSensor::MODIFIER_UNIT_NONE);
		sensor.linearization(SensorDataRecord01::LIN_LINEAR);
		// IPMI Specifies a linearization function of: y = L[(Mx + (B * 10^(Bexp) ) ) * 10^(Rexp) ]
		// Our settings produce a valid range of 0 (Volts) to 2.091 (Volts) with 0.0082 Volts granularity.
		sensor.conversion_m(82);
		sensor.conversion_b(0);
		sensor.conversion_b_exp(0);
		sensor.conversion_r_exp(-4);
		sensor.sensor_direction(SensorDataRecordReadableSensor::DIR_UNSPECIFIED);
		//sensor.normal_min_specified(false); // Default
		//sensor.normal_min_rawvalue(0); // Unspecified
		//sensor.normal_max_specified(false); // Default
		//sensor.normal_max_rawvalue(0); // Unspecified
		sensor.nominal_reading_specified(true);
		sensor.nominal_reading_rawvalue(219); // 1.8 Volts
		sensor.threshold_unr_rawvalue(241); // 1.98 Volts
		sensor.threshold_ucr_rawvalue(235); // 1.935 Volts
		sensor.threshold_unc_rawvalue(230); // 1.89 Volts
		sensor.threshold_lnc_rawvalue(208); // 1.71 Volts
		sensor.threshold_lcr_rawvalue(203); // 1.665 Volts
		sensor.threshold_lnr_rawvalue(197); // 1.62 Volts
		sensor.hysteresis_high(0); // +0 Volts
		sensor.hysteresis_low(0); // -0 Volts
		ADD_TO_REPO(sensor);
		if (!ipmc_sensors.get(sensor.sensor_number()))
			ipmc_sensors.add(std::make_shared<ThresholdSensor>(sensor.record_key(), LOG["sensors"]["+1.8VFFLY2"]));
	}

	{
		SensorDataRecord01 sensor;
		// SDR Calculator.html#precision=4&s-na=%2B1.8VFFLY3&s-no=15&s-t=0x02&s-u-p=4&lnrf=1.62&lcrf=1.665&lncf=1.71&uncf=1.89&ucrf=1.935&unrf=1.98&nominalf=1.8&minf=0&granularity=0.0082
		sensor.initialize_blank("+1.8VFFLY3");
		sensor.sensor_owner_id(0); // Tag as "self". This will be auto-calculated in "Get SDR" commands.
		sensor.sensor_owner_channel(0); // See above.
		sensor.sensor_owner_lun(0); // Generally zero
		sensor.sensor_number(15);
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
		sensor.deassertion_upper_threshold_reading_mask(0x7fff); // All events supported & LNR, LCR, LNC, UNC, UCR, UNR deassertions enabled.
		sensor.discrete_reading_setable_threshold_reading_mask(0x3fff); // All thresholds are configurable.
		sensor.units_numeric_format(SensorDataRecord01::UNITS_UNSIGNED);
		sensor.units_rate_unit(SensorDataRecordReadableSensor::RATE_UNIT_NONE);
		sensor.units_base_unit(4); // Volts
		sensor.units_modifier_unit(0); // unspecified
		sensor.units_modifier_unit_method(SensorDataRecordReadableSensor::MODIFIER_UNIT_NONE);
		sensor.linearization(SensorDataRecord01::LIN_LINEAR);
		// IPMI Specifies a linearization function of: y = L[(Mx + (B * 10^(Bexp) ) ) * 10^(Rexp) ]
		// Our settings produce a valid range of 0 (Volts) to 2.091 (Volts) with 0.0082 Volts granularity.
		sensor.conversion_m(82);
		sensor.conversion_b(0);
		sensor.conversion_b_exp(0);
		sensor.conversion_r_exp(-4);
		sensor.sensor_direction(SensorDataRecordReadableSensor::DIR_UNSPECIFIED);
		//sensor.normal_min_specified(false); // Default
		//sensor.normal_min_rawvalue(0); // Unspecified
		//sensor.normal_max_specified(false); // Default
		//sensor.normal_max_rawvalue(0); // Unspecified
		sensor.nominal_reading_specified(true);
		sensor.nominal_reading_rawvalue(219); // 1.8 Volts
		sensor.threshold_unr_rawvalue(241); // 1.98 Volts
		sensor.threshold_ucr_rawvalue(235); // 1.935 Volts
		sensor.threshold_unc_rawvalue(230); // 1.89 Volts
		sensor.threshold_lnc_rawvalue(208); // 1.71 Volts
		sensor.threshold_lcr_rawvalue(203); // 1.665 Volts
		sensor.threshold_lnr_rawvalue(197); // 1.62 Volts
		sensor.hysteresis_high(0); // +0 Volts
		sensor.hysteresis_low(0); // -0 Volts
		ADD_TO_REPO(sensor);
		if (!ipmc_sensors.get(sensor.sensor_number()))
			ipmc_sensors.add(std::make_shared<ThresholdSensor>(sensor.record_key(), LOG["sensors"]["+1.8VFFLY3"]));
	}

	{
		SensorDataRecord01 sensor;
		// SDR Calculator.html#precision=4&s-na=%2B1.8VFFLY4&s-no=16&s-t=0x02&s-u-p=4&lnrf=1.62&lcrf=1.665&lncf=1.71&uncf=1.89&ucrf=1.935&unrf=1.98&nominalf=1.8&minf=0&granularity=0.0082
		sensor.initialize_blank("+1.8VFFLY4");
		sensor.sensor_owner_id(0); // Tag as "self". This will be auto-calculated in "Get SDR" commands.
		sensor.sensor_owner_channel(0); // See above.
		sensor.sensor_owner_lun(0); // Generally zero
		sensor.sensor_number(16);
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
		sensor.deassertion_upper_threshold_reading_mask(0x7fff); // All events supported & LNR, LCR, LNC, UNC, UCR, UNR deassertions enabled.
		sensor.discrete_reading_setable_threshold_reading_mask(0x3fff); // All thresholds are configurable.
		sensor.units_numeric_format(SensorDataRecord01::UNITS_UNSIGNED);
		sensor.units_rate_unit(SensorDataRecordReadableSensor::RATE_UNIT_NONE);
		sensor.units_base_unit(4); // Volts
		sensor.units_modifier_unit(0); // unspecified
		sensor.units_modifier_unit_method(SensorDataRecordReadableSensor::MODIFIER_UNIT_NONE);
		sensor.linearization(SensorDataRecord01::LIN_LINEAR);
		// IPMI Specifies a linearization function of: y = L[(Mx + (B * 10^(Bexp) ) ) * 10^(Rexp) ]
		// Our settings produce a valid range of 0 (Volts) to 2.091 (Volts) with 0.0082 Volts granularity.
		sensor.conversion_m(82);
		sensor.conversion_b(0);
		sensor.conversion_b_exp(0);
		sensor.conversion_r_exp(-4);
		sensor.sensor_direction(SensorDataRecordReadableSensor::DIR_UNSPECIFIED);
		//sensor.normal_min_specified(false); // Default
		//sensor.normal_min_rawvalue(0); // Unspecified
		//sensor.normal_max_specified(false); // Default
		//sensor.normal_max_rawvalue(0); // Unspecified
		sensor.nominal_reading_specified(true);
		sensor.nominal_reading_rawvalue(219); // 1.8 Volts
		sensor.threshold_unr_rawvalue(241); // 1.98 Volts
		sensor.threshold_ucr_rawvalue(235); // 1.935 Volts
		sensor.threshold_unc_rawvalue(230); // 1.89 Volts
		sensor.threshold_lnc_rawvalue(208); // 1.71 Volts
		sensor.threshold_lcr_rawvalue(203); // 1.665 Volts
		sensor.threshold_lnr_rawvalue(197); // 1.62 Volts
		sensor.hysteresis_high(0); // +0 Volts
		sensor.hysteresis_low(0); // -0 Volts
		ADD_TO_REPO(sensor);
		if (!ipmc_sensors.get(sensor.sensor_number()))
			ipmc_sensors.add(std::make_shared<ThresholdSensor>(sensor.record_key(), LOG["sensors"]["+1.8VFFLY4"]));
	}

	{
		SensorDataRecord01 sensor;
		// SDR Calculator.html#precision=4&s-na=%2B1.8VFFLY5&s-no=17&s-t=0x02&s-u-p=4&lnrf=1.62&lcrf=1.665&lncf=1.71&uncf=1.89&ucrf=1.935&unrf=1.98&nominalf=1.8&minf=0&granularity=0.0082
		sensor.initialize_blank("+1.8VFFLY5");
		sensor.sensor_owner_id(0); // Tag as "self". This will be auto-calculated in "Get SDR" commands.
		sensor.sensor_owner_channel(0); // See above.
		sensor.sensor_owner_lun(0); // Generally zero
		sensor.sensor_number(17);
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
		sensor.deassertion_upper_threshold_reading_mask(0x7fff); // All events supported & LNR, LCR, LNC, UNC, UCR, UNR deassertions enabled.
		sensor.discrete_reading_setable_threshold_reading_mask(0x3fff); // All thresholds are configurable.
		sensor.units_numeric_format(SensorDataRecord01::UNITS_UNSIGNED);
		sensor.units_rate_unit(SensorDataRecordReadableSensor::RATE_UNIT_NONE);
		sensor.units_base_unit(4); // Volts
		sensor.units_modifier_unit(0); // unspecified
		sensor.units_modifier_unit_method(SensorDataRecordReadableSensor::MODIFIER_UNIT_NONE);
		sensor.linearization(SensorDataRecord01::LIN_LINEAR);
		// IPMI Specifies a linearization function of: y = L[(Mx + (B * 10^(Bexp) ) ) * 10^(Rexp) ]
		// Our settings produce a valid range of 0 (Volts) to 2.091 (Volts) with 0.0082 Volts granularity.
		sensor.conversion_m(82);
		sensor.conversion_b(0);
		sensor.conversion_b_exp(0);
		sensor.conversion_r_exp(-4);
		sensor.sensor_direction(SensorDataRecordReadableSensor::DIR_UNSPECIFIED);
		//sensor.normal_min_specified(false); // Default
		//sensor.normal_min_rawvalue(0); // Unspecified
		//sensor.normal_max_specified(false); // Default
		//sensor.normal_max_rawvalue(0); // Unspecified
		sensor.nominal_reading_specified(true);
		sensor.nominal_reading_rawvalue(219); // 1.8 Volts
		sensor.threshold_unr_rawvalue(241); // 1.98 Volts
		sensor.threshold_ucr_rawvalue(235); // 1.935 Volts
		sensor.threshold_unc_rawvalue(230); // 1.89 Volts
		sensor.threshold_lnc_rawvalue(208); // 1.71 Volts
		sensor.threshold_lcr_rawvalue(203); // 1.665 Volts
		sensor.threshold_lnr_rawvalue(197); // 1.62 Volts
		sensor.hysteresis_high(0); // +0 Volts
		sensor.hysteresis_low(0); // -0 Volts
		ADD_TO_REPO(sensor);
		if (!ipmc_sensors.get(sensor.sensor_number()))
			ipmc_sensors.add(std::make_shared<ThresholdSensor>(sensor.record_key(), LOG["sensors"]["+1.8VFFLY5"]));
	}

	{
		SensorDataRecord01 sensor;
		// SDR Calculator.html#precision=4&s-na=%2B1.95VBULK&s-no=18&s-t=0x02&s-u-p=4&lnrf=1.755&lcrf=1.8037&lncf=1.8525&uncf=2.0475&ucrf=2.0962&unrf=2.145&nominalf=1.95&minf=0&granularity=0.0088
		sensor.initialize_blank("+1.95VBULK");
		sensor.sensor_owner_id(0); // Tag as "self". This will be auto-calculated in "Get SDR" commands.
		sensor.sensor_owner_channel(0); // See above.
		sensor.sensor_owner_lun(0); // Generally zero
		sensor.sensor_number(18);
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
		sensor.deassertion_upper_threshold_reading_mask(0x7fff); // All events supported & LNR, LCR, LNC, UNC, UCR, UNR deassertions enabled.
		sensor.discrete_reading_setable_threshold_reading_mask(0x3fff); // All thresholds are configurable.
		sensor.units_numeric_format(SensorDataRecord01::UNITS_UNSIGNED);
		sensor.units_rate_unit(SensorDataRecordReadableSensor::RATE_UNIT_NONE);
		sensor.units_base_unit(4); // Volts
		sensor.units_modifier_unit(0); // unspecified
		sensor.units_modifier_unit_method(SensorDataRecordReadableSensor::MODIFIER_UNIT_NONE);
		sensor.linearization(SensorDataRecord01::LIN_LINEAR);
		// IPMI Specifies a linearization function of: y = L[(Mx + (B * 10^(Bexp) ) ) * 10^(Rexp) ]
		// Our settings produce a valid range of 0 (Volts) to 2.244 (Volts) with 0.0088 Volts granularity.
		sensor.conversion_m(88);
		sensor.conversion_b(0);
		sensor.conversion_b_exp(0);
		sensor.conversion_r_exp(-4);
		sensor.sensor_direction(SensorDataRecordReadableSensor::DIR_UNSPECIFIED);
		//sensor.normal_min_specified(false); // Default
		//sensor.normal_min_rawvalue(0); // Unspecified
		//sensor.normal_max_specified(false); // Default
		//sensor.normal_max_rawvalue(0); // Unspecified
		sensor.nominal_reading_specified(true);
		sensor.nominal_reading_rawvalue(221); // 1.95 Volts
		sensor.threshold_unr_rawvalue(243); // 2.145 Volts
		sensor.threshold_ucr_rawvalue(238); // 2.0962 Volts
		sensor.threshold_unc_rawvalue(232); // 2.0475 Volts
		sensor.threshold_lnc_rawvalue(210); // 1.8525 Volts
		sensor.threshold_lcr_rawvalue(204); // 1.8037 Volts
		sensor.threshold_lnr_rawvalue(199); // 1.755 Volts
		sensor.hysteresis_high(0); // +0 Volts
		sensor.hysteresis_low(0); // -0 Volts
		ADD_TO_REPO(sensor);
		if (!ipmc_sensors.get(sensor.sensor_number()))
			ipmc_sensors.add(std::make_shared<ThresholdSensor>(sensor.record_key(), LOG["sensors"]["+1.95VBULK"]));
	}

	{
		SensorDataRecord01 sensor;
		// SDR Calculator.html#precision=4&s-na=%2B12VPYLD&s-no=19&s-t=0x02&s-u-p=4&lnrf=10.8&lcrf=11.1&lncf=11.4&uncf=12.6&ucrf=12.9&unrf=13.2&nominalf=12&minf=0&granularity=0.0542
		sensor.initialize_blank("+12VPYLD");
		sensor.sensor_owner_id(0); // Tag as "self". This will be auto-calculated in "Get SDR" commands.
		sensor.sensor_owner_channel(0); // See above.
		sensor.sensor_owner_lun(0); // Generally zero
		sensor.sensor_number(19);
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
		sensor.deassertion_upper_threshold_reading_mask(0x7fff); // All events supported & LNR, LCR, LNC, UNC, UCR, UNR deassertions enabled.
		sensor.discrete_reading_setable_threshold_reading_mask(0x3fff); // All thresholds are configurable.
		sensor.units_numeric_format(SensorDataRecord01::UNITS_UNSIGNED);
		sensor.units_rate_unit(SensorDataRecordReadableSensor::RATE_UNIT_NONE);
		sensor.units_base_unit(4); // Volts
		sensor.units_modifier_unit(0); // unspecified
		sensor.units_modifier_unit_method(SensorDataRecordReadableSensor::MODIFIER_UNIT_NONE);
		sensor.linearization(SensorDataRecord01::LIN_LINEAR);
		// IPMI Specifies a linearization function of: y = L[(Mx + (B * 10^(Bexp) ) ) * 10^(Rexp) ]
		// Our settings produce a valid range of 0 (Volts) to 13.821 (Volts) with 0.0542 Volts granularity.
		sensor.conversion_m(542);
		sensor.conversion_b(0);
		sensor.conversion_b_exp(0);
		sensor.conversion_r_exp(-4);
		sensor.sensor_direction(SensorDataRecordReadableSensor::DIR_UNSPECIFIED);
		//sensor.normal_min_specified(false); // Default
		//sensor.normal_min_rawvalue(0); // Unspecified
		//sensor.normal_max_specified(false); // Default
		//sensor.normal_max_rawvalue(0); // Unspecified
		sensor.nominal_reading_specified(true);
		sensor.nominal_reading_rawvalue(221); // 12 Volts
		sensor.threshold_unr_rawvalue(243); // 13.2 Volts
		sensor.threshold_ucr_rawvalue(238); // 12.9 Volts
		sensor.threshold_unc_rawvalue(232); // 12.6 Volts
		sensor.threshold_lnc_rawvalue(210); // 11.4 Volts
		sensor.threshold_lcr_rawvalue(204); // 11.1 Volts
		sensor.threshold_lnr_rawvalue(199); // 10.8 Volts
		sensor.hysteresis_high(0); // +0 Volts
		sensor.hysteresis_low(0); // -0 Volts
		ADD_TO_REPO(sensor);
		if (!ipmc_sensors.get(sensor.sensor_number()))
			ipmc_sensors.add(std::make_shared<ThresholdSensor>(sensor.record_key(), LOG["sensors"]["+12VPYLD"]));
	}

	{
		SensorDataRecord01 sensor;
		// SDR Calculator.html#precision=4&s-na=%2B2.5VXPT&s-no=20&s-t=0x02&s-u-p=4&lnrf=2.25&lcrf=2.3125&lncf=2.375&uncf=2.625&ucrf=2.6875&unrf=2.75&nominalf=2.5&minf=0&granularity=0.0113
		sensor.initialize_blank("+2.5VXPT");
		sensor.sensor_owner_id(0); // Tag as "self". This will be auto-calculated in "Get SDR" commands.
		sensor.sensor_owner_channel(0); // See above.
		sensor.sensor_owner_lun(0); // Generally zero
		sensor.sensor_number(20);
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
		sensor.deassertion_upper_threshold_reading_mask(0x7fff); // All events supported & LNR, LCR, LNC, UNC, UCR, UNR deassertions enabled.
		sensor.discrete_reading_setable_threshold_reading_mask(0x3fff); // All thresholds are configurable.
		sensor.units_numeric_format(SensorDataRecord01::UNITS_UNSIGNED);
		sensor.units_rate_unit(SensorDataRecordReadableSensor::RATE_UNIT_NONE);
		sensor.units_base_unit(4); // Volts
		sensor.units_modifier_unit(0); // unspecified
		sensor.units_modifier_unit_method(SensorDataRecordReadableSensor::MODIFIER_UNIT_NONE);
		sensor.linearization(SensorDataRecord01::LIN_LINEAR);
		// IPMI Specifies a linearization function of: y = L[(Mx + (B * 10^(Bexp) ) ) * 10^(Rexp) ]
		// Our settings produce a valid range of 0 (Volts) to 2.8815 (Volts) with 0.0113 Volts granularity.
		sensor.conversion_m(113);
		sensor.conversion_b(0);
		sensor.conversion_b_exp(0);
		sensor.conversion_r_exp(-4);
		sensor.sensor_direction(SensorDataRecordReadableSensor::DIR_UNSPECIFIED);
		//sensor.normal_min_specified(false); // Default
		//sensor.normal_min_rawvalue(0); // Unspecified
		//sensor.normal_max_specified(false); // Default
		//sensor.normal_max_rawvalue(0); // Unspecified
		sensor.nominal_reading_specified(true);
		sensor.nominal_reading_rawvalue(221); // 2.5 Volts
		sensor.threshold_unr_rawvalue(243); // 2.75 Volts
		sensor.threshold_ucr_rawvalue(237); // 2.6875 Volts
		sensor.threshold_unc_rawvalue(232); // 2.625 Volts
		sensor.threshold_lnc_rawvalue(210); // 2.375 Volts
		sensor.threshold_lcr_rawvalue(204); // 2.3125 Volts
		sensor.threshold_lnr_rawvalue(199); // 2.25 Volts
		sensor.hysteresis_high(0); // +0 Volts
		sensor.hysteresis_low(0); // -0 Volts
		ADD_TO_REPO(sensor);
		if (!ipmc_sensors.get(sensor.sensor_number()))
			ipmc_sensors.add(std::make_shared<ThresholdSensor>(sensor.record_key(), LOG["sensors"]["+2.5VXPT"]));
	}

	{
		SensorDataRecord01 sensor;
		// SDR Calculator.html#precision=4&s-na=%2B3.3VDD&s-no=21&s-t=0x02&s-u-p=4&lnrf=2.97&lcrf=3.0525&lncf=3.135&uncf=3.465&ucrf=3.5475&unrf=3.63&nominalf=3.3&minf=0&granularity=0.0149
		sensor.initialize_blank("+3.3VDD");
		sensor.sensor_owner_id(0); // Tag as "self". This will be auto-calculated in "Get SDR" commands.
		sensor.sensor_owner_channel(0); // See above.
		sensor.sensor_owner_lun(0); // Generally zero
		sensor.sensor_number(21);
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
		sensor.deassertion_upper_threshold_reading_mask(0x7fff); // All events supported & LNR, LCR, LNC, UNC, UCR, UNR deassertions enabled.
		sensor.discrete_reading_setable_threshold_reading_mask(0x3fff); // All thresholds are configurable.
		sensor.units_numeric_format(SensorDataRecord01::UNITS_UNSIGNED);
		sensor.units_rate_unit(SensorDataRecordReadableSensor::RATE_UNIT_NONE);
		sensor.units_base_unit(4); // Volts
		sensor.units_modifier_unit(0); // unspecified
		sensor.units_modifier_unit_method(SensorDataRecordReadableSensor::MODIFIER_UNIT_NONE);
		sensor.linearization(SensorDataRecord01::LIN_LINEAR);
		// IPMI Specifies a linearization function of: y = L[(Mx + (B * 10^(Bexp) ) ) * 10^(Rexp) ]
		// Our settings produce a valid range of 0 (Volts) to 3.7995 (Volts) with 0.0149 Volts granularity.
		sensor.conversion_m(149);
		sensor.conversion_b(0);
		sensor.conversion_b_exp(0);
		sensor.conversion_r_exp(-4);
		sensor.sensor_direction(SensorDataRecordReadableSensor::DIR_UNSPECIFIED);
		//sensor.normal_min_specified(false); // Default
		//sensor.normal_min_rawvalue(0); // Unspecified
		//sensor.normal_max_specified(false); // Default
		//sensor.normal_max_rawvalue(0); // Unspecified
		sensor.nominal_reading_specified(true);
		sensor.nominal_reading_rawvalue(221); // 3.3 Volts
		sensor.threshold_unr_rawvalue(243); // 3.63 Volts
		sensor.threshold_ucr_rawvalue(238); // 3.5475 Volts
		sensor.threshold_unc_rawvalue(232); // 3.465 Volts
		sensor.threshold_lnc_rawvalue(210); // 3.135 Volts
		sensor.threshold_lcr_rawvalue(204); // 3.0525 Volts
		sensor.threshold_lnr_rawvalue(199); // 2.97 Volts
		sensor.hysteresis_high(0); // +0 Volts
		sensor.hysteresis_low(0); // -0 Volts
		ADD_TO_REPO(sensor);
		if (!ipmc_sensors.get(sensor.sensor_number()))
			ipmc_sensors.add(std::make_shared<ThresholdSensor>(sensor.record_key(), LOG["sensors"]["+3.3VDD"]));
	}

	{
		SensorDataRecord01 sensor;
		// SDR Calculator.html#precision=4&s-na=%2B3.3VFFLY1&s-no=22&s-t=0x02&s-u-p=4&lnrf=2.97&lcrf=3.0525&lncf=3.135&uncf=3.465&ucrf=3.5475&unrf=3.63&nominalf=3.3&minf=0&granularity=0.0149
		sensor.initialize_blank("+3.3VFFLY1");
		sensor.sensor_owner_id(0); // Tag as "self". This will be auto-calculated in "Get SDR" commands.
		sensor.sensor_owner_channel(0); // See above.
		sensor.sensor_owner_lun(0); // Generally zero
		sensor.sensor_number(22);
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
		sensor.deassertion_upper_threshold_reading_mask(0x7fff); // All events supported & LNR, LCR, LNC, UNC, UCR, UNR deassertions enabled.
		sensor.discrete_reading_setable_threshold_reading_mask(0x3fff); // All thresholds are configurable.
		sensor.units_numeric_format(SensorDataRecord01::UNITS_UNSIGNED);
		sensor.units_rate_unit(SensorDataRecordReadableSensor::RATE_UNIT_NONE);
		sensor.units_base_unit(4); // Volts
		sensor.units_modifier_unit(0); // unspecified
		sensor.units_modifier_unit_method(SensorDataRecordReadableSensor::MODIFIER_UNIT_NONE);
		sensor.linearization(SensorDataRecord01::LIN_LINEAR);
		// IPMI Specifies a linearization function of: y = L[(Mx + (B * 10^(Bexp) ) ) * 10^(Rexp) ]
		// Our settings produce a valid range of 0 (Volts) to 3.7995 (Volts) with 0.0149 Volts granularity.
		sensor.conversion_m(149);
		sensor.conversion_b(0);
		sensor.conversion_b_exp(0);
		sensor.conversion_r_exp(-4);
		sensor.sensor_direction(SensorDataRecordReadableSensor::DIR_UNSPECIFIED);
		//sensor.normal_min_specified(false); // Default
		//sensor.normal_min_rawvalue(0); // Unspecified
		//sensor.normal_max_specified(false); // Default
		//sensor.normal_max_rawvalue(0); // Unspecified
		sensor.nominal_reading_specified(true);
		sensor.nominal_reading_rawvalue(221); // 3.3 Volts
		sensor.threshold_unr_rawvalue(243); // 3.63 Volts
		sensor.threshold_ucr_rawvalue(238); // 3.5475 Volts
		sensor.threshold_unc_rawvalue(232); // 3.465 Volts
		sensor.threshold_lnc_rawvalue(210); // 3.135 Volts
		sensor.threshold_lcr_rawvalue(204); // 3.0525 Volts
		sensor.threshold_lnr_rawvalue(199); // 2.97 Volts
		sensor.hysteresis_high(0); // +0 Volts
		sensor.hysteresis_low(0); // -0 Volts
		ADD_TO_REPO(sensor);
		if (!ipmc_sensors.get(sensor.sensor_number()))
			ipmc_sensors.add(std::make_shared<ThresholdSensor>(sensor.record_key(), LOG["sensors"]["+3.3VFFLY1"]));
	}

	{
		SensorDataRecord01 sensor;
		// SDR Calculator.html#precision=4&s-na=%2B3.3VFFLY2&s-no=23&s-t=0x02&s-u-p=4&lnrf=2.97&lcrf=3.0525&lncf=3.135&uncf=3.465&ucrf=3.5475&unrf=3.63&nominalf=3.3&minf=0&granularity=0.0149
		sensor.initialize_blank("+3.3VFFLY2");
		sensor.sensor_owner_id(0); // Tag as "self". This will be auto-calculated in "Get SDR" commands.
		sensor.sensor_owner_channel(0); // See above.
		sensor.sensor_owner_lun(0); // Generally zero
		sensor.sensor_number(23);
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
		sensor.deassertion_upper_threshold_reading_mask(0x7fff); // All events supported & LNR, LCR, LNC, UNC, UCR, UNR deassertions enabled.
		sensor.discrete_reading_setable_threshold_reading_mask(0x3fff); // All thresholds are configurable.
		sensor.units_numeric_format(SensorDataRecord01::UNITS_UNSIGNED);
		sensor.units_rate_unit(SensorDataRecordReadableSensor::RATE_UNIT_NONE);
		sensor.units_base_unit(4); // Volts
		sensor.units_modifier_unit(0); // unspecified
		sensor.units_modifier_unit_method(SensorDataRecordReadableSensor::MODIFIER_UNIT_NONE);
		sensor.linearization(SensorDataRecord01::LIN_LINEAR);
		// IPMI Specifies a linearization function of: y = L[(Mx + (B * 10^(Bexp) ) ) * 10^(Rexp) ]
		// Our settings produce a valid range of 0 (Volts) to 3.7995 (Volts) with 0.0149 Volts granularity.
		sensor.conversion_m(149);
		sensor.conversion_b(0);
		sensor.conversion_b_exp(0);
		sensor.conversion_r_exp(-4);
		sensor.sensor_direction(SensorDataRecordReadableSensor::DIR_UNSPECIFIED);
		//sensor.normal_min_specified(false); // Default
		//sensor.normal_min_rawvalue(0); // Unspecified
		//sensor.normal_max_specified(false); // Default
		//sensor.normal_max_rawvalue(0); // Unspecified
		sensor.nominal_reading_specified(true);
		sensor.nominal_reading_rawvalue(221); // 3.3 Volts
		sensor.threshold_unr_rawvalue(243); // 3.63 Volts
		sensor.threshold_ucr_rawvalue(238); // 3.5475 Volts
		sensor.threshold_unc_rawvalue(232); // 3.465 Volts
		sensor.threshold_lnc_rawvalue(210); // 3.135 Volts
		sensor.threshold_lcr_rawvalue(204); // 3.0525 Volts
		sensor.threshold_lnr_rawvalue(199); // 2.97 Volts
		sensor.hysteresis_high(0); // +0 Volts
		sensor.hysteresis_low(0); // -0 Volts
		ADD_TO_REPO(sensor);
		if (!ipmc_sensors.get(sensor.sensor_number()))
			ipmc_sensors.add(std::make_shared<ThresholdSensor>(sensor.record_key(), LOG["sensors"]["+3.3VFFLY2"]));
	}

	{
		SensorDataRecord01 sensor;
		// SDR Calculator.html#precision=4&s-na=%2B3.3VFFLY3&s-no=24&s-t=0x02&s-u-p=4&lnrf=2.97&lcrf=3.0525&lncf=3.135&uncf=3.465&ucrf=3.5475&unrf=3.63&nominalf=3.3&minf=0&granularity=0.0149
		sensor.initialize_blank("+3.3VFFLY3");
		sensor.sensor_owner_id(0); // Tag as "self". This will be auto-calculated in "Get SDR" commands.
		sensor.sensor_owner_channel(0); // See above.
		sensor.sensor_owner_lun(0); // Generally zero
		sensor.sensor_number(24);
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
		sensor.deassertion_upper_threshold_reading_mask(0x7fff); // All events supported & LNR, LCR, LNC, UNC, UCR, UNR deassertions enabled.
		sensor.discrete_reading_setable_threshold_reading_mask(0x3fff); // All thresholds are configurable.
		sensor.units_numeric_format(SensorDataRecord01::UNITS_UNSIGNED);
		sensor.units_rate_unit(SensorDataRecordReadableSensor::RATE_UNIT_NONE);
		sensor.units_base_unit(4); // Volts
		sensor.units_modifier_unit(0); // unspecified
		sensor.units_modifier_unit_method(SensorDataRecordReadableSensor::MODIFIER_UNIT_NONE);
		sensor.linearization(SensorDataRecord01::LIN_LINEAR);
		// IPMI Specifies a linearization function of: y = L[(Mx + (B * 10^(Bexp) ) ) * 10^(Rexp) ]
		// Our settings produce a valid range of 0 (Volts) to 3.7995 (Volts) with 0.0149 Volts granularity.
		sensor.conversion_m(149);
		sensor.conversion_b(0);
		sensor.conversion_b_exp(0);
		sensor.conversion_r_exp(-4);
		sensor.sensor_direction(SensorDataRecordReadableSensor::DIR_UNSPECIFIED);
		//sensor.normal_min_specified(false); // Default
		//sensor.normal_min_rawvalue(0); // Unspecified
		//sensor.normal_max_specified(false); // Default
		//sensor.normal_max_rawvalue(0); // Unspecified
		sensor.nominal_reading_specified(true);
		sensor.nominal_reading_rawvalue(221); // 3.3 Volts
		sensor.threshold_unr_rawvalue(243); // 3.63 Volts
		sensor.threshold_ucr_rawvalue(238); // 3.5475 Volts
		sensor.threshold_unc_rawvalue(232); // 3.465 Volts
		sensor.threshold_lnc_rawvalue(210); // 3.135 Volts
		sensor.threshold_lcr_rawvalue(204); // 3.0525 Volts
		sensor.threshold_lnr_rawvalue(199); // 2.97 Volts
		sensor.hysteresis_high(0); // +0 Volts
		sensor.hysteresis_low(0); // -0 Volts
		ADD_TO_REPO(sensor);
		if (!ipmc_sensors.get(sensor.sensor_number()))
			ipmc_sensors.add(std::make_shared<ThresholdSensor>(sensor.record_key(), LOG["sensors"]["+3.3VFFLY3"]));
	}

	{
		SensorDataRecord01 sensor;
		// SDR Calculator.html#precision=4&s-na=%2B3.3VFFLY4&s-no=25&s-t=0x02&s-u-p=4&lnrf=2.97&lcrf=3.0525&lncf=3.135&uncf=3.465&ucrf=3.5475&unrf=3.63&nominalf=3.3&minf=0&granularity=0.0149
		sensor.initialize_blank("+3.3VFFLY4");
		sensor.sensor_owner_id(0); // Tag as "self". This will be auto-calculated in "Get SDR" commands.
		sensor.sensor_owner_channel(0); // See above.
		sensor.sensor_owner_lun(0); // Generally zero
		sensor.sensor_number(25);
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
		sensor.deassertion_upper_threshold_reading_mask(0x7fff); // All events supported & LNR, LCR, LNC, UNC, UCR, UNR deassertions enabled.
		sensor.discrete_reading_setable_threshold_reading_mask(0x3fff); // All thresholds are configurable.
		sensor.units_numeric_format(SensorDataRecord01::UNITS_UNSIGNED);
		sensor.units_rate_unit(SensorDataRecordReadableSensor::RATE_UNIT_NONE);
		sensor.units_base_unit(4); // Volts
		sensor.units_modifier_unit(0); // unspecified
		sensor.units_modifier_unit_method(SensorDataRecordReadableSensor::MODIFIER_UNIT_NONE);
		sensor.linearization(SensorDataRecord01::LIN_LINEAR);
		// IPMI Specifies a linearization function of: y = L[(Mx + (B * 10^(Bexp) ) ) * 10^(Rexp) ]
		// Our settings produce a valid range of 0 (Volts) to 3.7995 (Volts) with 0.0149 Volts granularity.
		sensor.conversion_m(149);
		sensor.conversion_b(0);
		sensor.conversion_b_exp(0);
		sensor.conversion_r_exp(-4);
		sensor.sensor_direction(SensorDataRecordReadableSensor::DIR_UNSPECIFIED);
		//sensor.normal_min_specified(false); // Default
		//sensor.normal_min_rawvalue(0); // Unspecified
		//sensor.normal_max_specified(false); // Default
		//sensor.normal_max_rawvalue(0); // Unspecified
		sensor.nominal_reading_specified(true);
		sensor.nominal_reading_rawvalue(221); // 3.3 Volts
		sensor.threshold_unr_rawvalue(243); // 3.63 Volts
		sensor.threshold_ucr_rawvalue(238); // 3.5475 Volts
		sensor.threshold_unc_rawvalue(232); // 3.465 Volts
		sensor.threshold_lnc_rawvalue(210); // 3.135 Volts
		sensor.threshold_lcr_rawvalue(204); // 3.0525 Volts
		sensor.threshold_lnr_rawvalue(199); // 2.97 Volts
		sensor.hysteresis_high(0); // +0 Volts
		sensor.hysteresis_low(0); // -0 Volts
		ADD_TO_REPO(sensor);
		if (!ipmc_sensors.get(sensor.sensor_number()))
			ipmc_sensors.add(std::make_shared<ThresholdSensor>(sensor.record_key(), LOG["sensors"]["+3.3VFFLY4"]));
	}

	{
		SensorDataRecord01 sensor;
		// SDR Calculator.html#precision=4&s-na=%2B3.3VFFLY5&s-no=26&s-t=0x02&s-u-p=4&lnrf=2.97&lcrf=3.0525&lncf=3.135&uncf=3.465&ucrf=3.5475&unrf=3.63&nominalf=3.3&minf=0&granularity=0.0149
		sensor.initialize_blank("+3.3VFFLY5");
		sensor.sensor_owner_id(0); // Tag as "self". This will be auto-calculated in "Get SDR" commands.
		sensor.sensor_owner_channel(0); // See above.
		sensor.sensor_owner_lun(0); // Generally zero
		sensor.sensor_number(26);
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
		sensor.deassertion_upper_threshold_reading_mask(0x7fff); // All events supported & LNR, LCR, LNC, UNC, UCR, UNR deassertions enabled.
		sensor.discrete_reading_setable_threshold_reading_mask(0x3fff); // All thresholds are configurable.
		sensor.units_numeric_format(SensorDataRecord01::UNITS_UNSIGNED);
		sensor.units_rate_unit(SensorDataRecordReadableSensor::RATE_UNIT_NONE);
		sensor.units_base_unit(4); // Volts
		sensor.units_modifier_unit(0); // unspecified
		sensor.units_modifier_unit_method(SensorDataRecordReadableSensor::MODIFIER_UNIT_NONE);
		sensor.linearization(SensorDataRecord01::LIN_LINEAR);
		// IPMI Specifies a linearization function of: y = L[(Mx + (B * 10^(Bexp) ) ) * 10^(Rexp) ]
		// Our settings produce a valid range of 0 (Volts) to 3.7995 (Volts) with 0.0149 Volts granularity.
		sensor.conversion_m(149);
		sensor.conversion_b(0);
		sensor.conversion_b_exp(0);
		sensor.conversion_r_exp(-4);
		sensor.sensor_direction(SensorDataRecordReadableSensor::DIR_UNSPECIFIED);
		//sensor.normal_min_specified(false); // Default
		//sensor.normal_min_rawvalue(0); // Unspecified
		//sensor.normal_max_specified(false); // Default
		//sensor.normal_max_rawvalue(0); // Unspecified
		sensor.nominal_reading_specified(true);
		sensor.nominal_reading_rawvalue(221); // 3.3 Volts
		sensor.threshold_unr_rawvalue(243); // 3.63 Volts
		sensor.threshold_ucr_rawvalue(238); // 3.5475 Volts
		sensor.threshold_unc_rawvalue(232); // 3.465 Volts
		sensor.threshold_lnc_rawvalue(210); // 3.135 Volts
		sensor.threshold_lcr_rawvalue(204); // 3.0525 Volts
		sensor.threshold_lnr_rawvalue(199); // 2.97 Volts
		sensor.hysteresis_high(0); // +0 Volts
		sensor.hysteresis_low(0); // -0 Volts
		ADD_TO_REPO(sensor);
		if (!ipmc_sensors.get(sensor.sensor_number()))
			ipmc_sensors.add(std::make_shared<ThresholdSensor>(sensor.record_key(), LOG["sensors"]["+3.3VFFLY5"]));
	}

	{
		SensorDataRecord01 sensor;
		// SDR Calculator.html#precision=4&s-na=%2B3.3VMP2&s-no=27&s-t=0x02&s-u-p=4&lnrf=2.97&lcrf=3.0525&lncf=3.135&uncf=3.465&ucrf=3.5475&unrf=3.63&nominalf=3.3&minf=0&granularity=0.0149
		sensor.initialize_blank("+3.3VMP2");
		sensor.sensor_owner_id(0); // Tag as "self". This will be auto-calculated in "Get SDR" commands.
		sensor.sensor_owner_channel(0); // See above.
		sensor.sensor_owner_lun(0); // Generally zero
		sensor.sensor_number(27);
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
		sensor.deassertion_upper_threshold_reading_mask(0x7fff); // All events supported & LNR, LCR, LNC, UNC, UCR, UNR deassertions enabled.
		sensor.discrete_reading_setable_threshold_reading_mask(0x3fff); // All thresholds are configurable.
		sensor.units_numeric_format(SensorDataRecord01::UNITS_UNSIGNED);
		sensor.units_rate_unit(SensorDataRecordReadableSensor::RATE_UNIT_NONE);
		sensor.units_base_unit(4); // Volts
		sensor.units_modifier_unit(0); // unspecified
		sensor.units_modifier_unit_method(SensorDataRecordReadableSensor::MODIFIER_UNIT_NONE);
		sensor.linearization(SensorDataRecord01::LIN_LINEAR);
		// IPMI Specifies a linearization function of: y = L[(Mx + (B * 10^(Bexp) ) ) * 10^(Rexp) ]
		// Our settings produce a valid range of 0 (Volts) to 3.7995 (Volts) with 0.0149 Volts granularity.
		sensor.conversion_m(149);
		sensor.conversion_b(0);
		sensor.conversion_b_exp(0);
		sensor.conversion_r_exp(-4);
		sensor.sensor_direction(SensorDataRecordReadableSensor::DIR_UNSPECIFIED);
		//sensor.normal_min_specified(false); // Default
		//sensor.normal_min_rawvalue(0); // Unspecified
		//sensor.normal_max_specified(false); // Default
		//sensor.normal_max_rawvalue(0); // Unspecified
		sensor.nominal_reading_specified(true);
		sensor.nominal_reading_rawvalue(221); // 3.3 Volts
		sensor.threshold_unr_rawvalue(243); // 3.63 Volts
		sensor.threshold_ucr_rawvalue(238); // 3.5475 Volts
		sensor.threshold_unc_rawvalue(232); // 3.465 Volts
		sensor.threshold_lnc_rawvalue(210); // 3.135 Volts
		sensor.threshold_lcr_rawvalue(204); // 3.0525 Volts
		sensor.threshold_lnr_rawvalue(199); // 2.97 Volts
		sensor.hysteresis_high(0); // +0 Volts
		sensor.hysteresis_low(0); // -0 Volts
		ADD_TO_REPO(sensor);
		if (!ipmc_sensors.get(sensor.sensor_number()))
			ipmc_sensors.add(std::make_shared<ThresholdSensor>(sensor.record_key(), LOG["sensors"]["+3.3VMP2"]));
	}

	{
		SensorDataRecord01 sensor;
		// SDR Calculator.html#precision=4&s-na=%2B3.55VBULK&s-no=28&s-t=0x02&s-u-p=4&lnrf=3.195&lcrf=3.2837&lncf=3.3725&uncf=3.7275&ucrf=3.8163&unrf=3.905&nominalf=3.55&minf=0&granularity=0.0161
		sensor.initialize_blank("+3.55VBULK");
		sensor.sensor_owner_id(0); // Tag as "self". This will be auto-calculated in "Get SDR" commands.
		sensor.sensor_owner_channel(0); // See above.
		sensor.sensor_owner_lun(0); // Generally zero
		sensor.sensor_number(28);
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
		sensor.deassertion_upper_threshold_reading_mask(0x7fff); // All events supported & LNR, LCR, LNC, UNC, UCR, UNR deassertions enabled.
		sensor.discrete_reading_setable_threshold_reading_mask(0x3fff); // All thresholds are configurable.
		sensor.units_numeric_format(SensorDataRecord01::UNITS_UNSIGNED);
		sensor.units_rate_unit(SensorDataRecordReadableSensor::RATE_UNIT_NONE);
		sensor.units_base_unit(4); // Volts
		sensor.units_modifier_unit(0); // unspecified
		sensor.units_modifier_unit_method(SensorDataRecordReadableSensor::MODIFIER_UNIT_NONE);
		sensor.linearization(SensorDataRecord01::LIN_LINEAR);
		// IPMI Specifies a linearization function of: y = L[(Mx + (B * 10^(Bexp) ) ) * 10^(Rexp) ]
		// Our settings produce a valid range of 0 (Volts) to 4.1055 (Volts) with 0.0161 Volts granularity.
		sensor.conversion_m(161);
		sensor.conversion_b(0);
		sensor.conversion_b_exp(0);
		sensor.conversion_r_exp(-4);
		sensor.sensor_direction(SensorDataRecordReadableSensor::DIR_UNSPECIFIED);
		//sensor.normal_min_specified(false); // Default
		//sensor.normal_min_rawvalue(0); // Unspecified
		//sensor.normal_max_specified(false); // Default
		//sensor.normal_max_rawvalue(0); // Unspecified
		sensor.nominal_reading_specified(true);
		sensor.nominal_reading_rawvalue(220); // 3.55 Volts
		sensor.threshold_unr_rawvalue(242); // 3.905 Volts
		sensor.threshold_ucr_rawvalue(237); // 3.8163 Volts
		sensor.threshold_unc_rawvalue(231); // 3.7275 Volts
		sensor.threshold_lnc_rawvalue(209); // 3.3725 Volts
		sensor.threshold_lcr_rawvalue(203); // 3.2837 Volts
		sensor.threshold_lnr_rawvalue(198); // 3.195 Volts
		sensor.hysteresis_high(0); // +0 Volts
		sensor.hysteresis_low(0); // -0 Volts
		ADD_TO_REPO(sensor);
		if (!ipmc_sensors.get(sensor.sensor_number()))
			ipmc_sensors.add(std::make_shared<ThresholdSensor>(sensor.record_key(), LOG["sensors"]["+3.55VBULK"]));
	}

	{
		SensorDataRecord01 sensor;
		// SDR Calculator.html#precision=4&s-na=%2B5VPYLD&s-no=29&s-t=0x02&s-u-p=4&lnrf=4.5&lcrf=4.625&lncf=4.75&uncf=5.25&ucrf=5.375&unrf=5.5&nominalf=5&minf=0&granularity=0.0226
		sensor.initialize_blank("+5VPYLD");
		sensor.sensor_owner_id(0); // Tag as "self". This will be auto-calculated in "Get SDR" commands.
		sensor.sensor_owner_channel(0); // See above.
		sensor.sensor_owner_lun(0); // Generally zero
		sensor.sensor_number(29);
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
		sensor.deassertion_upper_threshold_reading_mask(0x7fff); // All events supported & LNR, LCR, LNC, UNC, UCR, UNR deassertions enabled.
		sensor.discrete_reading_setable_threshold_reading_mask(0x3fff); // All thresholds are configurable.
		sensor.units_numeric_format(SensorDataRecord01::UNITS_UNSIGNED);
		sensor.units_rate_unit(SensorDataRecordReadableSensor::RATE_UNIT_NONE);
		sensor.units_base_unit(4); // Volts
		sensor.units_modifier_unit(0); // unspecified
		sensor.units_modifier_unit_method(SensorDataRecordReadableSensor::MODIFIER_UNIT_NONE);
		sensor.linearization(SensorDataRecord01::LIN_LINEAR);
		// IPMI Specifies a linearization function of: y = L[(Mx + (B * 10^(Bexp) ) ) * 10^(Rexp) ]
		// Our settings produce a valid range of 0 (Volts) to 5.763 (Volts) with 0.0226 Volts granularity.
		sensor.conversion_m(226);
		sensor.conversion_b(0);
		sensor.conversion_b_exp(0);
		sensor.conversion_r_exp(-4);
		sensor.sensor_direction(SensorDataRecordReadableSensor::DIR_UNSPECIFIED);
		//sensor.normal_min_specified(false); // Default
		//sensor.normal_min_rawvalue(0); // Unspecified
		//sensor.normal_max_specified(false); // Default
		//sensor.normal_max_rawvalue(0); // Unspecified
		sensor.nominal_reading_specified(true);
		sensor.nominal_reading_rawvalue(221); // 5 Volts
		sensor.threshold_unr_rawvalue(243); // 5.5 Volts
		sensor.threshold_ucr_rawvalue(237); // 5.375 Volts
		sensor.threshold_unc_rawvalue(232); // 5.25 Volts
		sensor.threshold_lnc_rawvalue(210); // 4.75 Volts
		sensor.threshold_lcr_rawvalue(204); // 4.625 Volts
		sensor.threshold_lnr_rawvalue(199); // 4.5 Volts
		sensor.hysteresis_high(0); // +0 Volts
		sensor.hysteresis_low(0); // -0 Volts
		ADD_TO_REPO(sensor);
		if (!ipmc_sensors.get(sensor.sensor_number()))
			ipmc_sensors.add(std::make_shared<ThresholdSensor>(sensor.record_key(), LOG["sensors"]["+5VPYLD"]));
	}

	{
		SensorDataRecord01 sensor;
		// SDR Calculator.html#precision=4&s-na=MGT0.9VB_IMON&s-no=30&s-t=0x03&s-u-p=5&minf=0&maxf=25.5
		sensor.initialize_blank("MGT0.9VB_IMON");
		sensor.sensor_owner_id(0); // Tag as "self". This will be auto-calculated in "Get SDR" commands.
		sensor.sensor_owner_channel(0); // See above.
		sensor.sensor_owner_lun(0); // Generally zero
		sensor.sensor_number(30);
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
		sensor.sensor_type_code(0x03); // Current
		sensor.event_type_reading_code(SensorDataRecordReadableSensor::EVENT_TYPE_THRESHOLD_SENSOR);
		sensor.assertion_lower_threshold_reading_mask(0x7000); // All events supported & no threshold assertions enabled.
		sensor.deassertion_upper_threshold_reading_mask(0x7000); // All events supported & no threshold deassertions enabled.
		sensor.discrete_reading_setable_threshold_reading_mask(0x3fff); // All thresholds are configurable.
		sensor.units_numeric_format(SensorDataRecord01::UNITS_UNSIGNED);
		sensor.units_rate_unit(SensorDataRecordReadableSensor::RATE_UNIT_NONE);
		sensor.units_base_unit(5); // Amps
		sensor.units_modifier_unit(0); // unspecified
		sensor.units_modifier_unit_method(SensorDataRecordReadableSensor::MODIFIER_UNIT_NONE);
		sensor.linearization(SensorDataRecord01::LIN_LINEAR);
		// IPMI Specifies a linearization function of: y = L[(Mx + (B * 10^(Bexp) ) ) * 10^(Rexp) ]
		// Our settings produce a valid range of 0 (Amps) to 25.5 (Amps) with 0.1 Amps granularity.
		sensor.conversion_m(1);
		sensor.conversion_b(0);
		sensor.conversion_b_exp(0);
		sensor.conversion_r_exp(-1);
		sensor.sensor_direction(SensorDataRecordReadableSensor::DIR_UNSPECIFIED);
		//sensor.normal_min_specified(false); // Default
		//sensor.normal_min_rawvalue(0); // Unspecified
		//sensor.normal_max_specified(false); // Default
		//sensor.normal_max_rawvalue(0); // Unspecified
		//sensor.nominal_reading_specified(true);
		//sensor.nominal_reading_rawvalue(0); // null Amps
		sensor.threshold_unr_rawvalue(255); // null Amps
		sensor.threshold_ucr_rawvalue(255); // null Amps
		sensor.threshold_unc_rawvalue(255); // null Amps
		sensor.threshold_lnc_rawvalue(0); // null Amps
		sensor.threshold_lcr_rawvalue(0); // null Amps
		sensor.threshold_lnr_rawvalue(0); // null Amps
		sensor.hysteresis_high(0); // +0 Amps
		sensor.hysteresis_low(0); // -0 Amps
		ADD_TO_REPO(sensor);
		if (!ipmc_sensors.get(sensor.sensor_number()))
			ipmc_sensors.add(std::make_shared<ThresholdSensor>(sensor.record_key(), LOG["sensors"]["MGT0.9VB_IMON"]));
	}

	{
		SensorDataRecord01 sensor;
		// SDR Calculator.html#precision=4&s-na=MGT0.9VT_IMON&s-no=31&s-t=0x03&s-u-p=5&minf=0&maxf=25.5
		sensor.initialize_blank("MGT0.9VT_IMON");
		sensor.sensor_owner_id(0); // Tag as "self". This will be auto-calculated in "Get SDR" commands.
		sensor.sensor_owner_channel(0); // See above.
		sensor.sensor_owner_lun(0); // Generally zero
		sensor.sensor_number(31);
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
		sensor.sensor_type_code(0x03); // Current
		sensor.event_type_reading_code(SensorDataRecordReadableSensor::EVENT_TYPE_THRESHOLD_SENSOR);
		sensor.assertion_lower_threshold_reading_mask(0x7000); // All events supported & no threshold assertions enabled.
		sensor.deassertion_upper_threshold_reading_mask(0x7000); // All events supported & no threshold deassertions enabled.
		sensor.discrete_reading_setable_threshold_reading_mask(0x3fff); // All thresholds are configurable.
		sensor.units_numeric_format(SensorDataRecord01::UNITS_UNSIGNED);
		sensor.units_rate_unit(SensorDataRecordReadableSensor::RATE_UNIT_NONE);
		sensor.units_base_unit(5); // Amps
		sensor.units_modifier_unit(0); // unspecified
		sensor.units_modifier_unit_method(SensorDataRecordReadableSensor::MODIFIER_UNIT_NONE);
		sensor.linearization(SensorDataRecord01::LIN_LINEAR);
		// IPMI Specifies a linearization function of: y = L[(Mx + (B * 10^(Bexp) ) ) * 10^(Rexp) ]
		// Our settings produce a valid range of 0 (Amps) to 25.5 (Amps) with 0.1 Amps granularity.
		sensor.conversion_m(1);
		sensor.conversion_b(0);
		sensor.conversion_b_exp(0);
		sensor.conversion_r_exp(-1);
		sensor.sensor_direction(SensorDataRecordReadableSensor::DIR_UNSPECIFIED);
		//sensor.normal_min_specified(false); // Default
		//sensor.normal_min_rawvalue(0); // Unspecified
		//sensor.normal_max_specified(false); // Default
		//sensor.normal_max_rawvalue(0); // Unspecified
		//sensor.nominal_reading_specified(true);
		//sensor.nominal_reading_rawvalue(0); // null Amps
		sensor.threshold_unr_rawvalue(255); // null Amps
		sensor.threshold_ucr_rawvalue(255); // null Amps
		sensor.threshold_unc_rawvalue(255); // null Amps
		sensor.threshold_lnc_rawvalue(0); // null Amps
		sensor.threshold_lcr_rawvalue(0); // null Amps
		sensor.threshold_lnr_rawvalue(0); // null Amps
		sensor.hysteresis_high(0); // +0 Amps
		sensor.hysteresis_low(0); // -0 Amps
		ADD_TO_REPO(sensor);
		if (!ipmc_sensors.get(sensor.sensor_number()))
			ipmc_sensors.add(std::make_shared<ThresholdSensor>(sensor.record_key(), LOG["sensors"]["MGT0.9VT_IMON"]));
	}

	{
		SensorDataRecord01 sensor;
		// SDR Calculator.html#precision=4&s-na=MGT1.2VB_IMON&s-no=32&s-t=0x03&s-u-p=5&minf=0&maxf=30.6
		sensor.initialize_blank("MGT1.2VB_IMON");
		sensor.sensor_owner_id(0); // Tag as "self". This will be auto-calculated in "Get SDR" commands.
		sensor.sensor_owner_channel(0); // See above.
		sensor.sensor_owner_lun(0); // Generally zero
		sensor.sensor_number(32);
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
		sensor.sensor_type_code(0x03); // Current
		sensor.event_type_reading_code(SensorDataRecordReadableSensor::EVENT_TYPE_THRESHOLD_SENSOR);
		sensor.assertion_lower_threshold_reading_mask(0x7000); // All events supported & no threshold assertions enabled.
		sensor.deassertion_upper_threshold_reading_mask(0x7000); // All events supported & no threshold deassertions enabled.
		sensor.discrete_reading_setable_threshold_reading_mask(0x3fff); // All thresholds are configurable.
		sensor.units_numeric_format(SensorDataRecord01::UNITS_UNSIGNED);
		sensor.units_rate_unit(SensorDataRecordReadableSensor::RATE_UNIT_NONE);
		sensor.units_base_unit(5); // Amps
		sensor.units_modifier_unit(0); // unspecified
		sensor.units_modifier_unit_method(SensorDataRecordReadableSensor::MODIFIER_UNIT_NONE);
		sensor.linearization(SensorDataRecord01::LIN_LINEAR);
		// IPMI Specifies a linearization function of: y = L[(Mx + (B * 10^(Bexp) ) ) * 10^(Rexp) ]
		// Our settings produce a valid range of 0 (Amps) to 30.6 (Amps) with 0.12 Amps granularity.
		sensor.conversion_m(12);
		sensor.conversion_b(0);
		sensor.conversion_b_exp(0);
		sensor.conversion_r_exp(-2);
		sensor.sensor_direction(SensorDataRecordReadableSensor::DIR_UNSPECIFIED);
		//sensor.normal_min_specified(false); // Default
		//sensor.normal_min_rawvalue(0); // Unspecified
		//sensor.normal_max_specified(false); // Default
		//sensor.normal_max_rawvalue(0); // Unspecified
		//sensor.nominal_reading_specified(true);
		//sensor.nominal_reading_rawvalue(0); // null Amps
		sensor.threshold_unr_rawvalue(255); // null Amps
		sensor.threshold_ucr_rawvalue(255); // null Amps
		sensor.threshold_unc_rawvalue(255); // null Amps
		sensor.threshold_lnc_rawvalue(0); // null Amps
		sensor.threshold_lcr_rawvalue(0); // null Amps
		sensor.threshold_lnr_rawvalue(0); // null Amps
		sensor.hysteresis_high(0); // +0 Amps
		sensor.hysteresis_low(0); // -0 Amps
		ADD_TO_REPO(sensor);
		if (!ipmc_sensors.get(sensor.sensor_number()))
			ipmc_sensors.add(std::make_shared<ThresholdSensor>(sensor.record_key(), LOG["sensors"]["MGT1.2VB_IMON"]));
	}

	{
		SensorDataRecord01 sensor;
		// SDR Calculator.html#precision=4&s-na=MGT1.2VT_IMON&s-no=33&s-t=0x03&s-u-p=5&minf=0&maxf=30.6
		sensor.initialize_blank("MGT1.2VT_IMON");
		sensor.sensor_owner_id(0); // Tag as "self". This will be auto-calculated in "Get SDR" commands.
		sensor.sensor_owner_channel(0); // See above.
		sensor.sensor_owner_lun(0); // Generally zero
		sensor.sensor_number(33);
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
		sensor.sensor_type_code(0x03); // Current
		sensor.event_type_reading_code(SensorDataRecordReadableSensor::EVENT_TYPE_THRESHOLD_SENSOR);
		sensor.assertion_lower_threshold_reading_mask(0x7000); // All events supported & no threshold assertions enabled.
		sensor.deassertion_upper_threshold_reading_mask(0x7000); // All events supported & no threshold deassertions enabled.
		sensor.discrete_reading_setable_threshold_reading_mask(0x3fff); // All thresholds are configurable.
		sensor.units_numeric_format(SensorDataRecord01::UNITS_UNSIGNED);
		sensor.units_rate_unit(SensorDataRecordReadableSensor::RATE_UNIT_NONE);
		sensor.units_base_unit(5); // Amps
		sensor.units_modifier_unit(0); // unspecified
		sensor.units_modifier_unit_method(SensorDataRecordReadableSensor::MODIFIER_UNIT_NONE);
		sensor.linearization(SensorDataRecord01::LIN_LINEAR);
		// IPMI Specifies a linearization function of: y = L[(Mx + (B * 10^(Bexp) ) ) * 10^(Rexp) ]
		// Our settings produce a valid range of 0 (Amps) to 30.6 (Amps) with 0.12 Amps granularity.
		sensor.conversion_m(12);
		sensor.conversion_b(0);
		sensor.conversion_b_exp(0);
		sensor.conversion_r_exp(-2);
		sensor.sensor_direction(SensorDataRecordReadableSensor::DIR_UNSPECIFIED);
		//sensor.normal_min_specified(false); // Default
		//sensor.normal_min_rawvalue(0); // Unspecified
		//sensor.normal_max_specified(false); // Default
		//sensor.normal_max_rawvalue(0); // Unspecified
		//sensor.nominal_reading_specified(true);
		//sensor.nominal_reading_rawvalue(0); // null Amps
		sensor.threshold_unr_rawvalue(255); // null Amps
		sensor.threshold_ucr_rawvalue(255); // null Amps
		sensor.threshold_unc_rawvalue(255); // null Amps
		sensor.threshold_lnc_rawvalue(0); // null Amps
		sensor.threshold_lcr_rawvalue(0); // null Amps
		sensor.threshold_lnr_rawvalue(0); // null Amps
		sensor.hysteresis_high(0); // +0 Amps
		sensor.hysteresis_low(0); // -0 Amps
		ADD_TO_REPO(sensor);
		if (!ipmc_sensors.get(sensor.sensor_number()))
			ipmc_sensors.add(std::make_shared<ThresholdSensor>(sensor.record_key(), LOG["sensors"]["MGT1.2VT_IMON"]));
	}

	{
		SensorDataRecord01 sensor;
		// SDR Calculator.html#precision=4&s-na=T_BOARD1&s-no=34&s-t=0x01&s-u-p=1&uncf=40&ucrf=45&unrf=50&nominalf=30&minf=0&granularity=0.5
		sensor.initialize_blank("T_BOARD1");
		sensor.sensor_owner_id(0); // Tag as "self". This will be auto-calculated in "Get SDR" commands.
		sensor.sensor_owner_channel(0); // See above.
		sensor.sensor_owner_lun(0); // Generally zero
		sensor.sensor_number(34);
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
		sensor.sensor_type_code(0x01); // Temperature
		sensor.event_type_reading_code(SensorDataRecordReadableSensor::EVENT_TYPE_THRESHOLD_SENSOR);
		sensor.assertion_lower_threshold_reading_mask(0x7fc0); // All events supported & UNC, UCR, UNR assertions enabled.
		sensor.deassertion_upper_threshold_reading_mask(0x7fc0); // All events supported & UNC, UCR, UNR deassertions enabled.
		sensor.discrete_reading_setable_threshold_reading_mask(0x3fff); // All thresholds are configurable.
		sensor.units_numeric_format(SensorDataRecord01::UNITS_UNSIGNED);
		sensor.units_rate_unit(SensorDataRecordReadableSensor::RATE_UNIT_NONE);
		sensor.units_base_unit(1); // degrees C
		sensor.units_modifier_unit(0); // unspecified
		sensor.units_modifier_unit_method(SensorDataRecordReadableSensor::MODIFIER_UNIT_NONE);
		sensor.linearization(SensorDataRecord01::LIN_LINEAR);
		// IPMI Specifies a linearization function of: y = L[(Mx + (B * 10^(Bexp) ) ) * 10^(Rexp) ]
		// Our settings produce a valid range of 0 (degrees C) to 127.5 (degrees C) with 0.5 degrees C granularity.
		sensor.conversion_m(5);
		sensor.conversion_b(0);
		sensor.conversion_b_exp(0);
		sensor.conversion_r_exp(-1);
		sensor.sensor_direction(SensorDataRecordReadableSensor::DIR_UNSPECIFIED);
		//sensor.normal_min_specified(false); // Default
		//sensor.normal_min_rawvalue(0); // Unspecified
		//sensor.normal_max_specified(false); // Default
		//sensor.normal_max_rawvalue(0); // Unspecified
		sensor.nominal_reading_specified(true);
		sensor.nominal_reading_rawvalue(60); // 30 degrees C
		sensor.threshold_unr_rawvalue(100); // 50 degrees C
		sensor.threshold_ucr_rawvalue(90); // 45 degrees C
		sensor.threshold_unc_rawvalue(80); // 40 degrees C
		sensor.threshold_lnc_rawvalue(0); // null degrees C
		sensor.threshold_lcr_rawvalue(0); // null degrees C
		sensor.threshold_lnr_rawvalue(0); // null degrees C
		sensor.hysteresis_high(0); // +0 degrees C
		sensor.hysteresis_low(0); // -0 degrees C
		ADD_TO_REPO(sensor);
		if (!ipmc_sensors.get(sensor.sensor_number()))
			ipmc_sensors.add(std::make_shared<ThresholdSensor>(sensor.record_key(), LOG["sensors"]["T_BOARD1"]));
	}

	{
		SensorDataRecord01 sensor;
		// SDR Calculator.html#precision=4&s-na=T_BOARD2&s-no=35&s-t=0x01&s-u-p=1&uncf=40&ucrf=45&unrf=50&nominalf=30&minf=0&granularity=0.5
		sensor.initialize_blank("T_BOARD2");
		sensor.sensor_owner_id(0); // Tag as "self". This will be auto-calculated in "Get SDR" commands.
		sensor.sensor_owner_channel(0); // See above.
		sensor.sensor_owner_lun(0); // Generally zero
		sensor.sensor_number(35);
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
		sensor.sensor_type_code(0x01); // Temperature
		sensor.event_type_reading_code(SensorDataRecordReadableSensor::EVENT_TYPE_THRESHOLD_SENSOR);
		sensor.assertion_lower_threshold_reading_mask(0x7fc0); // All events supported & UNC, UCR, UNR assertions enabled.
		sensor.deassertion_upper_threshold_reading_mask(0x7fc0); // All events supported & UNC, UCR, UNR deassertions enabled.
		sensor.discrete_reading_setable_threshold_reading_mask(0x3fff); // All thresholds are configurable.
		sensor.units_numeric_format(SensorDataRecord01::UNITS_UNSIGNED);
		sensor.units_rate_unit(SensorDataRecordReadableSensor::RATE_UNIT_NONE);
		sensor.units_base_unit(1); // degrees C
		sensor.units_modifier_unit(0); // unspecified
		sensor.units_modifier_unit_method(SensorDataRecordReadableSensor::MODIFIER_UNIT_NONE);
		sensor.linearization(SensorDataRecord01::LIN_LINEAR);
		// IPMI Specifies a linearization function of: y = L[(Mx + (B * 10^(Bexp) ) ) * 10^(Rexp) ]
		// Our settings produce a valid range of 0 (degrees C) to 127.5 (degrees C) with 0.5 degrees C granularity.
		sensor.conversion_m(5);
		sensor.conversion_b(0);
		sensor.conversion_b_exp(0);
		sensor.conversion_r_exp(-1);
		sensor.sensor_direction(SensorDataRecordReadableSensor::DIR_UNSPECIFIED);
		//sensor.normal_min_specified(false); // Default
		//sensor.normal_min_rawvalue(0); // Unspecified
		//sensor.normal_max_specified(false); // Default
		//sensor.normal_max_rawvalue(0); // Unspecified
		sensor.nominal_reading_specified(true);
		sensor.nominal_reading_rawvalue(60); // 30 degrees C
		sensor.threshold_unr_rawvalue(100); // 50 degrees C
		sensor.threshold_ucr_rawvalue(90); // 45 degrees C
		sensor.threshold_unc_rawvalue(80); // 40 degrees C
		sensor.threshold_lnc_rawvalue(0); // null degrees C
		sensor.threshold_lcr_rawvalue(0); // null degrees C
		sensor.threshold_lnr_rawvalue(0); // null degrees C
		sensor.hysteresis_high(0); // +0 degrees C
		sensor.hysteresis_low(0); // -0 degrees C
		ADD_TO_REPO(sensor);
		if (!ipmc_sensors.get(sensor.sensor_number()))
			ipmc_sensors.add(std::make_shared<ThresholdSensor>(sensor.record_key(), LOG["sensors"]["T_BOARD2"]));
	}

#undef ADD_TO_REPO

	UWTaskCreate("persist_sdr", TASK_PRIORITY_SERVICE, [reinit]() -> void {
		VariablePersistentAllocation sdr_persist(*persistent_storage, PersistentStorageAllocations::WISC_SDR_REPOSITORY);
		// If not reinitializing, merge in saved configuration, overwriting matching records.
		if (!reinit) {
			device_sdr_repo.u8import(sdr_persist.get_data());
			// Now that we've merged in our stored settings, we'll have to update the linkages (and sensor processor settings) again.
			payload_manager->refresh_sensor_linkage();
		}

		// Store the newly initialized Device SDRs
		sdr_persist.set_data(device_sdr_repo.u8export());

		// I think these needta be imported to the main SDR repo too?
		sdr_repo.add(device_sdr_repo, 0);
	});
}

/**
 * Generate the appropriate headers (up to and excluding Record Format Version)
 * and add the PICMG multirecord to the provided FRU Data vector.
 *
 * @param fruarea The FRU Data area to be appended
 * @param mrdata The multirecord to be added
 * @param last_record true if the "end of list" flag should be set on this record, else false
 * @param record_format The record format version, if not default
 */
static void add_PICMG_multirecord (std::vector<uint8_t> &fruarea, std::vector<uint8_t> mrdata, bool last_record, uint8_t record_format=2) {
	static const std::vector<uint8_t> mrheader{
		0xC0, // "OEM", specified
		0x00, // [7] 1b=EOL (set later);  [3:0] record_format (set later)
		0, // Length (placeholder)
		0, // Record checksum (placeholder)
		0, // Header checksum (placeholder)
		0x5A, // Mfgr: PICMG, specified
		0x31, // Mfgr: PICMG, specified
		0x00, // Mfgr: PICMG, specified
	};
	mrdata.insert(mrdata.begin(), mrheader.begin(), mrheader.end());
	mrdata[1] = (last_record ? 0x80 : 0) | record_format;
	mrdata[2] = mrdata.size() - 5 /* Apparently this is record DATA length. */;
	mrdata[3] = ipmi_checksum(std::vector<uint8_t>(std::next(mrdata.begin(), 5), mrdata.end()));
	mrdata[4] = ipmi_checksum(std::vector<uint8_t>(mrdata.begin(), std::next(mrdata.begin(), 5)));
	fruarea.insert(fruarea.end(), mrdata.begin(), mrdata.end());
}

void init_fru_data(bool reinit) {
	std::vector<uint8_t> tlstring;

	std::vector<uint8_t> board_info;
	board_info.push_back(0x01); // Format Version
	board_info.push_back(0x00); // Length Placeholder
	board_info.push_back(25);   // Language Code (English)
	board_info.push_back(0x00); // Mfg Date/Time (Unspecified)
	board_info.push_back(0x00); // Mfg Date/Time (Unspecified)
	board_info.push_back(0x00); // Mfg Date/Time (Unspecified)
	tlstring = encode_ipmi_type_length_field("University of Wisconsin");
	board_info.insert(board_info.end(), tlstring.begin(), tlstring.end()); // Board Mfgr.
	tlstring = encode_ipmi_type_length_field("ZYNQ IPMC");
	board_info.insert(board_info.end(), tlstring.begin(), tlstring.end()); // Board Product Name
	tlstring = encode_ipmi_type_length_field(std::to_string(IPMC_SERIAL));
	board_info.insert(board_info.end(), tlstring.begin(), tlstring.end()); // Board Serial
	tlstring = encode_ipmi_type_length_field(std::string("IPMC Rev") + std::to_string(IPMC_HW_REVISION));
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
	tlstring = encode_ipmi_type_length_field(std::string("IPMC Rev") + std::to_string(IPMC_HW_REVISION));
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


	fru_data.resize(8);
	fru_data[0] = 0x01; // Common Header Format Version
	fru_data[1] = 0x00; // Internal Use Area Offset (multiple of 8 bytes)
	fru_data[2] = 0x00; // Chassis Info Area Offset (multiple of 8 bytes)
	fru_data[3] = 0x01; // Board Area Offset (multiple of 8 bytes)
	fru_data[4] = 0x01 + board_info.size()/8; // Product Info Area Offset (multiple of 8 bytes)
	fru_data[5] = 0x01 + board_info.size()/8 + product_info.size()/8; // Multi-Record Area Offset (multiple of 8 bytes)
	fru_data[6] = 0x00; // PAD, write as 00h
	fru_data[7] = 0x00;
	fru_data[7] = ipmi_checksum(fru_data); // Checksum

	fru_data.insert(fru_data.end(), board_info.begin(), board_info.end());
	fru_data.insert(fru_data.end(), product_info.begin(), product_info.end());

	/* Board Point-to-Point Connectivity Record
	 *
	 * This block of code will generate this automatically based on the E-Keying
	 * link descriptors provided by the payload manager.  It does not need to be
	 * customized by a board integrator.  E-Keying link descriptions should be
	 * defined in the Payload Manager instead.
	 */
	{
		std::vector<uint8_t> bp2pcr{0x14, 0, 0};
		for (uint8_t i = 0xF0; i < 0xFF; ++i) {
			try {
				std::vector<uint8_t> guid = PayloadManager::LinkDescriptor::lookup_oem_LinkType_guid(i);
				bp2pcr[2]++; // Increment number of GUIDs in record.
				bp2pcr.insert(bp2pcr.end(), guid.begin(), guid.end()); // Add GUID to record.
			}
			catch (std::out_of_range) {
				break;
			}
		}
		configASSERT(payload_manager); // We should be called only after this is initialized.
		std::vector<PayloadManager::LinkDescriptor> links = payload_manager->get_links();
		for (auto it = links.begin(), eit = links.end(); it != eit; ++it) {
			if (bp2pcr.size() > 255 /* record data limit */ - 3 /* MultiRec OEM ID header */ - 4 /* next link descriptor size */ - 1 /* safety margin */) {
				// We filled up this record we need to start a new one.
				add_PICMG_multirecord(fru_data, bp2pcr, false);
				// Start new record, zero GUIDs in further records, they all fit in one (barely).
				bp2pcr = std::vector<uint8_t>{0x14, 0, 0};
			}
			std::vector<uint8_t> ld_vec(*it);
			bp2pcr.insert(bp2pcr.end(), ld_vec.begin(), ld_vec.end());
		}
		// We have at least one link or at least one GUID, or just need to say we have none.
		add_PICMG_multirecord(fru_data, bp2pcr, false /* We are not the last record in the FRU Data. */);
	}

	/* Carrier Activation and Current Management record
	 * ...not that we have any AMC modules.
	 *
	 * This is supposed to specify the maximum power we can provide to our AMCs,
	 * and be used for validating our AMC modules' power requirements.
	 */
	add_PICMG_multirecord(fru_data, std::vector<uint8_t>{0x17, 0, 0x3f /* ~75W for all AMCs (and self..?) LSB */, 0 /* MSB */, 5, 0}, true);

	UWTaskCreate("persist_fru", TASK_PRIORITY_SERVICE, [reinit]() -> void {
		safe_init_static_mutex(fru_data_mutex, false);
		MutexGuard<false> lock(fru_data_mutex, true);
		VariablePersistentAllocation fru_persist(*persistent_storage, PersistentStorageAllocations::WISC_FRU_DATA);

		// If not reinitializing, and there's an area to read, replace ours, else write.
		std::vector<uint8_t> persist_data = fru_persist.get_data();
		if (persist_data.size() && !reinit)
			fru_data = persist_data;

		// Store the newly initialized Device SDRs
		fru_persist.set_data(fru_data);
		std::string out;
#if 0
		for (auto it = fru_data.begin(), eit = fru_data.end(); it != eit; ++it)
			out += stdsprintf("%02hhx", *it);
		printf("FRU Data \"%s\"\n", out.c_str());
#endif
	});
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
	bannerstr += std::string("HW revision : ") + std::to_string(IPMC_HW_REVISION) + "\n"; // TODO
	bannerstr += std::string("SW revision : ") + GIT_DESCRIBE + " (" + GIT_BRANCH + ")\n";
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

#include "core_console_commands/date.h"
#include "core_console_commands/flash_info.h"
#include "core_console_commands/ps.h"
#include "core_console_commands/restart.h"
#include "core_console_commands/setauth.h"
#include "core_console_commands/set_serial.h"
#include "core_console_commands/throw.h"
#include "core_console_commands/upload.h"
#include "core_console_commands/uptime.h"
#include "core_console_commands/version.h"

#include "blade_console_commands/adc.h"
#include "blade_console_commands/dac.h"
#include "blade_console_commands/xvctarget.h"

static void register_core_console_commands(CommandParser &parser) {
	console_command_parser.register_command("uptime", std::make_shared<ConsoleCommand_uptime>());
	console_command_parser.register_command("date", std::make_shared<ConsoleCommand_date>());
	console_command_parser.register_command("version", std::make_shared<ConsoleCommand_version>());
	console_command_parser.register_command("ps", std::make_shared<ConsoleCommand_ps>());
	console_command_parser.register_command("restart", std::make_shared<ConsoleCommand_restart>());
	console_command_parser.register_command("flash_info", std::make_shared<ConsoleCommand_flash_info>());
	console_command_parser.register_command("setauth", std::make_shared<ConsoleCommand_setauth>());
	if (IPMC_SERIAL == 0 || IPMC_SERIAL == 0xFFFF) // The serial is settable only if unset.  This implements lock on write (+reboot).
		console_command_parser.register_command("set_serial", std::make_shared<ConsoleCommand_set_serial>());
	console_command_parser.register_command("upload", std::make_shared<ConsoleCommand_upload>());
	console_command_parser.register_command("throw", std::make_shared<ConsoleCommand_throw>());
	console_command_parser.register_command("trace", std::make_shared<ConsoleCommand_trace>());

	console_command_parser.register_command("adc", std::make_shared<ConsoleCommand_adc>());
	console_command_parser.register_command("dac", std::make_shared<ConsoleCommand_dac>());
	console_command_parser.register_command("xvctarget", std::make_shared<ConsoleCommand_xvctarget>(xvctarget_gpio, PL_GPIO::Channel::GPIO_CHANNEL1, 0, 1));
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
