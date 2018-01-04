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

extern LogTree LOG;
extern LogTree::Filter *console_log_filter;
class TraceBuffer;
extern TraceBuffer TRACE;

class SPI_EEPROM;
extern SPI_EEPROM *eeprom_mac;
extern SPI_EEPROM *eeprom_data;

void driver_init(bool use_pl);
void ipmc_service_init();

#define TASK_PRIORITY_WATCHDOG    7 // configMAX_PRIORITIES
#define TASK_PRIORITY_PRIORITY    6
#define TASK_PRIORITY_DRIVER      5
#define TASK_PRIORITY_SERVICE     4
#define TASK_PRIORITY_INTERACTIVE 3
#define TASK_PRIORITY_BACKGROUND  2
#define TASK_PRIORITY_IDLE        0

#endif /* SRC_IPMC_H_ */
