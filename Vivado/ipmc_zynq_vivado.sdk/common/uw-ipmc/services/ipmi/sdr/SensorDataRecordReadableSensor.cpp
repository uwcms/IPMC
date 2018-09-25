/*
 * SensorDataRecordReadableSensor.cpp
 *
 *  Created on: Aug 8, 2018
 *      Author: jtikalsky
 */

#include <services/ipmi/sdr/SensorDataRecordReadableSensor.h>
#include <IPMC.h>

/// Create a bitmask with the lower `nbits` bits set.
#define LOWBITS(nbits) (0xff >> (8-(nbits)))

/// Define a `type` type SDR_FIELD from byte `byte`[a:b].
#define SDR_FIELD(name, type, byte, a, b) \
	type SensorDataRecordReadableSensor::name() const { \
		configASSERT(this->validate()); \
		return static_cast<type>((this->sdr_data[byte] >> (b)) & LOWBITS((a)-(b)+1)); \
	} \
	void SensorDataRecordReadableSensor::name(type val) { \
		configASSERT((static_cast<uint8_t>(val) & LOWBITS((a)-(b)+1)) == static_cast<uint8_t>(val)); \
		configASSERT(this->validate()); \
		this->sdr_data[byte] &= ~(LOWBITS((a)-(b)+1)<<(b)); /* Erase old value */ \
		this->sdr_data[byte] |= static_cast<uint8_t>(val)<<(b); /* Set new value */ \
	}

SDR_FIELD(sensor_setable, bool, 10, 7, 7)
SDR_FIELD(scanning_enabled, bool, 10, 6, 6)
SDR_FIELD(events_enabled, bool, 10, 5, 5)
SDR_FIELD(initialize_thresholds, bool, 10, 4, 4)
SDR_FIELD(initialize_hysteresis, bool, 10, 3, 3)
SDR_FIELD(initialize_sensor_type, bool, 10, 2, 2)
SDR_FIELD(events_enabled_default, bool, 10, 1, 1)
SDR_FIELD(scanning_enabled_default, bool, 10, 0, 0)

SDR_FIELD(ignore_if_entity_absent, bool, 11, 7, 7)
SDR_FIELD(sensor_auto_rearm, bool, 11, 6, 6)
SDR_FIELD(sensor_hysteresis_support, uint8_t, 11, 5, 4)
SDR_FIELD(sensor_threshold_access_support, uint8_t, 11, 3, 2)
SDR_FIELD(sensor_event_message_control_support, uint8_t, 11, 1, 0)

SDR_FIELD(sensor_type_code, uint8_t, 12, 7, 0)
SDR_FIELD(event_type_reading_code, uint8_t, 13, 7, 0)

uint16_t SensorDataRecordReadableSensor::assertion_lower_threshold_reading_mask() const {
	configASSERT(this->validate());
	return (this->sdr_data[15]<<8) | this->sdr_data[14];
}
void SensorDataRecordReadableSensor::assertion_lower_threshold_reading_mask(uint16_t val) {
	configASSERT(this->validate());
	this->sdr_data[15] = val >> 8;
	this->sdr_data[14] = val & 0xff;
}

uint16_t SensorDataRecordReadableSensor::deassertion_upper_threshold_reading_mask() const {
	configASSERT(this->validate());
	return (this->sdr_data[17]<<8) | this->sdr_data[16];
}
void SensorDataRecordReadableSensor::deassertion_upper_threshold_reading_mask(uint16_t val) {
	configASSERT(this->validate());
	this->sdr_data[17] = val >> 8;
	this->sdr_data[16] = val & 0xff;
}

uint16_t SensorDataRecordReadableSensor::discrete_reading_setable_threshold_reading_mask() const {
	configASSERT(this->validate());
	return (this->sdr_data[19]<<8) | this->sdr_data[18];
}
void SensorDataRecordReadableSensor::discrete_reading_setable_threshold_reading_mask(uint16_t val) {
	configASSERT(this->validate());
	this->sdr_data[19] = val >> 8;
	this->sdr_data[18] = val & 0xff;
}

SDR_FIELD(units_rate_unit, enum SensorDataRecordReadableSensor::UnitsRateUnit, 20, 5, 3)
SDR_FIELD(units_modifier_unit, enum SensorDataRecordReadableSensor::UnitsModifierUnit, 20, 2, 1)
SDR_FIELD(units_percentage, bool, 20, 0, 0)

