/**
 * This file is intended to contain foundational IPMC structures.  This mainly
 * consists of allocations of common extern driver instances, and the core init
 * functions.
 */

#include <FreeRTOS.h>
#include <task.h>
#include <IPMC.h>
#include <drivers/watchdog/PSWDT.h>
#include <drivers/ps_uart/PSUART.h>
#include <drivers/ps_ipmb/PSIPMB.h>
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

u8 IPMC_HW_REVISION = 1; // TODO: Detect, Update, etc

PS_WDT *SWDT;
PS_UART *uart_ps0;
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

// External static variables
extern uint8_t mac_address[6]; ///< The MAC address of the board, read by persistent storage statically

static void tracebuffer_log_handler(LogTree &logtree, const std::string &message, enum LogTree::LogLevel level);
static void register_core_console_commands(CommandParser &parser);
static void console_log_handler(LogTree &logtree, const std::string &message, enum LogTree::LogLevel level);

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
	SWDT = new PS_WDT(XPAR_PS7_WDT_0_DEVICE_ID, 8, LOG["watchdog"]);

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
	uint8_t hwaddr = IPMBSvc::lookup_ipmb_address(hwaddr_gpios);
	LogTree &log_ipmb0 = LOG["ipmi"]["ipmb"]["ipmb0"];
	log_ipmb0.log(stdsprintf("Our IPMB0 hardware address is %02Xh", hwaddr), LogTree::LOG_NOTICE);
	PS_IPMB *ps_ipmb[2];
	ps_ipmb[0] = new PS_IPMB(XPAR_PS7_I2C_0_DEVICE_ID, XPAR_PS7_I2C_0_INTR, hwaddr);
	ps_ipmb[1] = new PS_IPMB(XPAR_PS7_I2C_1_DEVICE_ID, XPAR_PS7_I2C_1_INTR, hwaddr);
	ipmi_command_parser = new IPMICommandParser(ipmicmd_default, *ipmicmd_index);
	ipmb0 = new IPMBSvc(ps_ipmb[0], ps_ipmb[1], hwaddr, ipmi_command_parser, log_ipmb0, "ipmb0", SWDT);
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

	network = new Network(LOG["network"], mac_address);
	influxdbclient = new InfluxDBClient(LOG["influxdb"]);
	influxdbclient->register_console_commands(console_command_parser, "influxdb.");
	telnet = new TelnetServer();
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
	bannerstr += std::string("University of Wisconsin IPMC ") + GIT_DESCRIBE + "\n";
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
		std::string out;
		UBaseType_t task_count = uxTaskGetNumberOfTasks();
		TaskStatus_t taskinfo[task_count+2];
		UBaseType_t total_runtime;
		if (!uxTaskGetSystemState(taskinfo, task_count+2, &total_runtime)) {
			print("Failed to generate process listing.\n");
			return;
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

		out += "PID Name             BasePrio CurPrio StackHW";
		if (runstats)
			out += " CPU% CPU";
		out += "\n";
		for (auto it = tasks.begin(), eit = tasks.end(); it != eit; ++it) {
			out += stdsprintf("%3lu %-16s %8lu %7lu %7hu",
					it->xTaskNumber,
					it->pcTaskName,
					it->uxBasePriority,
					it->uxCurrentPriority,
					it->usStackHighWaterMark);
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
		console->write(out);
	}

	//virtual std::vector<std::string> complete(const CommandParser::CommandParameters &parameters) const { };
};
} // anonymous namespace

static void register_core_console_commands(CommandParser &parser) {
	console_command_parser.register_command("uptime", std::make_shared<ConsoleCommand_uptime>());
	console_command_parser.register_command("version", std::make_shared<ConsoleCommand_version>());
	console_command_parser.register_command("ps", std::make_shared<ConsoleCommand_ps>());
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
