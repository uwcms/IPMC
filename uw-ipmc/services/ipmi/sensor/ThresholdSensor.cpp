/*
 * ThresholdSensor.cpp
 *
 *  Created on: Oct 19, 2018
 *      Author: jtikalsky
 */

#include <services/ipmi/sensor/ThresholdSensor.h>
#include <services/ipmi/sensor/Sensor.h>
#include <services/ipmi/sdr/SensorDataRepository.h>
#include <services/ipmi/sdr/SensorDataRecordSensor.h>
#include <services/ipmi/sdr/SensorDataRecordReadableSensor.h>
#include <services/ipmi/sdr/SensorDataRecord01.h>
#include <services/ipmi/ipmbsvc/IPMBSvc.h>
#include <libs/printf.h>
#include <IPMC.h>
#include <math.h>
#include <cmath>

ThresholdSensor::ThresholdSensor(const std::vector<uint8_t> &sdr_key, LogTree &log)
	: Sensor(sdr_key, log) {
	this->value_mutex = xSemaphoreCreateMutex();
	std::shared_ptr<const SensorDataRecord01> sdr01 = std::dynamic_pointer_cast<const SensorDataRecord01>(device_sdr_repo.find(this->sdr_key));
	if (sdr01) {
		// Update our initial thresholds from our Type 01 SDR data.
		this->thresholds.lnc = sdr01->threshold_lnc_rawvalue();
		this->thresholds.lcr = sdr01->threshold_lcr_rawvalue();
		this->thresholds.lnr = sdr01->threshold_lnr_rawvalue();
		this->thresholds.unc = sdr01->threshold_unc_rawvalue();
		this->thresholds.ucr = sdr01->threshold_ucr_rawvalue();
		this->thresholds.unr = sdr01->threshold_unr_rawvalue();
	}
	else {
		// Apply defaults, and let the user fill them in after initialization.
		this->thresholds.lnc = 0x00;
		this->thresholds.lcr = 0x00;
		this->thresholds.lnr = 0x00;
		this->thresholds.unc = 0xFF;
		this->thresholds.ucr = 0xFF;
		this->thresholds.unr = 0xFF;
	}
	this->last_value = NAN;
	this->value_expiration = UINT64_MAX;
	this->active_thresholds = 0;
	this->in_context = false;
}

ThresholdSensor::~ThresholdSensor() {
	vSemaphoreDelete(this->value_mutex);
}

/**
 * An internal descriptor of a threshold event, to be provided by threshold
 * comparators for use in generating events.
 */
struct ThresholdEvent {
	enum Sensor::EventDirection direction; ///< The event direction.
	uint8_t bit; ///< The event offset (bit number)
	uint8_t value; ///< The value triggering the event.
	uint8_t threshold; ///< The threshold triggering the event.
};

/**
 * Performs the appropriate calculations for a given threshold, and generates
 * any relevant events.
 *
 * @param[in,out] state The current threshold state bitmask
 * @param[in] bit The number of the relevant bit in the threshold state bitmask (Going high/low is inferred from bit 0.)
 * @param[in] threshold The threshold value itself
 * @param[in] hysteresis The hysteresis for this threshold
 * @param[in] value The value to be compared
 * @param[out] events IPMI event data in "Platform Event Message" "Event Data" format.
 * @param[in] force_assert send an assertion event regardless of current value
 * @param[in] force_deassert send a deassertion event regardless of current value
 */
static void process_threshold(uint16_t &state, uint8_t bit, uint8_t threshold, uint8_t hysteresis, uint8_t value, std::vector<struct ThresholdEvent> &events, bool force_assert, bool force_deassert) {
	bool assert = false, deassert = false;

	const int intthresh = static_cast<int>(threshold);
	const int intval = static_cast<int>(value);

	const bool going_high = bit & 1;

	if (going_high) {
		if (intval >= intthresh)
			assert = true;
		if (intval < intthresh - static_cast<int>(hysteresis))
			deassert = true;
	}
	else {
		if (intval <= intthresh)
			assert = true;
		if (intval > intthresh + static_cast<int>(hysteresis))
			deassert = true;
	}

	const uint16_t old_state = state;
	if (old_state & (1<<bit))
		assert = false; // Already asserted, no changes to make. (force is later)
	else
		deassert = false; // Already deasserted, no changes to make. (force is later)

	if (assert)
		state |= (1<<bit);
	if (deassert)
		state &= ~(1<<bit);

	if (deassert || force_deassert) {
		// Send deassertion event.
		struct ThresholdEvent event_data;
		event_data.direction = Sensor::EVENT_DEASSERTION;
		event_data.bit = bit;
		event_data.value = value;
		event_data.threshold = threshold;
		events.push_back(event_data);
	}
	if (assert || force_assert) {
		// Send the appropriate event.
		struct ThresholdEvent event_data;
		event_data.direction = Sensor::EVENT_ASSERTION;
		event_data.bit = bit;
		event_data.value = value;
		event_data.threshold = threshold;
		events.push_back(event_data);
	}
}

