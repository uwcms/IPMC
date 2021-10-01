/*
 * This file is part of the ZYNQ-IPMC Framework.
 *
 * The ZYNQ-IPMC Framework is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The ZYNQ-IPMC Framework is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the ZYNQ-IPMC Framework.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "tps35667.h"
#include <cmath>
#include <libs/except.h>
#include <libs/printf.h>

TPS35667::TPS35667(I2C &i2cbus, uint8_t address)
    : i2cbus(i2cbus), address(address) {
	this->mutex = xSemaphoreCreateMutex();
	configASSERT(this->mutex);
}

TPS35667::~TPS35667() {
	xSemaphoreTake(this->mutex, portMAX_DELAY);
	vSemaphoreDelete(this->mutex);
}

static int32_t sign_extend_2c(uint32_t value, size_t source_bits) {
	const uint32_t extend_mask = 0xffffffff << source_bits;
	uint32_t extended;
	if (((uint32_t)1 << (source_bits - 1)) & value)
		extended = value | extend_mask;
	else
		extended = value & ~extend_mask;
	return *reinterpret_cast<int32_t *>(&extended);
}

float TPS35667::readVin(AbsoluteTimeout timeout) {
	uint16_t regval;
	try {
		regval = this->reg16read(0x88, timeout);
	}
	catch (std::runtime_error &e) {
		return NAN;
	}
	int32_t exponent = sign_extend_2c((regval >> 11) & 0x1f, 5);
	int32_t mantissa = sign_extend_2c(regval & 0x07ff, 11);
	return mantissa * std::pow(2, exponent);
}

float TPS35667::readIin(AbsoluteTimeout timeout) {
	uint16_t regval;
	try {
		regval = this->reg16read(0x89, timeout);
	}
	catch (std::runtime_error &e) {
		return NAN;
	}
	int32_t exponent = sign_extend_2c((regval >> 11) & 0x1f, 5);
	int32_t mantissa = sign_extend_2c(regval & 0x07ff, 11);
	return mantissa * std::pow(2, exponent);
}

float TPS35667::readVout(AbsoluteTimeout timeout) {
	uint16_t regval;
	try {
		regval = this->reg16read(0xD4, timeout);
	}
	catch (std::runtime_error &e) {
		return NAN;
	}
	int32_t mantissa = sign_extend_2c(regval, 16);
	return mantissa * std::pow(2, -9);
}

float TPS35667::readIout(AbsoluteTimeout timeout) {
	uint16_t regval;
	try {
		regval = this->reg16read(0x8C, timeout);
	}
	catch (std::runtime_error &e) {
		return NAN;
	}
	int32_t exponent = sign_extend_2c((regval >> 11) & 0x1f, 5);
	int32_t mantissa = sign_extend_2c(regval & 0x07ff, 11);
	return mantissa * std::pow(2, exponent);
}

float TPS35667::readPin(AbsoluteTimeout timeout) {
	uint16_t regval;
	try {
		regval = this->reg16read(0x97, timeout);
	}
	catch (std::runtime_error &e) {
		return NAN;
	}
	int32_t exponent = sign_extend_2c((regval >> 11) & 0x1f, 5);
	int32_t mantissa = sign_extend_2c(regval & 0x07ff, 11);
	return mantissa * std::pow(2, exponent);
}

float TPS35667::readPout(AbsoluteTimeout timeout) {
	uint16_t regval;
	try {
		regval = this->reg16read(0x96, timeout);
	}
	catch (std::runtime_error &e) {
		return NAN;
	}
	int32_t exponent = sign_extend_2c((regval >> 11) & 0x1f, 5);
	int32_t mantissa = sign_extend_2c(regval & 0x07ff, 11);
	return mantissa * std::pow(2, exponent);
}

float TPS35667::readTemperature(AbsoluteTimeout timeout) {
	uint16_t regval;
	try {
		regval = this->reg16read(0x8D, timeout);
	}
	catch (std::runtime_error &e) {
		return NAN;
	}
	int32_t exponent = sign_extend_2c((regval >> 11) & 0x1f, 5);
	int32_t mantissa = sign_extend_2c(regval & 0x07ff, 11);
	return mantissa * std::pow(2, exponent);
}

uint16_t TPS35667::reg16read(uint8_t address, AbsoluteTimeout timeout) {
	size_t rv;
	uint8_t buf[2];
	buf[0] = address;
	rv = this->i2cbus.write(this->address, buf, 1, timeout.getTimeout(), true);
	if (rv != 1)
		throw std::runtime_error("I2C write failed.");
	rv = this->i2cbus.read(this->address, buf, 2, timeout.getTimeout());
	if (rv != 2)
		throw std::runtime_error("I2C read failed.");
	return (static_cast<uint16_t>(buf[1]) << 8) | static_cast<uint16_t>(buf[0]);
}

namespace
{
class TPS35667_Power final : public CommandParser::Command {
public:
	TPS35667_Power(TPS35667 &chip) : chip(chip){};
	virtual std::string getHelpText(const std::string &command) const {
		return command + "\n\n"
		                 "Read out power information from the TPS35667.\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		float Vin = this->chip.readVin();
		if (std::isnan(Vin)) {
			console->write("Unable to read Vin.  Is the chip online?\n");
			return;
		}
		console->write(stdsprintf("Vin:   %6.3f Volts\n", Vin)); // xx.yyy
		console->write(stdsprintf("Iin:  %6.2f  Amps\n", this->chip.readIin())); // aa.bb
		console->write(stdsprintf("Vout:  %6.3f Volts\n", this->chip.readVout())); // x.yyy
		console->write(stdsprintf("Iout: %6.2f  Amps\n", this->chip.readIout())); // ccc.dd
		console->write(stdsprintf("Pin:  %6.2f  Watts\n", this->chip.readPin())); // eee.ff
		console->write(stdsprintf("Pout: %6.2f  Watts\n", this->chip.readPout())); // eee.ff
		console->write(stdsprintf("Temp: %6.2f  degrees Celsius\n", this->chip.readTemperature())); // zzz.yy
	}

private:
	TPS35667 &chip;
};
}; // anonymous namespace

void TPS35667::registerConsoleCommands(CommandParser &parser, const std::string &prefix) {
	parser.registerCommand(prefix + "power", std::make_shared<TPS35667_Power>(*this));
}

void TPS35667::deregisterConsoleCommands(CommandParser &parser, const std::string &prefix) {
	parser.registerCommand(prefix + "power", nullptr);
}
