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

#ifndef SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_SDR_SENSORDATARECORD01_H_
#define SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_SDR_SENSORDATARECORD01_H_

#include <services/ipmi/sdr/sensor_data_record_readable_sensor.h>
#include <stdint.h>
#include <vector>

/**
 * A SensorDataRecord representing and parsing a Type 01 SDR.
 */
class SensorDataRecord01 : public SensorDataRecordReadableSensor {
public:
	/// Instantiate a Type 01 SensorDataRecord
	SensorDataRecord01(const std::vector<uint8_t> &sdr_data = std::vector<uint8_t>()) : SensorDataRecordSensor(sdr_data), SensorDataRecordReadableSensor(sdr_data) { };
	virtual ~SensorDataRecord01() { };

	virtual void validate() const;
	virtual uint8_t parsedRecordType() const { return 0x01; };

	/**
	 * SDR Data Accessors
	 *
	 * @warning Do not call any accessors on a record that does not validate().
	 */
	///@{
	// Helper macro to instantiate getters and setters.
	// a & b represent bit fields used in the source file.
#define SDR_FIELD(name, type, byte, a, b, attributes) \
	virtual type name() const attributes; \
	virtual void name(type val) attributes;

	enum UnitsNumericFormat {
		UNITS_UNSIGNED     = 0,//!< UNITS_UNSIGNED
		UNITS_1SCOMPLEMENT = 1,//!< UNITS_1SCOMPLEMENT
		UNITS_2SCOMPLEMENT = 2,//!< UNITS_2SCOMPLEMENT
		UNITS_NONNUMERIC   = 3,//!< UNITS_NONNUMERIC
	};
	SDR_FIELD(units_numeric_format, enum UnitsNumericFormat, 20, 7, 6, )

	enum Linearization {
		LIN_LINEAR = 0,
		LIN_LN,
		LIN_LOG10,
		LIN_LOG2,
		LIN_E,
		LIN_EXP10,
		LIN_EXP2,
		LIN_1DIVX,
		LIN_SQRX,
		LIN_CUBEX,
		LIN_SQRTX,
		LIN_CUBERTX,
		LIN_NONLINEAR       = 0x70,
		LIN_NONLINEAR_OEM_1 = 0x71,
		LIN_NONLINEAR_OEM_2 = 0x72,
		LIN_NONLINEAR_OEM_3 = 0x73,
		LIN_NONLINEAR_OEM_4 = 0x74,
		LIN_NONLINEAR_OEM_5 = 0x75,
		LIN_NONLINEAR_OEM_6 = 0x76,
		LIN_NONLINEAR_OEM_7 = 0x77,
		LIN_NONLINEAR_OEM_8 = 0x78,
		LIN_NONLINEAR_OEM_9 = 0x79,
		LIN_NONLINEAR_OEM_A = 0x7A,
		LIN_NONLINEAR_OEM_B = 0x7B,
		LIN_NONLINEAR_OEM_C = 0x7C,
		LIN_NONLINEAR_OEM_D = 0x7D,
		LIN_NONLINEAR_OEM_E = 0x7E,
		LIN_NONLINEAR_OEM_F = 0x7F,
	};
	SDR_FIELD(linearization, enum Linearization, 23, 7, 0, )

	SDR_FIELD(conversion_m, uint16_t, 24 AND 25, 7, 0, )
	SDR_FIELD(conversion_m_tolerance, uint8_t, 25, 5, 0, ) // Unit: +/- half raw counts

	SDR_FIELD(conversion_b, uint16_t, 26 AND 27, 7, 0, )
	SDR_FIELD(conversion_b_accuracy, uint16_t, 27 AND 28, 7, 0, )
	SDR_FIELD(conversion_b_accuracy_exp, uint8_t, 28, 3, 2, )

	SDR_FIELD(sensor_direction, enum SensorDataRecordReadableSensor::Direction, 28, 1, 0, )

	SDR_FIELD(conversion_r_exp, int8_t, 29, 7, 4, )
	SDR_FIELD(conversion_b_exp, int8_t, 29, 3, 0, )

	SDR_FIELD(normal_min_specified, bool, 30, 2, 2, )
	SDR_FIELD(normal_max_specified, bool, 30, 1, 1, )
	SDR_FIELD(nominal_reading_specified, bool, 30, 0, 0, )

	SDR_FIELD(nominal_reading_rawvalue, uint8_t, 31, 7, 0, )
	SDR_FIELD(normal_max_rawvalue, uint8_t, 32, 7, 0, )
	SDR_FIELD(normal_min_rawvalue, uint8_t, 33, 7, 0, )

	SDR_FIELD(sensor_min_rawvalue, uint8_t, 34, 7, 0, )
	SDR_FIELD(sensor_max_rawvalue, uint8_t, 35, 7, 0, )

	SDR_FIELD(threshold_unr_rawvalue, uint8_t, 36, 7, 0, )
	SDR_FIELD(threshold_ucr_rawvalue, uint8_t, 37, 7, 0, )
	SDR_FIELD(threshold_unc_rawvalue, uint8_t, 38, 7, 0, )
	SDR_FIELD(threshold_lnr_rawvalue, uint8_t, 39, 7, 0, )
	SDR_FIELD(threshold_lcr_rawvalue, uint8_t, 40, 7, 0, )
	SDR_FIELD(threshold_lnc_rawvalue, uint8_t, 41, 7, 0, )

	SDR_FIELD(hysteresis_high, uint8_t, 42, 7, 0, )
	SDR_FIELD(hysteresis_low, uint8_t, 43, 7, 0, )

	SDR_FIELD(oem, uint8_t, 46, 7, 0, )

#undef SDR_FIELD
	virtual uint8_t getIdStringOffset() const { return 47; };
	///@}

	virtual uint8_t fromFloat(float value) const;
	virtual float toFloat(uint8_t value) const;
};

#endif /* SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_SDR_SENSORDATARECORD01_H_ */
