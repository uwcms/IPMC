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

void driver_init(bool use_pl);
void ipmc_service_init();

#endif /* SRC_IPMC_H_ */
