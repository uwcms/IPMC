/*
 * LTC2654F.cpp
 *
 *  Created on: Jan 7, 2019
 *      Author: mpv
 */

#include "LTC2654F.h"
#include "libs/Utils.h"

LTC2654F::LTC2654F(SPIMaster& spi, uint8_t cs, bool is12Bits) :
is12Bits(is12Bits), spi(spi), cs(cs) {
}

bool LTC2654F::sendCommand(Address addr, Command cmd, uint16_t val) {
	uint8_t data[3] = {0};

	data[0] = ((cmd << 4) & 0xF0) | (addr & 0x0F);
	if (this->is12Bits) {
		data[1] = val >> 4;
		data[2] = (val << 4) & 0xF0;
	} else {
		data[1] = (val >> 8);
		data[2] = val;
	}

	return this->spi.transfer(this->cs, data, NULL, sizeof(data), pdMS_TO_TICKS(1000));
}