uint8_t SensorDataRecordReadableSensor::hysteresis_high() const {
	configASSERT(0); // How'd you construct this object?  It should be virtual.
	return 0;
}
void SensorDataRecordReadableSensor::hysteresis_high(uint8_t val) {
	configASSERT(0); // How'd you construct this object?  It should be virtual.
}

uint8_t SensorDataRecordReadableSensor::hysteresis_low() const {
	configASSERT(0); // How'd you construct this object?  It should be virtual.
	return 0;
}
void SensorDataRecordReadableSensor::hysteresis_low(uint8_t val) {
	configASSERT(0); // How'd you construct this object?  It should be virtual.
}

const std::map<uint8_t, std::string> SensorDataRecordReadableSensor::sensor_unit_type_codes = {
		{ 0,  "unspecified"           },
		{ 1,  "degrees C"             },
		{ 2,  "degrees F"             },
		{ 3,  "degrees K"             },
		{ 4,  "Volts"                 },
		{ 5,  "Amps"                  },
		{ 6,  "Watts"                 },
		{ 7,  "Joules"                },
		{ 8,  "Coulombs"              },
		{ 9,  "VA"                    },
		{ 10, "Nits"                  },
		{ 11, "lumen"                 },
		{ 12, "lux"                   },
		{ 13, "Candela"               },
		{ 14, "kPa"                   },
		{ 15, "PSI"                   },
		{ 16, "Newton"                },
		{ 17, "CFM"                   },
		{ 18, "RPM"                   },
		{ 19, "Hz"                    },
		{ 20, "microsecond"           },
		{ 21, "millisecond"           },
		{ 22, "second"                },
		{ 23, "minute"                },
		{ 24, "hour"                  },
		{ 25, "day"                   },
		{ 26, "week"                  },
		{ 27, "mil"                   },
		{ 28, "inches"                },
		{ 29, "feet"                  },
		{ 30, "cu in"                 },
		{ 31, "cu feet"               },
		{ 32, "mm"                    },
		{ 33, "cm"                    },
		{ 34, "m"                     },
		{ 35, "cu cm"                 },
		{ 36, "cu m"                  },
		{ 37, "liters"                },
		{ 38, "fluid ounce"           },
		{ 39, "radians"               },
		{ 40, "steradians"            },
		{ 41, "revolutions"           },
		{ 42, "cycles"                },
		{ 43, "gravities"             },
		{ 44, "ounce"                 },
		{ 45, "pound"                 },
		{ 46, "ft-lb"                 },
		{ 47, "oz-in"                 },
		{ 48, "gauss"                 },
		{ 49, "gilberts"              },
		{ 50, "henry"                 },
		{ 51, "millihenry"            },
		{ 52, "farad"                 },
		{ 53, "microfarad"            },
		{ 54, "ohms"                  },
		{ 55, "siemens"               },
		{ 56, "mole"                  },
		{ 57, "becquerel"             },
		{ 58, "PPM (parts/million)"   },
		{ 59, "reserved"              },
		{ 60, "Decibels"              },
		{ 61, "DbA"                   },
		{ 62, "DbC"                   },
		{ 63, "gray"                  },
		{ 64, "sievert"               },
		{ 65, "color temp deg K"      },
		{ 66, "bit"                   },
		{ 67, "kilobit"               },
		{ 68, "megabit"               },
		{ 69, "gigabit"               },
		{ 70, "byte"                  },
		{ 71, "kilobyte"              },
		{ 72, "megabyte"              },
		{ 73, "gigabyte"              },
		{ 74, "word (data)"           },
		{ 75, "dword"                 },
		{ 76, "qword"                 },
		{ 77, "line (re. mem. line)"  },
		{ 78, "hit"                   },
		{ 79, "miss"                  },
		{ 80, "retry"                 },
		{ 81, "reset"                 },
		{ 82, "overrun / overflow"    },
		{ 83, "underrun"              },
		{ 84, "collision"             },
		{ 85, "packets"               },
		{ 86, "messages"              },
		{ 87, "characters"            },
		{ 88, "error"                 },
		{ 89, "correctable error"     },
		{ 90, "uncorrectable error"   },
		{ 91, "fatal error"           },
		{ 92, "grams"                 },
};