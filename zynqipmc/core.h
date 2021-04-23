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

#ifndef SRC_COMMON_ZYNQIPMC_CORE_H_
#define SRC_COMMON_ZYNQIPMC_CORE_H_

#include <drivers/generics/flash.h>
#include <drivers/ps_qspi/ps_qspi.h>
#include <drivers/ps_uart/ps_uart.h>
#include <drivers/spi_eeprom/spi_eeprom.h>
#include <drivers/watchdog/ps_wdt.h>
#include <services/ipmi/ipmbsvc/ipmbsvc.h>
#include <services/persistentstorage/persistent_storage.h>
#include <drivers/tracebuffer/tracebuffer.h>
#include <libs/logtree/logtree.h>
#include <payload_manager.h>

#include <services/ipmi/sdr/sensor_data_repository.h>
#include <services/ipmi/ipmi_led/ipmi_led.h>
#include <services/ipmi/m_state_machine.h>
#include <services/ipmi/sensor/sensor_set.h>

//! ZYNQ-IPMC framework default priorities.
///@{
// All priorities must be less than configMAX_PRIORITIES (7)
#define TASK_PRIORITY_WATCHDOG    6
#define TASK_PRIORITY_PRIORITY    5 // Also used by the FreeRTOS timer thread, which handles deferred interrupts and similar.
#define TASK_PRIORITY_DRIVER      4
#define TASK_PRIORITY_SERVICE     3
#define TASK_PRIORITY_INTERACTIVE 2
#define TASK_PRIORITY_BACKGROUND  1
#define TASK_PRIORITY_IDLE        0 // Used by FreeRTOS.
///@}

// From version.cc:
extern const long int GIT_SHORT_INT;
extern const char *GIT_SHORT;
extern const char *GIT_LONG;
extern const char *GIT_DESCRIBE;
extern const char *GIT_BRANCH;
extern const char *GIT_STATUS;
extern const char *COMPILE_DATE;
extern const char *COMPILE_HOST;
extern const char *BUILD_CONFIGURATION;


/**
 * Global variables common to all ZYNQ-IPMC applications
 */

// TODO: Check where all of these are initialized

extern LogTree LOG;

extern PSWDT   *swdt;		///< System Watchdog Timer
extern PSUART  *psuart0;	///< PS UART (interface 0)
extern Flash   *qspiflash;	///< QSPI interface to on-board Flash

extern SPIEEPROM *eeprom_mac;
extern PersistentStorage *persistent_storage;
extern uint8_t mac_address[6];

const uint8_t IPMC_FW_REVISION[2] = { 0, 1 }; // 0.1 (Max: 63.99, see Get Device ID)
extern uint8_t IPMC_HW_REVISION;
extern uint16_t IPMC_SERIAL;
extern uint16_t BLADE_SERIAL;
extern uint8_t IMAGE_LOADED;

class BootConfig;
extern BootConfig *boot_config;

extern IPMBSvc *ipmb0;
extern IPMBSvc::EventReceiver ipmi_event_receiver;

extern MStateMachine *mstatemachine;

extern CommandParser console_command_parser;

extern LogTree LOG;
extern LogTree::Filter *console_log_filter;


// TODO: UNSORTED
extern SensorSet ipmc_sensors;

extern PayloadManager *payload_manager;

extern SensorDataRepository sdr_repo;
extern SensorDataRepository device_sdr_repo;

extern std::vector<IPMILED*> ipmi_leds; // Blue, Red, Green, Amber

extern SemaphoreHandle_t fru_data_mutex;
extern std::vector<uint8_t> fru_data;

typedef struct EventGroupDef_t * EventGroupHandle_t;
extern EventGroupHandle_t init_complete;

// From libwrap.cc
void init_stdlib_mutex();
extern SemaphoreHandle_t stdlib_mutex;

extern bool flash_upgrade_successful;

/**
 * Banner that gets displayed when booting, connectivity via Telnet and also after running version.
 * @return String with banner contents.
 * @note Weak function, can be overridden in ipmc.cpp.
 */
std::string generateBanner();

/**
 * Function triggered when the watchdog fails to be serviced.
 * By default a trace will be printed to terminal and log.
 * @note Weak function, can be overridden in ipmc.cpp.
 */
void watchdogOnTrip();

/**
 * Handler that runs when there are new messages in the LogTree.
 * Default behaviour will copy the exact messages to the Trace Buffer.
 * @param logtree LogTree reference where the message is coming from.
 * @param message String with the incoming message.
 * @param level Log severity level.
 * @note Weak function, can be overridden in ipmc.cpp.
 */
void tracebufferLogHandler(LogTree &logtree, const std::string &message, enum LogTree::LogLevel level);

// TODO: End of unsorted

//! Retrieve the global trace buffer reference, or just use TRACE.
TraceBuffer& getTraceBuffer();
#define TRACE getTraceBuffer()


//! Initialize and prepare the main init task. Can run before the FreeRTOS scheduler starts.
void startInitTask();
void core_driver_init();
void core_service_init();

//! Application specific driver initialization, normally implemented in ipmc.cpp.
extern void driverInit();

//! Application specific service initialization, normally implemented in ipmc.cpp.
extern void serviceInit();


#endif /* SRC_COMMON_ZYNQIPMC_CORE_H_ */
