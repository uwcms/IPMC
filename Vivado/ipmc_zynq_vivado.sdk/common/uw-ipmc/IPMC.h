/*
 * IPMC.h
 *
 *  Created on: Sep 14, 2017
 *      Author: jtikalsky
 */

#ifndef SRC_IPMC_H_
#define SRC_IPMC_H_

#include "libwrap.h" // Mutex wrappers for various non-reentrant stdlib functions.
#include "xscugic.h"
#include "xgpiops.h"
#include <libs/LogTree.h>

extern "C" {
	extern XScuGic xInterruptController;
}

class PS_UART;
extern PS_UART *uart_ps0;
extern XGpioPs gpiops;

class IPMBSvc;
class IPMICommandParser;
extern IPMBSvc *ipmb0;
extern IPMICommandParser ipmi_command_parser;

extern LogTree LOG;
extern LogTree::Filter *console_log_filter;
class TraceBuffer;
extern TraceBuffer TRACE;

class SPI_EEPROM;
extern SPI_EEPROM *eeprom_mac;
extern SPI_EEPROM *eeprom_data;
class PersistentStorage;
extern PersistentStorage *persistent_storage;

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

#endif /* SRC_IPMC_H_ */