/**
 * Update thresholds in local cache from the SDR if present.
 * @param sdr01 The sdr to use.  It must be a type 01 SDR or NULL for NOOP
 */
void ThresholdSensor::update_thresholds_from_sdr(std::shared_ptr<const SensorDataRecord01> sdr01) {
	if (sdr01) {
		// Update our thresholds from SDR data.
		this->thresholds.lnc = sdr01->threshold_lnc_rawvalue();
		this->thresholds.lcr = sdr01->threshold_lcr_rawvalue();
		this->thresholds.lnr = sdr01->threshold_lnr_rawvalue();
		this->thresholds.unc = sdr01->threshold_unc_rawvalue();
		this->thresholds.ucr = sdr01->threshold_ucr_rawvalue();
		this->thresholds.unr = sdr01->threshold_unr_rawvalue();
	}
}

/**
 * Update the internal state of this sensor with the provided float value and
 * generate any relevant events based on this.
 *
 * If the provided value is NAN, the sensor is presumed to be out of context,
 * there is considered to be no reading available, and event processing is
 * suspended.
 *
 * @param value The new sensor value
 * @param in_context True if this sensor is in context for this update. (Out of
 *                   context sensors will not send or process events.)
 * @param value_max_age The number of ticks after which the value should be
 *                      considered to be out of date and should be replaced with
 *                      NAN.
 * @param force_assertions Force these event assertions regardless of the (non-NaN) value.
 * @param force_deassertions Force these event deassertions regardless of the (non-NaN) value.
 *
 * @note For force_(de)assertions, the following bitmask is interpreted:
 *       bit 11: 1b = upper non-recoverable going high occurred
 *       bit 10: 1b = upper non-recoverable going low occurred
 *       bit  9: 1b = upper critical going high occurred
 *       bit  8: 1b = upper critical going low occurred
 *       bit  7: 1b = upper non-critical going high occurred
 *       bit  6: 1b = upper non-critical going low occurred
 *       bit  5: 1b = lower non-recoverable going high occurred
 *       bit  4: 1b = lower non-recoverable going low occurred
 *       bit  3: 1b = lower critical going high occurred
 *       bit  2: 1b = lower critical going low occurred
 *       bit  1: 1b = lower non-critical going high occurred
 *       bit  0: 1b = lower non-critical going low occurred
 */
