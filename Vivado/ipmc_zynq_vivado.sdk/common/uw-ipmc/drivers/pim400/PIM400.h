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
	/**
	 * Initialize the PIM400 interface driver
	 *
	 * \param i2c       The I2C interface to where the PIM400 is connected to.
	 * \param addr      The PIM400 I2C address (does NOT require to be shifted!)
	 */
	PIM400(I2C &i2c, uint8_t i2c_addr);
	virtual ~PIM400();

	float read_holdup_voltage(); ///< Read the hold-up voltage in 0.398V steps
	float read_out_current(); ///< Read the output current in 0.094A steps
	float read_feedA_voltage(); ///< Read the feed A voltage in 0.325V steps
	float read_feedB_voltage(); ///< Read the feed B voltage in 0.325V steps
	float read_temperature(); ///< Read the module temperature in 1.961C steps

	/**
	 * PIM400 status register bits, used in PIM400::read_status
	 */
	typedef struct {
		union {
			struct {
				bool enable_af :1; ///< ENABLE_AF is enabled
				bool enable_bf :1; ///< ENABLE_BF is enabled
				bool alarm_set :1; ///< ALARM is set
				bool :1;
				bool hdlp_connected :1; ///< C_HLDP is connected
				bool hotswap_on :1; ///< Hotswap switch is on
				bool out_volt_undervoltage :1; ///< -48V_OUT is above threshold
				bool :1;
			};
			uint8_t _raw;
		};
	} pim400status;

	pim400status read_status(); ///< Read the status register

	void register_console_commands(CommandParser &parser, const std::string &prefix="");
	void deregister_console_commands(CommandParser &parser, const std::string &prefix="");

private:
	I2C &i2c;
	uint8_t i2c_addr;

	enum PIM400_REGISTERS {
		PIM400_STATUS = 0x1E,
		PIM400_V_HLDP = 0x1F,
		PIM400_NEG48V_IOUT = 0x21,
		PIM400_NEG48V_AF = 0x22,
		PIM400_NEG48V_BF = 0x23,
		PIM400_TEMP = 0x28,
	};

	uint8_t read_int_reg(const PIM400_REGISTERS reg);

};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_PIM400_PIM400_H_ */
