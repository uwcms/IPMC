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

#include <math.h>
#include <services/ipmi/sdr/sensor_data_record_01.h>

void SensorDataRecord01::validate() const {
	SensorDataRecordReadableSensor::validate();
	if (this->recordType() != 0x01)
		throw invalid_sdr_error("SensorDataRecord01 supports only type 01h SDRs.");
}

//! Create a bitmask with the lower `nbits` bits set.
#define LOWBITS(nbits) (0xff >> (8-(nbits)))

//! Define a `type` type SDR_FIELD from byte `byte`[a:b].
#define SDR_FIELD(name, type, byte, a, b, attributes) \
	type SensorDataRecord01::name() const attributes { \
		this->validate(); \
		return static_cast<type>((this->sdr_data[byte] >> (b)) & LOWBITS((a)-(b)+1)); \
	} \
	void SensorDataRecord01::name(type val) attributes { \
		if ((static_cast<uint8_t>(val) & LOWBITS((a)-(b)+1)) != static_cast<uint8_t>(val)) \
			throw std::domain_error("The supplied value does not fit correctly in the field."); \
		this->validate(); \
		this->sdr_data[byte] &= ~(LOWBITS((a)-(b)+1)<<(b)); /* Erase old value */ \
		this->sdr_data[byte] |= static_cast<uint8_t>(val)<<(b); /* Set new value */ \
	}

SDR_FIELD(units_numeric_format, enum SensorDataRecord01::UnitsNumericFormat, 20, 7, 6, )

SDR_FIELD(linearization, enum SensorDataRecord01::Linearization, 23, 7, 0, )

int16_t SensorDataRecord01::conversion_m() const {
	this->validate();
	int16_t signedMS = (int16_t)(((int8_t)this->sdr_data[25]) >> 6);
	return (signedMS << 8) | this->sdr_data[24];
}
void SensorDataRecord01::conversion_m(int16_t val) {
    // NOTE: Cannot use bit masks for this check because if have 16
    // bits, and reduce to 10, the value 512 and -512 look the same
    // with a bit mask, however, 512 cannot be stored in 10-bits
    // signed.
    if (val > 511 || val < -512)
	throw std::domain_error("The supplied value does not fit correctly in the field.");
    this->validate();
    this->sdr_data[24] = val & 0x00ff;
    this->sdr_data[25] &= ~0xc0;
    this->sdr_data[25] |= (val & 0x0300) >> 2;
}

SDR_FIELD(conversion_m_tolerance, uint8_t, 25, 5, 0, ) // Unit: +/- half raw counts

int16_t SensorDataRecord01::conversion_b() const {
	this->validate();
	int16_t signedMS = (int16_t)(((int8_t)this->sdr_data[27]) >> 6);
	return (signedMS << 8) | this->sdr_data[26];
}
void SensorDataRecord01::conversion_b(int16_t val) {
    // NOTE: Cannot use bit masks for this check because if have 16
    // bits, and reduce to 10, the value 512 and -512 look the same
    // with a bit mask, however, 512 cannot be stored in 10-bits
    // signed.
    if (val > 511 || val < -512)
	throw std::domain_error("The supplied value does not fit correctly in the field.");
    this->validate();
    this->sdr_data[26] = val & 0x00ff;
    this->sdr_data[27] &= ~0xc0;
    this->sdr_data[27] |= (val & 0x0300) >> 2;
}

uint16_t SensorDataRecord01::conversion_b_accuracy() const {
	this->validate();
	return (this->sdr_data[27] & 0x3f) | (((uint16_t)this->sdr_data[28]&0x00f0)<<2);
}
void SensorDataRecord01::conversion_b_accuracy(uint16_t val) {
	if ((val & 0x3ff) != val)
		throw std::domain_error("The supplied value does not fit correctly in the field.");
	this->validate();
	this->sdr_data[27] &= ~0x3f;
	this->sdr_data[27] |= val & 0x3f;
	this->sdr_data[28] &= ~0xf0;
	this->sdr_data[28] |= (val >> 2) & 0x00f0;
}

SDR_FIELD(conversion_b_accuracy_exp, uint8_t, 28, 3, 2, )

SDR_FIELD(sensor_direction, enum SensorDataRecordReadableSensor::Direction, 28, 1, 0, )

int8_t SensorDataRecord01::conversion_r_exp() const {
	this->validate();
	uint8_t val = this->sdr_data[29] >> 4;
	if (val & 0x08)
		val |= 0xf0; // Extend the 1 sign to 8 bits. (2s complement)
	return *reinterpret_cast<int8_t*>(&val);
}
void SensorDataRecord01::conversion_r_exp(int8_t val) {
	if (val > 7 || val < -8)
		throw std::domain_error("The supplied value does not fit correctly in the field.");
	this->validate();
	this->sdr_data[29] = (val<<4) | (this->sdr_data[29] & 0x0f);
}

