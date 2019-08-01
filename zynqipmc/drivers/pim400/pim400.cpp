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

// TODO: Do better error handling.

#include "pim400.h"
#include <libs/printf.h>
#include <libs/except.h>
#include <services/console/consolesvc.h>

PIM400::PIM400(I2C &i2c, uint8_t addr) :
i2c(i2c), kAddr(addr>>1) {
	if (this->kAddr == 0) {
		throw std::domain_error("Invalid I2C address (0) for PIM400");
	}
}

PIM400::~PIM400() {
}

float PIM400::getHoldupVoltage() {
	return ((this->readRegister(PIM400_V_HLDP)*1.0) * 0.398);
}

float PIM400::getOutCurrent() {
	return ((this->readRegister(PIM400_NEG48V_IOUT)*1.0) * 0.094);
}

float PIM400::getFeedAVoltage() {
	return ((this->readRegister(PIM400_NEG48V_AF)*1.0) * 0.325);
}

float PIM400::getFeedBVoltage() {
	return ((this->readRegister(PIM400_NEG48V_BF)*1.0) * 0.325);
}

float PIM400::getTemperature() {
	return ((this->readRegister(PIM400_TEMP)*1.0) * 1.961 - 50.0);
}

PIM400::PIM400Status PIM400::getStatus() {
	PIM400::PIM400Status r;
	r._raw = this->readRegister(PIM400_STATUS);
	return r;
}


class PIM400::Status : public CommandParser::Command {
public:
	Status(PIM400 &pim400) : pim400(pim400) { };

	virtual std::string getHelpText(const std::string &command) const {
		return command + "\n\n"
				"Read the PIM400 status.\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		float holdup = pim400.getHoldupVoltage();
		float current = pim400.getOutCurrent();
		float feeda = pim400.getFeedAVoltage();
		float feedb = pim400.getFeedBVoltage();
		float temp = pim400.getTemperature();
		PIM400::PIM400Status status = pim400.getStatus();

		std::string str;
		str += stdsprintf("Holdup voltage: %.2fV\n", holdup);
		str += stdsprintf("Out current: %.3fA\n", current);
		str += stdsprintf("Feed A voltage: %.2fV\n", feeda);
		str += stdsprintf("Feed B voltage: %.2fV\n", feedb);
		str += stdsprintf("Temperature: %.1fC\n", temp);
		str += stdsprintf("Status register:\n");
		str += stdsprintf("\tENABLE_AF is %s\n", (status.enable_af?"enabled":"disabled"));
		str += stdsprintf("\tENABLE_BF is %s\n", (status.enable_bf?"enabled":"disabled"));
		str += stdsprintf("\tAlarm %s set\n", (status.alarm_set?"is":"not"));
		str += stdsprintf("\tC_HLDP %s connected\n", (status.hdlp_connected?"is":"not"));
		str += stdsprintf("\tHotswap switch is %s\n", (status.hotswap_on?"on":"off"));
		str += stdsprintf("\t-48V_OUT is %s threshold\n", (status.out_volt_undervoltage?"above":"below"));
		console->write(str);
	}

private:
	PIM400 &pim400;
};

uint8_t PIM400::readRegister(const PIM400::PIM400Register reg) {
	uint8_t a = reg;
	uint8_t r = 0xFF;

	this->i2c.atomic([this, &a, &r]() -> void {
		this->i2c.write(this->kAddr, &a, 1, 1000 / portTICK_RATE_MS);
		this->i2c.read(this->kAddr, &r, 1, 1000 / portTICK_RATE_MS);
	});

	return r;
}

void PIM400::registerConsoleCommands(CommandParser &parser, const std::string &prefix) {
	parser.registerCommand(prefix + "status", std::make_shared<PIM400::Status>(*this));
}

void PIM400::deregisterConsoleCommands(CommandParser &parser, const std::string &prefix) {
	parser.registerCommand(prefix + "status", nullptr);
}

