/*
 * ELM.h
 *
 *  Created on: Feb 21, 2019
 *      Author: mpv
 */

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_ELM_ELM_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_ELM_ELM_H_

#include <FreeRTOS.h>
#include <semphr.h>
#include <string>
#include <drivers/generics/UART.h>
#include <drivers/pl_gpio/PLGPIO.h>
#include <services/console/CommandParser.h>
#include <services/console/ConsoleSvc.h>

/**
 * ELM driver that implements the software layers for ELM link and other features.
 */
class ELM {
public:
	ELM(UART *uart, PL_GPIO *gpio);
	~ELM();

	void register_console_commands(CommandParser &parser, const std::string &prefix="");
	void deregister_console_commands(CommandParser &parser, const std::string &prefix="");

protected:
	friend class elm_bootsource;

private:
	UART *uart;
	PL_GPIO *gpio;
};



#endif /* SRC_COMMON_UW_IPMC_DRIVERS_ELM_ELM_H_ */
