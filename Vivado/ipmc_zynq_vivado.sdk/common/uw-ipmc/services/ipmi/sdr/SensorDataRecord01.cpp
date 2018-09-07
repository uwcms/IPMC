/*
 * SensorDataRecord01.cpp
 *
 *  Created on: Aug 3, 2018
 *      Author: jtikalsky
 */

#include <services/ipmi/sdr/SensorDataRecord01.h>
#include <IPMC.h>

bool SensorDataRecord01::validate() const {
	if (!SensorDataRecordReadableSensor::validate())
		return false;
	if (this->record_type() != 0x01)
		return false;
	return true;
}

/// Create a bitmask with the lower `nbits` bits set.
#define LOWBITS(nbits) (0xff >> (8-(nbits)))

/// Define a `type` type SDR_FIELD from byte `byte`[a:b].
#define SDR_FIELD(name, type, byte, a, b) \
	type SensorDataRecord01::name() const { \
		configASSERT(this->validate()); \
		return static_cast<type>((this->sdr_data[byte] >> (b)) & LOWBITS((a)-(b)+1)); \
	} \
	void SensorDataRecord01::name(type val) { \
		configASSERT((static_cast<uint8_t>(val) & LOWBITS((a)-(b)+1)) == static_cast<uint8_t>(val)); \
		configASSERT(this->validate()); \
		this->sdr_data[byte] &= ~(LOWBITS((a)-(b)+1)<<(b)); /* Erase old value */ \
		this->sdr_data[byte] |= static_cast<uint8_t>(val)<<(b); /* Set new value */ \
	}

SDR_FIELD(linearization, enum SensorDataRecord01::Linearization, 23, 7, 0)

uint16_t SensorDataRecord01::conversion_m() const {
	configASSERT(this->validate());
	return this->sdr_data[24] | ((this->sdr_data[25]&0xc0)<<2);
}
void SensorDataRecord01::conversion_m(uint16_t val) {
	configASSERT((val & 0x3ff) == val);
	configASSERT(this->validate());
	this->sdr_data[24] = val & 0xff;
	this->sdr_data[25] &= ~0xc0;
	this->sdr_data[25] |= val >> 8;
}

SDR_FIELD(conversion_m_tolerance, uint8_t, 25, 5, 0) // Unit: +/- half raw counts

uint16_t SensorDataRecord01::conversion_b() const {
	configASSERT(this->validate());
	return this->sdr_data[26] | ((this->sdr_data[27]&0xc0)<<2);
}
void SensorDataRecord01::conversion_b(uint16_t val) {
	configASSERT((val & 0x3ff) == val);
	configASSERT(this->validate());
	this->sdr_data[26] = val & 0xff;
	this->sdr_data[27] &= ~0xc0;
	this->sdr_data[27] |= val >> 8;
}

uint16_t SensorDataRecord01::conversion_b_accuracy() const {
	configASSERT(this->validate());
	return (this->sdr_data[27] & 0x3f) | ((this->sdr_data[28]&0xf0)<<2);
}
void SensorDataRecord01::conversion_b_accuracy(uint16_t val) {
	configASSERT((val & 0x3ff) == val);
	configASSERT(this->validate());
	this->sdr_data[27] &= ~0x3f;
	this->sdr_data[27] |= val & 0x3f;
	this->sdr_data[28] &= ~0xf0;
	this->sdr_data[28] |= (val >> 2) & 0xf0;
}

SDR_FIELD(conversion_b_accuracy_exp, uint8_t, 28, 3, 2)

SDR_FIELD(sensor_direction, enum SensorDataRecordReadableSensor::Direction, 28, 1, 0)

int8_t SensorDataRecord01::r_exp() const {
	configASSERT(this->validate());
	uint8_t val = this->sdr_data[29] >> 4;
	if (val & 0x08)
		val |= 0xf0; // Extend the 1 sign to 8 bits. (2s complement)
	return *reinterpret_cast<int8_t*>(&val);
}
void SensorDataRecord01::r_exp(int8_t val) {
	uint8_t uval = *reinterpret_cast<uint8_t*>(&val);
	configASSERT((uval&0xf0) == 0 || (uval&0xf0) == 0xf0);
	configASSERT(this->validate());
	this->sdr_data[29] = (uval<<4) | (this->sdr_data[29] & 0x0f);
}

int8_t SensorDataRecord01::b_exp() const {
	configASSERT(this->validate());
	uint8_t val = this->sdr_data[29] & 0x0f;
	if (val & 0x08)
		val |= 0xf0; // Extend the 1 sign to 8 bits. (2s complement)
	return *reinterpret_cast<int8_t*>(&val);
}
void SensorDataRecord01::b_exp(int8_t val) {
	uint8_t uval = *reinterpret_cast<uint8_t*>(&val);
	configASSERT((uval&0xf0) == 0 || (uval&0xf0) == 0xf0);
	configASSERT(this->validate());
	this->sdr_data[29] = (this->sdr_data[29] & 0xf0) | uval;
}

SDR_FIELD(normal_min_specified, bool, 30, 2, 2)
SDR_FIELD(normal_max_specified, bool, 30, 1, 1)
SDR_FIELD(nominal_reading_specified, bool, 30, 0, 0)

SDR_FIELD(nominal_reading_rawvalue, uint8_t, 31, 7, 0)
SDR_FIELD(normal_max_rawvalue, uint8_t, 32, 7, 0)
SDR_FIELD(normal_min_rawvalue, uint8_t, 33, 7, 0)

SDR_FIELD(hysteresis_high, uint8_t, 42, 7, 0)
SDR_FIELD(hysteresis_low, uint8_t, 43, 7, 0)

SDR_FIELD(oem, uint8_t, 46, 7, 0)