void ThresholdSensor::update_value(const float value, bool in_context, uint64_t value_max_age, uint16_t force_assertions, uint16_t force_deassertions) {
	MutexGuard<false> lock(this->value_mutex, true);
	this->last_value = value;

	uint64_t now = get_tick64();
	if (UINT64_MAX - value_max_age <= now)
		this->value_expiration = UINT64_MAX;
	else
		this->value_expiration = now + value_max_age;

	this->in_context = in_context;

	if (std::isnan(value) || !in_context)
		return;

	std::shared_ptr<const SensorDataRecordReadableSensor> sdr = std::dynamic_pointer_cast<const SensorDataRecordReadableSensor>(device_sdr_repo.find(this->sdr_key));
	if (!sdr) {
		this->logunique.log_unique(stdsprintf("Unable to locate a readable (Type 01/02) sensor %s in the Device SDR Repository!  Thresholds not updated!", this->sensor_identifier().c_str()), LogTree::LOG_ERROR);
		return;
	}
	if (sdr->event_type_reading_code() != SensorDataRecordSensor::EVENT_TYPE_THRESHOLD_SENSOR) {
		this->logunique.log_unique(stdsprintf("Sensor %s is not a Threshold type sensor in the Device SDR Repository!  Thresholds not updated!", this->sensor_identifier().c_str()), LogTree::LOG_ERROR);
		return;
	}

	const uint8_t byteval = sdr->from_float(value);

	this->update_thresholds_from_sdr(std::dynamic_pointer_cast<const SensorDataRecord01>(sdr));

	// Automatic threshold calculation.
	const uint8_t hysth = sdr->hysteresis_high();
	const uint8_t hystl = sdr->hysteresis_low();

	std::vector<struct ThresholdEvent> events;

	process_threshold(this->active_thresholds,  0, this->thresholds.lnc, hystl, byteval, events, force_assertions & (1<<0), force_deassertions & (1<<0));
	process_threshold(this->active_thresholds,  1, this->thresholds.lnc, hysth, byteval, events, force_assertions & (1<<1), force_deassertions & (1<<1));

	process_threshold(this->active_thresholds,  2, this->thresholds.lcr, hystl, byteval, events, force_assertions & (1<<2), force_deassertions & (1<<2));
	process_threshold(this->active_thresholds,  3, this->thresholds.lcr, hysth, byteval, events, force_assertions & (1<<3), force_deassertions & (1<<3));

	process_threshold(this->active_thresholds,  4, this->thresholds.lnr, hystl, byteval, events, force_assertions & (1<<4), force_deassertions & (1<<4));
	process_threshold(this->active_thresholds,  5, this->thresholds.lnr, hysth, byteval, events, force_assertions & (1<<5), force_deassertions & (1<<5));

	process_threshold(this->active_thresholds,  6, this->thresholds.unc, hystl, byteval, events, force_assertions & (1<<6), force_deassertions & (1<<6));
	process_threshold(this->active_thresholds,  7, this->thresholds.unc, hysth, byteval, events, force_assertions & (1<<7), force_deassertions & (1<<7));

	process_threshold(this->active_thresholds,  8, this->thresholds.ucr, hystl, byteval, events, force_assertions & (1<<8), force_deassertions & (1<<8));
	process_threshold(this->active_thresholds,  9, this->thresholds.ucr, hysth, byteval, events, force_assertions & (1<<9), force_deassertions & (1<<9));

	process_threshold(this->active_thresholds, 10, this->thresholds.unr, hystl, byteval, events, force_assertions & (1<<10), force_deassertions & (1<<10));
	process_threshold(this->active_thresholds, 11, this->thresholds.unr, hysth, byteval, events, force_assertions & (1<<11), force_deassertions & (1<<11));

	lock.release();

	static const std::vector<std::string> threshold_names{
		"LNC going-low",
		"LNC going-high",
		"LCR going-low",
		"LCR going-high",
		"LNR going-low",
		"LNR going-high",
		"UNC going-low",
		"UNC going-high",
		"UCR going-low",
		"UCR going-high",
		"UNR going-low",
		"UNR going-high",
	};

	const uint16_t supported_assertion_mask = sdr->assertion_lower_threshold_reading_mask();
	const uint16_t supported_deassertion_mask = sdr->deassertion_upper_threshold_reading_mask();
	const uint16_t enabled_assertion_mask = sdr->ext_assertion_events_enabled();
	const uint16_t enabled_deassertion_mask = sdr->ext_deassertion_events_enabled();
	for (std::vector<struct ThresholdEvent>::iterator it = events.begin(), eit = events.end(); it != eit; ++it) {
		if (
				(it->direction == Sensor::EVENT_ASSERTION && !(supported_assertion_mask & (1 << it->bit))) ||
				(it->direction == Sensor::EVENT_DEASSERTION && !(supported_deassertion_mask & (1 << it->bit)))
				) {
			this->log.log(stdsprintf("Sensor %s: %s %s event for value 0x%02hhx (%f), threshold 0x%02hhx is specified as unsupported in the SDR and will not be sent",
					this->sensor_identifier().c_str(),
					threshold_names.at(it->bit).c_str(), (it->direction == Sensor::EVENT_ASSERTION ? "assertion" : "deassertion"),
					it->value, value, it->threshold), LogTree::LOG_DIAGNOSTIC);
		}
		if (
				(it->direction == Sensor::EVENT_ASSERTION && !(enabled_assertion_mask & (1 << it->bit))) ||
				(it->direction == Sensor::EVENT_DEASSERTION && !(enabled_deassertion_mask & (1 << it->bit)))
				) {
			this->log.log(stdsprintf("Sensor %s: %s %s event for value 0x%02hhx (%f), threshold 0x%02hhx is configured as disabled and will not be sent",
					this->sensor_identifier().c_str(),
					threshold_names.at(it->bit).c_str(), (it->direction == Sensor::EVENT_ASSERTION ? "assertion" : "deassertion"),
					it->value, value, it->threshold), LogTree::LOG_DIAGNOSTIC);
		}
		else {
			this->log.log(stdsprintf("Sensor %s: Sending %s %s event for value 0x%02hhx (%f), threshold 0x%02hhx",
					this->sensor_identifier().c_str(),
					threshold_names.at(it->bit).c_str(), (it->direction == Sensor::EVENT_ASSERTION ? "assertion" : "deassertion"),
					it->value, value, it->threshold), LogTree::LOG_DIAGNOSTIC);

			std::vector<uint8_t> event_data;
			event_data.push_back(0x50 | it->bit);
			event_data.push_back(it->value);
			event_data.push_back(it->threshold);
			this->send_event(it->direction, event_data);
		}
	}

	this->logunique.clean();
}

