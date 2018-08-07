/**
 * This file is intended to contain foundational IPMC structures.  This mainly
 * consists of allocations of common extern driver instances, and the core init
 * functions.
 */

#include <FreeRTOS.h>
#include <task.h>
#include <IPMC.h>
#include "libs/Utils.h"
#include "libs/XilinxImage.h"
#include <drivers/watchdog/PSWDT.h>
#include <drivers/ps_uart/PSUART.h>
#include <drivers/ipmb/PSIPMB.h>
#include <drivers/mgmt_zone/MGMTZone.h>
#include <drivers/ps_isfqspi/PSISFQSPI.h>
#include <services/ipmi/ipmbsvc/IPMBSvc.h>
#include <services/ipmi/ipmbsvc/IPMICommandParser.h>
#include <services/ipmi/commands/IPMICmd_Index.h>
#include <drivers/ps_spi/PSSPI.h>
#include <drivers/spi_eeprom/SPIEEPROM.h>
#include <drivers/network/Network.h>
#include <services/persistentstorage/PersistentStorage.h>
#include <drivers/tracebuffer/TraceBuffer.h>
#include <services/console/UARTConsoleSvc.h>
#include <services/influxdb/InfluxDBClient.h>
#include <libs/LogTree.h>

#include <algorithm>
#include <functional>

/* Xilinx includes. */
#include "xparameters.h"
#include "xscutimer.h"
#include "xscugic.h"
#include "xil_exception.h"
#include "xgpiops.h"

#include <services/telnet/Telnet.h>
#include <alloca.h>
#include <drivers/ipmb/IPMBPair.h>

#include <services/lwiperf/Lwiperf.h>
#include <services/xvcserver/XVCServer.h>

#include <drivers/pim400/PIM400.h>
#include <drivers/pl_uart/PLUART.h>

#include <services/ftp/FTPServer.h>

u8 IPMC_HW_REVISION = 1; // TODO: Detect, Update, etc

PS_WDT *SWDT;
PS_UART *uart_ps0;
MGMT_Zone *mgmt_zones[XPAR_MGMT_ZONE_CTRL_0_MZ_CNT];
PS_ISFQSPI *isfqspi;
IPMBSvc *ipmb0;
Network *network;
IPMICommandParser *ipmi_command_parser;
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
InfluxDBClient *influxdbclient;

TelnetServer *telnet;

PL_UART *uart;

// External static variables
extern uint8_t mac_address[6]; ///< The MAC address of the board, read by persistent storage statically

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
	register_core_console_commands(console_command_parser);

	isfqspi = new PS_ISFQSPI(XPAR_PS7_QSPI_0_DEVICE_ID, XPAR_PS7_QSPI_0_INTR);

	PS_SPI *ps_spi0 = new PS_SPI(XPAR_PS7_SPI_0_DEVICE_ID, XPAR_PS7_SPI_0_INTR);
	eeprom_data = new SPI_EEPROM(*ps_spi0, 0, 0x8000, 64);
	eeprom_mac = new SPI_EEPROM(*ps_spi0, 1, 0x100, 16);
	persistent_storage = new PersistentStorage(*eeprom_data, LOG["persistent_storage"], SWDT);
	persistent_storage->register_console_commands(console_command_parser, "eeprom.");
	configASSERT(eeprom_mac->read(250, mac_address, 6));
	LOG["network"].log(stdsprintf("Our MAC address is %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
			mac_address[0], mac_address[1], mac_address[2], mac_address[3], mac_address[4], mac_address[5]), LogTree::LOG_NOTICE);

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

	// TODO: Clean up this part
	PL_I2C *i2c = new PL_I2C(XPAR_AXI_IIC_PIM400_DEVICE_ID, XPAR_FABRIC_AXI_IIC_PIM400_IIC2INTC_IRPT_INTR);
	(new PIM400(*i2c, 0x5E))->register_console_commands(console_command_parser, "pim400");

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

/** IPMC service initialization.
 *
 * This function contains the initialization for IPMC services, and is
 * responsible for connecting and enabling/activating drivers and IPMC related
 * services.  It will not be called from the bootloader or non-IPMC application
 * projects, and the PL is assumed to be loaded.
 *
 * \note This function is called before the FreeRTOS scheduler has been started.
 */
