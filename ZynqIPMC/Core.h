/*
 * Core.h
 *
 *  Created on: Jun 28, 2019
 *      Author: mpv
 */

#ifndef SRC_COMMON_ZYNQIPMC_CORE_H_
#define SRC_COMMON_ZYNQIPMC_CORE_H_

#include <drivers/generics/flash.h>
#include <drivers/ps_qspi/ps_qspi.h>
#include <drivers/ps_uart/ps_uart.h>
#include <drivers/spi_eeprom/spi_eeprom.h>
#include <drivers/watchdog/ps_wdt.h>
#include <libs/logtree.h>

#include <services/persistentstorage/PersistentStorage.h>
#include <services/ipmi/ipmbsvc/IPMBSvc.h>
#include <services/ipmi/MStateMachine.h>


/**
 * Global variables common to all ZYNQ-IPMC applications
 */

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
extern uint8_t IMAGE_LOADED;

extern IPMBSvc *ipmb0;
extern IPMBSvc::EventReceiver ipmi_event_receiver;

extern MStateMachine *mstatemachine;

extern CommandParser console_command_parser;

//! Initialize the core drivers specific to the ZYNQ-IPMC hardware.
void core_driver_init();
void core_service_init();

extern void driver_init();
extern void service_init();

extern void tracebuffer_log_handler(LogTree &logtree, const std::string &message, enum LogTree::LogLevel level);
extern void watchdog_ontrip();


#endif /* SRC_COMMON_ZYNQIPMC_CORE_H_ */