int8_t SensorDataRecord01::conversion_b_exp() const {
	this->validate();
	uint8_t val = this->sdr_data[29] & 0x0f;
	if (val & 0x08)
		val |= 0xf0; // Extend the 1 sign to 8 bits. (2s complement)
	return *reinterpret_cast<int8_t*>(&val);
}
void SensorDataRecord01::conversion_b_exp(int8_t val) {
	if (val > 7 || val < -8)
		throw std::domain_error("The supplied value does not fit correctly in the field.");
	this->validate();
	this->sdr_data[29] = (this->sdr_data[29] & 0xf0) | (val & 0x0f);
}

SDR_FIELD(normal_min_specified, bool, 30, 2, 2, )
SDR_FIELD(normal_max_specified, bool, 30, 1, 1, )
SDR_FIELD(nominal_reading_specified, bool, 30, 0, 0, )

SDR_FIELD(nominal_reading_rawvalue, uint8_t, 31, 7, 0, )
SDR_FIELD(normal_max_rawvalue, uint8_t, 32, 7, 0, )
SDR_FIELD(normal_min_rawvalue, uint8_t, 33, 7, 0, )

// SDG: Old code had sensor_max_rawvalue and sensor_min_rawvalue bytes swapped w.r.t v2.0 of IPMI spec.
SDR_FIELD(sensor_max_rawvalue, uint8_t, 34, 7, 0, )
SDR_FIELD(sensor_min_rawvalue, uint8_t, 35, 7, 0, )

SDR_FIELD(threshold_unr_rawvalue, uint8_t, 36, 7, 0, )
SDR_FIELD(threshold_ucr_rawvalue, uint8_t, 37, 7, 0, )
SDR_FIELD(threshold_unc_rawvalue, uint8_t, 38, 7, 0, )
SDR_FIELD(threshold_lnr_rawvalue, uint8_t, 39, 7, 0, )
SDR_FIELD(threshold_lcr_rawvalue, uint8_t, 40, 7, 0, )
SDR_FIELD(threshold_lnc_rawvalue, uint8_t, 41, 7, 0, )

SDR_FIELD(hysteresis_high, uint8_t, 42, 7, 0, )
SDR_FIELD(hysteresis_low, uint8_t, 43, 7, 0, )

SDR_FIELD(oem, uint8_t, 46, 7, 0, )


uint8_t SensorDataRecord01::fromFloat(float value) const {
	// The reader side conversion function is: float = L[(M*raw + (B * 10^(Bexp) ) ) * 10^(Rexp) ] units

	// Apply L_inv(x).  Done.  We only support linear sensors atm where this is L(x)=x.
	if (this->linearization() != SensorDataRecord01::LIN_LINEAR) {
		/* Alas, we have no exceptions, so we will return a value that is
		 * obviously wrong and will trigger alarms.
		 */
		return 0xFF;
	}

	// Divide by 10^(Rexp).
	value /= powf(10,this->conversion_r_exp());

	// Subtract B*10^Bexp.
	value -= this->conversion_b() * powf(10, this->conversion_b_exp());

	// Divide by M.
	value /= this->conversion_m();

	// Round to nearest integer
	value = roundf(value);

	// And now I guess it's a uint8_t?
	uint8_t raw_discrete_value = static_cast<uint8_t>(value);

	// Resolve domain errors.
	if (value > 255.0)
		raw_discrete_value = 0xff;
	if (value < 0.0)
		raw_discrete_value = 0;

	return raw_discrete_value;
}

float SensorDataRecord01::toFloat(uint8_t value) const {
	// The reader side conversion function is: float = L[(M*raw + (B * 10^(Bexp) ) ) * 10^(Rexp) ] units

	// Into float land to begin calculations.
	float fval = static_cast<float>(value);

	// Multiply by M.
	fval *= this->conversion_m();

	// Add B*10^Bexp.
	fval += this->conversion_b() * powf(10, this->conversion_b_exp());

	// Multiply by 10^(Rexp).
	fval *= powf(10,this->conversion_r_exp());

	// Apply L(x).  Done.  We only support linear sensors atm where this is L(x)=x.
	if (this->linearization() != SensorDataRecord01::LIN_LINEAR) {
		// Alas, we have no exceptions, so we will return the equivalent of null.
		return NAN;
	}

	return fval;
}
