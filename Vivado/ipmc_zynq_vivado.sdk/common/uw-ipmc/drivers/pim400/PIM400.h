/*
 * PIM400.h
 *
 *  Created on: Feb 26, 2018
 *      Author: mpv
 */

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_PIM400_PIM400_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_PIM400_PIM400_H_

#include "drivers/pl_i2c/PLI2C.h"
#include <IPMC.h>

/**
 * PIM400 power module I2C interface driver.
 *
 * Provides a low-level API function set to retrieve status from a PIM module.
 */
class PIM400 {
public:
	PIM400(I2C &MasterI2C, u8 Addr);
	virtual ~PIM400();

	float read_holdup_voltage();
	float read_out_current();
	float read_feedA_voltage();
	float read_feedB_voltage();
	float read_temperature();

	void register_console_commands(CommandParser &parser, const std::string &prefix="");
	void deregister_console_commands(CommandParser &parser, const std::string &prefix="");

private:
	I2C &i2c;
	u8 PimAddr;

	enum PIM400_REGISTERS {
		PIM400_STATUS = 0x1E,
		PIM400_V_HLDP = 0x1F,
		PIM400_NEG48V_IOUT = 0x21,
		PIM400_NEG48V_AF = 0x22,
		PIM400_NEG48V_BF = 0x23,
		PIM400_TEMP = 0x28,
	};

	u8 read_int_reg(const PIM400_REGISTERS reg);

};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_PIM400_PIM400_H_ */
