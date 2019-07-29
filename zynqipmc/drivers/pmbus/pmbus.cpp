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

#include <drivers/pmbus/pmbus.h>
#include <cmath>
#include <libs/printf.h>

//! CRC8 necessary for PEC calculation as part of the PMBus spec.
static uint8_t crc8(uint8_t x) {
	for (size_t i = 0; i < 8; i++) {
		char toxor = (x & 0x80) ? 0x07 : 0;

		x = x << 1;
		x = x ^ toxor;
	}

	return x;
}

//! PEC for PMBus error detection which uses CRC8
static uint8_t pec(uint8_t crc, const uint8_t *p, size_t count) {
	for (size_t i = 0; i < count; i++)
		crc = crc8(crc ^ p[i]);

	return crc;
}

//! Some PMBus returns are in linear units, this converts them to direct or double precision.
static double linear2direct(uint16_t l) {
	int32_t mantissa = l & 0x7ff;
	if (mantissa & 0x400) mantissa |= 0xfffff800; // Adjust for 2-complement

	int32_t exponent = ((l >> 11) & 0x1f);
	if (exponent & 0x10) exponent |= 0xffffffe0; // Adjust for 2-complement

	return mantissa * pow(2, exponent);
}

const std::map<PMBus::Command, PMBus::CommandDetails> PMBus::CommandInfo = {
	{VOUT_MODE, {1, "VOUT_MODE", Format::CUSTOM, Unit::NONE}},
	{READ_VIN, {2, "READ_VIN", Format::LINEAR, Unit::VOLT}},
	{READ_VOUT, {2, "READ_VOUT", Format::CUSTOM, Unit::VOLT}},
	{READ_IOUT, {2, "READ_IOUT", Format::LINEAR, Unit::AMPERE}},
	{READ_TEMPERATURE_1, {2, "READ_TEMPERATURE_1", Format::LINEAR, Unit::CELSIUS}},
	{READ_TEMPERATURE_2, {2, "READ_TEMPERATURE_2", Format::LINEAR, Unit::CELSIUS}},
	{READ_DUTY_CYCLE, {2, "READ_DUTY_CYCLE", Format::LINEAR, Unit::NONE}},
	{READ_FREQUENCY, {2, "READ_FREQUENCY", Format::LINEAR, Unit::NONE}},
};

PMBus::PMBus(I2C &i2c, uint8_t addr) :
i2c(i2c), kAddr(addr) {

}

PMBus::~PMBus() {
}

const std::string PMBus::unitToString(Unit unit) {
	switch (unit) {
	case Unit::NONE: return ""; break;
	case Unit::VOLT: return "Volt"; break;
	case Unit::AMPERE: return "Ampere"; break;
	case Unit::MILLISECONDS: return "Millisecond"; break;
	case Unit::CELSIUS: return "Celsius"; break;
	default: return "Unknown"; break;
	}

	return "";
}

double PMBus::sendCommand(Command cmd, std::vector<uint8_t> *opt) {
	PMBus::CommandDetails details = PMBus::CommandInfo.at(cmd);

	uint8_t buffer[details.length+1] = {0};

	this->i2c.atomic([this, cmd, &buffer, &details] (void) {
		// Do repeated start
		this->i2c.write(this->kAddr, (uint8_t*)&cmd, 1, pdMS_TO_TICKS(2000), true);
		this->i2c.read(this->kAddr, buffer, details.length+1, pdMS_TO_TICKS(2000));
	});

	uint8_t crcbytes[] = { (uint8_t)((this->kAddr<<1) & 0xfe), cmd, (uint8_t)((this->kAddr<<1) | 0x01) };
	uint8_t crc = pec(0, crcbytes, sizeof(crcbytes));
	crc = pec(crc, buffer, details.length);

	if (crc != (buffer)[details.length]) {
		throw std::runtime_error(stdsprintf("PMBus PEC mismatch (read 0x%02x, expected 0x%02x)", buffer[details.length], crc));
	}

	// Read successful
	if (details.format == Format::LINEAR) {
		uint16_t v = buffer[0] | (buffer[1] << 8);
		double c = linear2direct(v);
		return c;
	} else if (details.format == Format::CUSTOM) {
		// Custom format
		switch (cmd) {
		case Command::VOUT_MODE: {
			if (!opt) break;

			this->sendCommand(Command::VOUT_MODE);
		} break;
		case Command::READ_VOUT: {
			std::vector<uint8_t> vout_mode;

			this->sendCommand(Command::VOUT_MODE, &vout_mode);

			uint8_t mode = (vout_mode[0] >> 5) & 0x7;
			uint8_t parameter = vout_mode[0] & 0x1F;

			if (mode != 0) {
				throw std::runtime_error("Only linear mode is supported for VOUT_MODE");
			}

			double mantissa = (buffer[0] | (buffer[1] << 8));
			double c = mantissa * pow(2, parameter);

			return c;
		} break;
		default: break;
		}
		return 0.0;
	}

	// If we are here it is because opt wasn't assigned when it should have
	if (!opt) throw std::runtime_error("*opt is nullptr");

	return 0.0;
}
