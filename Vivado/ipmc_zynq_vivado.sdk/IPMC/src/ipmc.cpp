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

/**
 * This file is intended to contain foundational IPMC structures.  This mainly
 * consists of allocations of common external driver instances, and the core init
 * functions.
 */

#include "zynqipmc_config.h"
#include "ipmc.h"

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <event_groups.h>
#include <algorithm>
#include <functional>
#include <alloca.h>
#include <sys/time.h>

/* Include ZYNQ-IPMC framework */
#include <core.h>
#include <payload_manager.h>

/* Include Xilinx related */
#include "xparameters.h"
#include "xscutimer.h"
#include "xscugic.h"
#include "xil_exception.h"
#include "xgpiops.h"

/* Include drivers */
#include <drivers/ad7689/ad7689.h>
#include <drivers/elm/elm.h>
#include <drivers/esm/esm.h>
#include <drivers/ipmb/ipmb_pair.h>
#include <drivers/ipmb/ps_ipmb.h>
#include <drivers/ltc2654/ltc2654.h>
#include <drivers/network/network.h>
#include <drivers/pim400/pim400.h>
#include <drivers/pl_gpio/pl_gpio.h>
#include <drivers/pl_i2c/pl_i2c.h>
#include <drivers/pl_led/pl_led.h>
#include <drivers/pl_spi/pl_spi.h>
#include <drivers/pl_uart/pl_uart.h>
#include <drivers/pmbus/pmbus.h>
#include <drivers/ps_gpio/ps_gpio.h>
#include <drivers/ps_qspi/ps_qspi.h>
#include <drivers/ps_spi/ps_spi.h>
#include <drivers/ps_uart/ps_uart.h>
#include <drivers/ps_xadc/ps_xadc.h>
#include <drivers/spi_eeprom/spi_eeprom.h>
#include <drivers/spi_flash/spi_flash.h>
#include <drivers/tracebuffer/tracebuffer.h>
#include <drivers/watchdog/ps_wdt.h>

/* Include services */
#include <services/sntp/sntp.h>
#include <services/console/uartconsolesvc.h>
#include <services/ftp/ftp.h>
#include <services/influxdb/influxdb.h>
#include <services/ipmi/commands/ipmicmd_index.h>
#include <services/ipmi/ipmbsvc/ipmbsvc.h>
#include <services/ipmi/ipmbsvc/ipmi_command_parser.h>
#include <services/ipmi/sdr/sensor_data_repository.h>
#include <services/lwiperf/lwiperf.h>
#include <services/persistentstorage/persistent_storage.h>
#include <services/telnet/telnet.h>
#include <services/xvcserver/xvcserver.h>
#include <services/ipmi/ipmi_formats.h>
#include <services/ipmi/m_state_machine.h>
#include <services/ipmi/sdr/sensor_data_record_01.h>
#include <services/ipmi/sdr/sensor_data_record_02.h>
#include <services/ipmi/sdr/sensor_data_record_12.h>
#include <services/ipmi/sensor/hotswap_sensor.h>
#include <services/ipmi/sensor/sensor.h>
#include <services/ipmi/sensor/sensor_set.h>
#include <services/ipmi/sensor/severity_sensor.h>
#include <services/ipmi/sensor/threshold_sensor.h>

/* Include libs */
#include <libs/printf.h>
#include "libs/base64/base64.h"
#include <libs/authentication/authentication.h>
#include <libs/backtrace/backtrace.h>
#include <libs/logtree/logtree.h>
#include <libs/utils.h>
#include <libs/xilinx_image/xilinx_image.h>


// Application specific variables
std::vector<AD7689*> adc;
PSXADC *xadc		= nullptr;

Network *network			= nullptr;
TelnetServer *telnet		= nullptr;

