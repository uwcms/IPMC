/*
 * PMBUS.h
 *
 *  Created on: Apr 19, 2019
 *      Author: mpv
 */

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_PMBUS_PMBUS_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_PMBUS_PMBUS_H_

#include <map>
#include <vector>
#include <drivers/generics/I2C.h>

class PMBus {
public:
	enum Command : uint8_t {
		VOUT_MODE = 0x20,

		READ_VIN = 0x88,
		READ_VOUT = 0x8B,
		READ_IOUT = 0x8C,
		READ_TEMPERATURE_1 = 0x8D,
		READ_TEMPERATURE_2 = 0x8E,
		READ_DUTY_CYCLE = 0x94,
		READ_FREQUENCY = 0x95,
	};

	enum Format { LINEAR, DIRECT, CUSTOM };

	enum Unit { NONE, VOLT, AMPERE, MILLISECONDS, CELSIUS };
	static const std::string unitToString(Unit unit);

	struct CommandDetails {
		size_t length;
		const std::string name;
		enum Format format;
		enum Unit unit;
	};

	static const std::map<Command, CommandDetails> CommandInfo;

	PMBus(I2C &i2c, uint8_t addr);
	virtual ~PMBus();

	double readCommand(Command cmd, std::vector<uint8_t> *opt = nullptr);

private:
	I2C &i2c;
	uint8_t addr;
};

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_PMBUS_PMBUS_H_ */
