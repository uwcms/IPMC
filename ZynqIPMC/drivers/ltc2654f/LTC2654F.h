/*
 * LTC2654F.h
 *
 *  Created on: Jan 7, 2019
 *      Author: mpv
 */

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_LTC2654F_LTC2654F_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_LTC2654F_LTC2654F_H_

#include <stdint.h>
#include <drivers/generics/SPI.h>

/**
 * High-level driver for the LTC2654F Quad 16/12bit rail-to-rail DAC.
 * Each channel (A/B/C/D) is individually configured. The value of
 * DAC ratio is written but only takes affect after an update.
 */
class LTC2654F final {
public:
	enum Address : uint8_t {
		DAC_A = 0x00,
		DAC_B = 0x01,
		DAC_C = 0x02,
		DAC_D = 0x03,
		ALL_DACS = 0x0f
	};

	enum Command : uint8_t {
		WRITE_INPUT_REG = 0x00,
		UPDATE_DAC_REG = 0x01,
		WRITE_INPUT_REG_AND_UPDATE_ALL = 0x02,
		WRITE_AND_UPDATE_REG = 0x03,
		POWER_DOWN = 0x04,
		POWER_DOWN_CHIP = 0x05,
		SELECT_INTERNAL_REF = 0x06,
		SELECT_EXTERNAL_REF = 0x07,
		NO_OP = 0x0f
	};

	LTC2654F(SPIMaster& spi, uint8_t cs, bool is12Bits);
	virtual ~LTC2654F() {};

	/**
	 *
	 * @param addr
	 * @param cmd
	 * @param val
	 * @return False if failed
	 */
	bool sendCommand(Address addr, Command cmd, uint16_t val = 0);

	inline void setDAC(Address addr, float ratio) {
		uint16_t val = (this->is12Bits?0x0fff:0xffff) * ratio;
		this->sendCommand(addr, WRITE_INPUT_REG, val);
	}

private:
	bool is12Bits;
	SPIMaster &spi;
	uint8_t cs;
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_LTC2654F_LTC2654F_H_ */