uint32_t *fakefile = NULL;
void ipmc_service_init() {
	console_service = UARTConsoleSvc::create(*uart_ps0, console_command_parser, "console", LOG["console"]["uart"], true);

	network = new Network(LOG["network"], mac_address, [](Network *network) {
		// Network Ready callback.

		influxdbclient = new InfluxDBClient(LOG["influxdb"]);
		influxdbclient->register_console_commands(console_command_parser, "influxdb.");
		telnet = new TelnetServer();

		// TODO: Can be removed when not needed
		new Lwiperf(5001);
		new XVCServer(XPAR_AXI_JTAG_0_BASEADDR);

		// TODO: This can be moved elsewhere
		FTPServer::setFiles({
			{"virtual", {0, NULL, NULL, true, {
				{"flash.bin", {16*1024*1024, [](uint8_t *buf) -> size_t {
					size_t totalsize = isfqspi->GetTotalSize();
					size_t pagesize = isfqspi->GetPageSize();
					int pagecount = totalsize / pagesize;

					for (int i = 0; i < pagecount; i++) {
						size_t addr = i * pagesize;
						uint8_t* ptr = isfqspi->ReadPage(addr);
						memcpy(buf + addr, ptr, pagesize);
					}

					return totalsize;
				}, [](uint8_t *buf, size_t len) -> size_t {
					// Validate the bin file before writing
					if (!validateBootFile(buf, len)) {
						// File is invalid!
						printf("Received bin file has errors, aborting firmware update\n");
						return 0;
					}

					// Write the buffer to flash
					const size_t baseaddr = 0x0;

					size_t rem = len % isfqspi->GetPageSize();
					size_t pages = len / isfqspi->GetPageSize() + (rem? 1 : 0);

					for (size_t i = 0; i < pages; i++) {
						size_t addr = i * isfqspi->GetPageSize();
						if (addr % isfqspi->GetSectorSize() == 0) {
							// This is the first page of a new sector, so erase the sector
							printf("Erasing 0x%08x", addr + baseaddr);
							if (!isfqspi->SectorErase(addr + baseaddr)) {
								// Failed to erase
								printf("Failed to erase 0x%08x", addr + baseaddr);
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
								printf("Failed to write page 0x%08x", addr + baseaddr);
								return addr;
							}
						} else {
							if (!isfqspi->WritePage(addr + baseaddr, buf + addr)) {
								// Failed to write page
								printf("Failed to write page 0x%08x", addr + baseaddr);
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
						if (ret != 0)
							printf("Page 0x%08x is different", addr + baseaddr);
					}

					return len;
				}, false, {}}},
			}},
		},
		});

		new FTPServer([](const std::string &user, const std::string &pass) -> bool {
			// TODO: For now, accept only ipmc/ipmc
			if (!user.compare("ipmc") && !pass.compare("ipmc")) return true;
			return false;
		});

	});
	network->register_console_commands(console_command_parser, "network.");

	uart = new PL_UART(XPAR_AXI_UARTLITE_ESM_DEVICE_ID, XPAR_FABRIC_AXI_UARTLITE_ESM_INTERRUPT_INTR);
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
	bannerstr += std::string("Build date  : ") + COMPILE_DATE + "\n";
	bannerstr += std::string("Build host  : ") + COMPILE_HOST + "\n";
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

#include "xuartlite_l.h"

namespace {
/// A "esm" console command.
class ConsoleCommand_esm : public CommandParser::Command {
public:
	virtual std::string get_helptext(const std::string &command) const {
		return stdsprintf(
				"%s\n"
				"\n"
				"Send a command to the ESM and see its output. Use ? to see possible commands.\n", command.c_str());
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		// Prepare the command
		int argn = parameters.nargs();
		std::string command = "", p;
		for (int i = 1; i < argn; i++) {
			parameters.parse_parameters(i, true, &p);
			if (i == (argn-1)) {
				command += p;
			} else {
				command += p + " ";
			}
		}

		// Check if there is a command to send
		if (command == "") {
			console->write("No command to send.\n");
			return;
		}

		// Terminate with '/r' to trigger ESM to respond
		command += "\r";

		// Clear the receiver buffer
		uart->clear();

		// Send the command
		uart->write((const u8*)command.c_str(), command.length(), pdMS_TO_TICKS(1000));

		// Read the incoming response
		// A single read such as:
		// size_t count = uart->read((u8*)buf, 2048, pdMS_TO_TICKS(1000));
		// .. works but reading one character at a time allows us to detect
		// the end of the response which is '\r\n>'.
		char inbuf[2048] = "";
		size_t pos = 0, count = 0;
		while (pos < 2043) {
			count = uart->read((u8*)inbuf+pos, 1, pdMS_TO_TICKS(1000));
			if (count == 0) break; // No character received
			if ((pos > 3) && (memcmp(inbuf+pos-2, "\r\n>", 3) == 0)) break;
			pos++;
		}

		if (pos == 0) {
			console->write("No response from ESM.\n");
		} else if (pos == 2043) {
			console->write("An abnormal number of characters was received.\n");
		} else {
			// ESM will send back the command written and a new line.
			// At the end it will return '\r\n>', we erase this.
			size_t start = command.length() + 1;
			size_t end = strlen(inbuf);

			// Force the end of buf to be before '\r\n>'
			if (end > 3) { // We don't want a data abort here..
				inbuf[end-3] = '\0';
			}

			console->write(inbuf + start);
		}
	}

	//virtual std::vector<std::string> complete(const CommandParser::CommandParameters &parameters) const { };
};

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
		volatile uint32_t* slcr_unlock_reg = (uint32_t*)(0xF8000000 + 0x008); // SLCR is at 0xF8000000
		volatile uint32_t* pss_rst_ctrl_reg = (uint32_t*)(0xF8000000 + 0x200);

		*slcr_unlock_reg = 0xDF0D; // Unlock key
		*pss_rst_ctrl_reg = 1;
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

} // anonymous namespace

static void register_core_console_commands(CommandParser &parser) {
	console_command_parser.register_command("esm", std::make_shared<ConsoleCommand_esm>());
	console_command_parser.register_command("uptime", std::make_shared<ConsoleCommand_uptime>());
	console_command_parser.register_command("version", std::make_shared<ConsoleCommand_version>());
	console_command_parser.register_command("ps", std::make_shared<ConsoleCommand_ps>());
	console_command_parser.register_command("backend_power", std::make_shared<ConsoleCommand_backend_power>());
	console_command_parser.register_command("restart", std::make_shared<ConsoleCommand_restart>());
	console_command_parser.register_command("flash_info", std::make_shared<ConsoleCommand_flash_info>());
}

static void console_log_handler(LogTree &logtree, const std::string &message, enum LogTree::LogLevel level) {
	std::string logmsg = ConsoleSvc_log_format(message, level);

	/* We write with 0 timeout, because we'd rather lose lines than hang on UART
	* output.  That's what the tracebuffer is for anyway.
	*/
	if (!console_service || IN_INTERRUPT() || IN_CRITICAL()) {
		// Still early startup.
		windows_newline(logmsg);
		uart_ps0->write(logmsg.data(), logmsg.size(), 0);
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