/**
 * Get the current values and thresholds for this sensor.
 *
 * \note byte_value may be 0xFF if there is no SDR available to interpret it,
 *       but this is also a valid value.  If no value is available, float_value
 *       will be NAN.
 *
 * @return The current values & thresholds.
 */
ThresholdSensor::Value ThresholdSensor::get_value() const {
	Value value;
	MutexGuard<false> lock(this->value_mutex, true);
	uint64_t now = get_tick64();
	value.float_value = this->last_value;
	value.byte_value = 0xFF;
	value.active_thresholds = this->active_thresholds;
	value.in_context = this->in_context;
	if (now >= this->value_expiration) {
		value.float_value = NAN;
		value.active_thresholds = 0;
	}
	lock.release();
	if (std::isnan(value.float_value))
		return value;
	std::shared_ptr<const SensorDataRecordSensor> sdr = std::dynamic_pointer_cast<const SensorDataRecordSensor>(device_sdr_repo.find(this->sdr_key));
	if (!sdr) {
		this->logunique.log_unique(stdsprintf("Unable to locate sensor %s in the Device SDR Repository!", this->sensor_identifier().c_str()), LogTree::LOG_ERROR);
		return value; // No exceptions available, so we'll send what we have, and byte_value can be something blatantly bad.
	}
	std::shared_ptr<const SensorDataRecordReadableSensor> sdr_readable = std::dynamic_pointer_cast<const SensorDataRecordReadableSensor>(sdr);
	if (!sdr_readable) {
		this->logunique.log_unique(stdsprintf("Sensor %s is not a readable (Type 01/02) sensor in the Device SDR Repository!", this->sensor_identifier().c_str()), LogTree::LOG_ERROR);
		return value; // No exceptions available, so we'll send what we have, and byte_value can be something blatantly bad.
	}
	value.byte_value = sdr_readable->from_float(value.float_value);
	return value;
}

std::vector<uint8_t> ThresholdSensor::get_sensor_reading() {
	Value value = this->get_value();
	std::vector<uint8_t> out;
	out.push_back(IPMI::Completion::Success);
	out.push_back(value.byte_value);
	out.push_back(
			(this->all_events_disabled() ? 0 : 0x80) |
			(this->sensor_scanning_disabled() ? 0 : 0x40) |
			(std::isnan(value.float_value) ? 0x20 : 0)
			);
	out.push_back(
			(value.byte_value >= this->thresholds.unr ? 0x20 : 0) |
			(value.byte_value >= this->thresholds.ucr ? 0x10 : 0) |
			(value.byte_value >= this->thresholds.unc ? 0x08 : 0) |
			(value.byte_value <= this->thresholds.lnr ? 0x04 : 0) |
			(value.byte_value <= this->thresholds.lcr ? 0x02 : 0) |
			(value.byte_value <= this->thresholds.lnc ? 0x01 : 0)
			);
	return out;
}

uint16_t ThresholdSensor::get_sensor_event_status(bool *reading_good) {
	Value value = this->get_value();
	if (reading_good)
		*reading_good = value.in_context && !std::isnan(value.float_value);
	return value.active_thresholds;
}

void ThresholdSensor::rearm() {
	/* Clearing our value and IPMI events such that the next update will do
	 * everything that is needed of us.
	 */
	MutexGuard<false> lock(this->value_mutex, true);
	this->last_value = NAN;
	this->value_expiration = UINT64_MAX;
	this->active_thresholds = 0;
	lock.release();
	this->log.log(stdsprintf("Sensor %s rearmed!", this->sensor_identifier().c_str()), LogTree::LOG_INFO);
}
