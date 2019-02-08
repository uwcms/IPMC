/*
 * IPMC.h
 *
 *  Created on: Sep 14, 2017
 *      Author: jtikalsky
 */

#ifndef SRC_IPMC_H_
#define SRC_IPMC_H_

#include "xscugic.h"
#include "xgpiops.h"
#include <libs/LogTree.h>
#include <map>
#include <memory>
#include <stdint.h>

extern "C" {
	extern XScuGic xInterruptController;
}

const u8 IPMC_FW_REVISION[2] = { 0, 1 }; // 0.1 (Max: 63.99, see Get Device ID)
extern u8 IPMC_HW_REVISION;
extern uint16_t IPMC_SERIAL;

struct EventGroupDef_t;
typedef struct EventGroupDef_t * EventGroupHandle_t;
extern EventGroupHandle_t init_complete;

class PS_WDT;
extern PS_WDT *SWDT;

class PS_UART;
extern PS_UART *uart_ps0;
extern XGpioPs gpiops;

class IPMBSvc;
class IPMICommandParser;
extern IPMBSvc *ipmb0;
extern IPMICommandParser *ipmi_command_parser;
class MStateMachine;
extern MStateMachine *mstatemachine;
class PayloadManager;
extern PayloadManager *payload_manager;

typedef struct {
	IPMBSvc *ipmb;
	uint8_t lun;
	uint8_t addr;
} EventReceiver;
extern EventReceiver ipmi_event_receiver;

class SensorDataRepository;
extern SensorDataRepository sdr_repo;
extern SensorDataRepository device_sdr_repo;
class SensorSet;
extern SensorSet ipmc_sensors;
extern SemaphoreHandle_t fru_data_mutex;
extern std::vector<uint8_t> fru_data;

extern LogTree LOG;
extern LogTree::Filter *console_log_filter;
class TraceBuffer;
TraceBuffer& get_tracebuffer();
#define TRACE get_tracebuffer()

class SPI_EEPROM;
extern SPI_EEPROM *eeprom_mac;
extern SPI_EEPROM *eeprom_data;
class PersistentStorage;
extern PersistentStorage *persistent_storage;
extern u8 mac_address[6];

class CommandParser;
extern CommandParser console_command_parser;

class IPMI_LED;
extern std::vector<IPMI_LED*> ipmi_leds; // Blue, Red, Green, Amber

void driver_init(bool use_pl);
void ipmc_service_init();

// All priorities must be less than configMAX_PRIORITIES (7)
#define TASK_PRIORITY_WATCHDOG    6
#define TASK_PRIORITY_PRIORITY    5 // Also used by the FreeRTOS timer thread, which handles deferred interrupts and similar.
#define TASK_PRIORITY_DRIVER      4
#define TASK_PRIORITY_SERVICE     3
#define TASK_PRIORITY_INTERACTIVE 2
#define TASK_PRIORITY_BACKGROUND  1
#define TASK_PRIORITY_IDLE        0 // Used by FreeRTOS.

#define UWIPMC_STANDARD_STACK_SIZE (16384/4)

std::string generate_banner();

// From libwrap.cc
void init_stdlib_mutex();
extern SemaphoreHandle_t stdlib_mutex;

// From version.cc
extern const long int GIT_SHORT_INT;
extern const char *GIT_SHORT;
extern const char *GIT_LONG;
extern const char *GIT_DESCRIBE;
extern const char *GIT_BRANCH;
extern const char *GIT_STATUS;
extern const char *COMPILE_DATE;
extern const char *COMPILE_HOST;
extern const char *BUILD_CONFIGURATION;

#endif /* SRC_IPMC_H_ */