// Include core command code:
#include <core_commands/date.inc>
#include <core_commands/flash.inc>
#include <core_commands/ps.inc>
#include <core_commands/restart.inc>
#include <core_commands/setauth.inc>
#include <core_commands/set_serial.inc>
#include <core_commands/upload.inc>
#include <core_commands/uptime.inc>
#include <core_commands/version.inc>
#include <core_commands/boottarget.inc>
#include <core_commands/adc.inc>

/**
 * Function that gets runs in serviceInit() and all commands that shouldn't be added
 * need to be commented out.
 */
static void registerConsoleCommands(CommandParser &parser) {
	// Core specific commands:
	console_command_parser.registerCommand("uptime", std::make_shared<CoreCommands::UptimeCommand>());
	console_command_parser.registerCommand("date", std::make_shared<CoreCommands::DateCommand>());
	console_command_parser.registerCommand("version", std::make_shared<CoreCommands::VersionCommand>());
	console_command_parser.registerCommand("ps", std::make_shared<CoreCommands::PsCommand>());
	console_command_parser.registerCommand("restart", std::make_shared<CoreCommands::RestartCommand>());
	console_command_parser.registerCommand("flash.info", std::make_shared<CoreCommands::FlashInfoCommand>(*qspiflash));
	console_command_parser.registerCommand("flash.verify", std::make_shared<CoreCommands::FlashVerifyCommand>(*qspiflash));
	console_command_parser.registerCommand("setauth", std::make_shared<CoreCommands::SetAuthCommand>());
	if (IPMC_SERIAL == 0 || IPMC_SERIAL == 0xFFFF) // The serial is settable only if unset.  This implements lock on write (+reboot).
		console_command_parser.registerCommand("set_serial", std::make_shared<CoreCommands::SetSerialCommand>());
	console_command_parser.registerCommand("upload", std::make_shared<CoreCommands::UploadCommand>());
	console_command_parser.registerCommand("boottarget", std::make_shared<CoreCommands::BootTargetCommand>(eeprom_mac));
	console_command_parser.registerCommand("adc", std::make_shared<CoreCommands::AdcCommand>());
	StatCounter::registerConsoleCommands(parser);
}

/**
 * Driver initialization.
 *
 * This function contains initialization for application specific hardware drivers.
 * It may or may not activate or enable features.  It should not depend on any service,
 * nor make any service connections.
 *
 * The ZYNQ-IPMC framework will take care of initializing common drivers.
 */
void driverInit() {

	for (int i = 0; i < 2; i++) {
		const std::string name = "adc_" + std::to_string(i);
		AD7689* ad7689 = new AD7689(XPAR_AD7689_S_0_DEVICE_ID + i, name);
		if (!ad7689) throw std::runtime_error("Failed to create " + name + " instance");
		ad7689->registerConsoleCommands(console_command_parser, name + ".");
		adc.push_back(ad7689);
	}

	xadc = new PSXADC(XPAR_XADCPS_0_DEVICE_ID);
	if (!xadc) throw std::runtime_error("Failed to create xadc instance");
}


/**
 * IPMC service initialization.
 *
 * This function contains the initialization for application specific services.
 * It is responsible for connecting and enabling/activating drivers and IPMC related
 * services.
 *
 * The ZYNQ-IPMC framework will take care of initializing common services.
 */
void serviceInit() {

	// Last services should be network related
	network = new Network(LOG["network"], mac_address, [](Network *network) {
		// Network Ready callback, start primary services
		sntp_init();

		telnet = new TelnetServer(LOG["telnetd"]);

		// Start iperf server
		new Lwiperf(5001);

		// Start XVC server
		new XVCServer((void*)XPAR_JTAG_AXI_JTAG_0_BASEADDR, LOG["xvc"]);

		// Start FTP server
		new FTPServer(Auth::validateCredentials, LOG["ftp"]);

	});
	network->registerConsoleCommands(console_command_parser, "network.");

	// This has to be lower, so the serial number has been read by the time we register (or not register) set_serial.
	registerConsoleCommands(console_command_parser);
}

