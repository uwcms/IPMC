/*
 * ThresholdSensor.cpp
 *
 *  Created on: Oct 19, 2018
 *      Author: jtikalsky
 */

#include <services/ipmi/sensor/ThresholdSensor.h>
#include <services/ipmi/sensor/Sensor.h>
#include <services/ipmi/sdr/SensorDataRecordSensor.h>
#include <services/ipmi/sdr/SensorDataRecordReadableSensor.h>
#include <services/ipmi/sdr/SensorDataRecord01.h>
#include <libs/printf.h>
#include <math.h>
#include <services/ipmi/ipmbsvc/ipmbsvc.h>
#include <services/ipmi/sdr/sensor_data_repository.h>
#include <cmath>
#include <core.h>

ThresholdSensor::ThresholdSensor(const std::vector<uint8_t> &sdr_key, LogTree &log)
	: Sensor(sdr_key, log), nominal_event_status_override_value(0xFFFF) {
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
	this->active_events = 0;
	this->event_context = 0;
	this->last_enabled_assertions = 0;
	this->last_enabled_deassertions = 0;
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
 * Performs the appropriate calculations for a given threshold, and generates
 * any relevant events.
 *
 * @param[in,out] state The current threshold state bitmask
 * @param[in] bit The number of the relevant bit in the threshold state bitmask (Going high/low is inferred from bit 0.)
 * @param[in] threshold The threshold value itself
 * @param[in] hysteresis The hysteresis for this threshold
 * @param[in] value The value to be compared
 * @param[out] events IPMI event data in "Platform Event Message" "Event Data" format.
 * @param[in] extra_assert send an assertion event regardless of current value
 * @param[in] extra_deassert send a deassertion event regardless of current value
 */
static void process_threshold(uint16_t &state, uint8_t bit, uint8_t threshold, uint8_t hysteresis, uint8_t value, std::vector<struct ThresholdEvent> &events, bool extra_assert, bool extra_deassert) {
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

	if (deassert || extra_deassert) {
		// Send deassertion event.
		struct ThresholdEvent event_data;
		event_data.direction = Sensor::EVENT_DEASSERTION;
		event_data.bit = bit;
		event_data.value = value;
		event_data.threshold = threshold;
		events.push_back(event_data);
	}
	if (assert || extra_assert) {
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
 * Perform all threshold processing on the supplied value and return the
 * resultant state.
 *
 * @param[in]  state The initial event state
 * @param[in]  event_context The event context to use when processing
 * @param[in]  thresholds The threshold configuration to use for processing
 * @param[in]  hystl Going-low hystersis
 * @param[in]  hysth Going-high hysteresis
 * @param[in]  byteval The value to process
 * @param[out] events A list of generated events
 * @param[in]  extra_assertions A mask of assertion events to send regardless of actual value
 * @param[in]  extra_deassertions A mask of deassertion events to send regardless of actual value
 * @return The final event state
 */
static uint16_t process_thresholds(uint16_t state, uint16_t event_context, const struct ThresholdSensor::threshold_configuration &thresholds, uint8_t hystl, uint8_t hysth, uint8_t byteval, std::vector<struct ThresholdEvent> &events, uint16_t extra_assertions, uint16_t extra_deassertions) {
	if (event_context & (1 << 0))
		process_threshold(state,  0, thresholds.lnc, hystl, byteval, events, extra_assertions & (1<<0), extra_deassertions & (1<<0));
	if (event_context & (1 << 1))
		process_threshold(state,  1, thresholds.lnc, hysth, byteval, events, extra_assertions & (1<<1), extra_deassertions & (1<<1));

	if (event_context & (1 << 2))
		process_threshold(state,  2, thresholds.lcr, hystl, byteval, events, extra_assertions & (1<<2), extra_deassertions & (1<<2));
	if (event_context & (1 << 3))
		process_threshold(state,  3, thresholds.lcr, hysth, byteval, events, extra_assertions & (1<<3), extra_deassertions & (1<<3));

	if (event_context & (1 << 4))
		process_threshold(state,  4, thresholds.lnr, hystl, byteval, events, extra_assertions & (1<<4), extra_deassertions & (1<<4));
	if (event_context & (1 << 5))
		process_threshold(state,  5, thresholds.lnr, hysth, byteval, events, extra_assertions & (1<<5), extra_deassertions & (1<<5));

	if (event_context & (1 << 6))
		process_threshold(state,  6, thresholds.unc, hystl, byteval, events, extra_assertions & (1<<6), extra_deassertions & (1<<6));
	if (event_context & (1 << 7))
		process_threshold(state,  7, thresholds.unc, hysth, byteval, events, extra_assertions & (1<<7), extra_deassertions & (1<<7));

	if (event_context & (1 << 8))
		process_threshold(state,  8, thresholds.ucr, hystl, byteval, events, extra_assertions & (1<<8), extra_deassertions & (1<<8));
	if (event_context & (1 << 9))
		process_threshold(state,  9, thresholds.ucr, hysth, byteval, events, extra_assertions & (1<<9), extra_deassertions & (1<<9));

	if (event_context & (1 << 10))
		process_threshold(state, 10, thresholds.unr, hystl, byteval, events, extra_assertions & (1<<10), extra_deassertions & (1<<10));
	if (event_context & (1 << 11))
		process_threshold(state, 11, thresholds.unr, hysth, byteval, events, extra_assertions & (1<<11), extra_deassertions & (1<<11));

	return state;
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
 * @param extra_assertions Send these event assertions regardless of the (non-NaN) value.
 * @param extra_deassertions Send these event deassertions regardless of the (non-NaN) value.
 *
 * @note For extra_(de)assertions, the following bitmask is interpreted:
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
void ThresholdSensor::update_value(const float value, uint16_t event_context, uint64_t value_max_age, uint16_t extra_assertions, uint16_t extra_deassertions) {
	MutexGuard<false> lock(this->value_mutex, true);
	this->last_value = value;

	uint64_t now = get_tick64();
	if (UINT64_MAX - value_max_age <= now)
		this->value_expiration = UINT64_MAX;
	else
		this->value_expiration = now + value_max_age;

	if (std::isnan(value)) {
		// If the value is expired or unavailable, treat all events as out of context.
		this->event_context = 0;
		return;
	}

	std::shared_ptr<const SensorDataRecordReadableSensor> sdr = std::dynamic_pointer_cast<const SensorDataRecordReadableSensor>(device_sdr_repo.find(this->sdr_key));
	if (!sdr) {
		this->logunique.logUnique(stdsprintf("Unable to locate a readable (Type 01/02) sensor %s in the Device SDR Repository!  Thresholds not updated!", this->sensor_identifier().c_str()), LogTree::LOG_ERROR);
		return;
	}
	if (sdr->event_type_reading_code() != SensorDataRecordSensor::EVENT_TYPE_THRESHOLD_SENSOR) {
		this->logunique.logUnique(stdsprintf("Sensor %s is not a Threshold type sensor in the Device SDR Repository!  Thresholds not updated!", this->sensor_identifier().c_str()), LogTree::LOG_ERROR);
		return;
	}

	const uint8_t byteval = sdr->from_float(value);

	std::shared_ptr<const SensorDataRecord01> sdr01 = std::dynamic_pointer_cast<const SensorDataRecord01>(sdr);
	this->update_thresholds_from_sdr(sdr01);

	// Automatic threshold calculation.
	const uint8_t hysth = sdr->hysteresis_high();
	const uint8_t hystl = sdr->hysteresis_low();

	event_context &= 0x0fff; // Limit to actual range.
	if (this->event_context != event_context) {
		// Events are going out of or coming into context or we're initializing.

		const uint16_t changed_bits = this->event_context ^ event_context;
		const uint16_t nominalize_bits = changed_bits | ~event_context;

		uint16_t nominal_event_status = 0x0a95; // A default nominal event status

		if (this->nominal_event_status_override_value != 0xFFFF) {
			nominal_event_status = this->nominal_event_status_override_value;

			this->log.log(stdsprintf("Sensor %s: Nominalizing events 0x%04hx (0x%04hx -> 0x%04hx) based on nominal event mask override value 0x%04hx.",
					this->sensor_identifier().c_str(),
					nominalize_bits, this->event_context, event_context,
					nominal_event_status
					), LogTree::LOG_DIAGNOSTIC);
		}
		else if (sdr01 && sdr01->nominal_reading_specified()) {
			// We have an SDR with a nominal value, so we'll calculate the REAL nominal event status.
			std::vector<struct ThresholdEvent> ignore_events;
			nominal_event_status = process_thresholds(0, 0x0fff, this->thresholds, hystl, hysth, sdr01->nominal_reading_rawvalue(), ignore_events, 0, 0);

			this->log.log(stdsprintf("Sensor %s: Nominalizing events 0x%04hx (0x%04hx -> 0x%04hx) based on nominal mask 0x%04hx @ 0x%02hhx (%f)",
					this->sensor_identifier().c_str(),
					nominalize_bits, this->event_context, event_context,
					nominal_event_status,
					sdr01->nominal_reading_rawvalue(),
					sdr01->to_float(sdr01->nominal_reading_rawvalue())
					), LogTree::LOG_DIAGNOSTIC);
		}

		this->event_context = event_context; // Update the event context.
		this->active_events &= ~nominalize_bits; // Clear all out of context event assertions.
		this->active_events |= nominalize_bits & nominal_event_status; // Assert any asserted-while-nominal out of context events.

		this->log.log(stdsprintf("Sensor %s: Outcome 0x%04hx, with Extras +0x%04hx -0x%04hx",
				this->sensor_identifier().c_str(),
				this->active_events,
				extra_assertions,
				extra_deassertions
				), LogTree::LOG_DIAGNOSTIC);
	}

	/* Don't process extra events that move toward the pre-existing state, but
	 * which are also not blips. (on and off in the same window).
	 *
	 * They'll get caught by the normal event processing, and this will
	 * keep us from getting waves of extra messages when sensors go into context.
	 */
	uint16_t extra_blips = extra_assertions & extra_deassertions;

	extra_assertions &= extra_blips | ~this->active_events;
	extra_deassertions &= extra_blips | this->active_events;

	std::vector<struct ThresholdEvent> events;
	this->active_events = process_thresholds(this->active_events, this->event_context, this->thresholds, hystl, hysth, byteval, events, extra_assertions, extra_deassertions);

	const uint16_t supported_assertion_mask = sdr->assertion_lower_threshold_reading_mask();
	const uint16_t supported_deassertion_mask = sdr->deassertion_upper_threshold_reading_mask();
	const uint16_t enabled_assertion_mask = this->assertion_events_enabled();
	const uint16_t enabled_deassertion_mask = this->deassertion_events_enabled();

	this->last_enabled_assertions = supported_assertion_mask & enabled_assertion_mask;
	this->last_enabled_deassertions = supported_deassertion_mask & enabled_deassertion_mask;

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

	for (std::vector<struct ThresholdEvent>::iterator it = events.begin(), eit = events.end(); it != eit; ++it) {
		if ( !(this->event_context & (1 << it->bit)) ) {
			this->log.log(stdsprintf("Sensor %s: %s %s event for value 0x%02hhx (%f), threshold 0x%02hhx is out of context and will not be sent",
					this->sensor_identifier().c_str(),
					threshold_names.at(it->bit).c_str(), (it->direction == Sensor::EVENT_ASSERTION ? "assertion" : "deassertion"),
					it->value, value, it->threshold), LogTree::LOG_DIAGNOSTIC);
		}
		else if (
				(it->direction == Sensor::EVENT_ASSERTION && !(supported_assertion_mask & (1 << it->bit))) ||
				(it->direction == Sensor::EVENT_DEASSERTION && !(supported_deassertion_mask & (1 << it->bit)))
				) {
			this->log.log(stdsprintf("Sensor %s: %s %s event for value 0x%02hhx (%f), threshold 0x%02hhx is specified as unsupported in the SDR and will not be sent",
					this->sensor_identifier().c_str(),
					threshold_names.at(it->bit).c_str(), (it->direction == Sensor::EVENT_ASSERTION ? "assertion" : "deassertion"),
					it->value, value, it->threshold), LogTree::LOG_DIAGNOSTIC);
		}
		else if (
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
 * Override the auto-calculated nominal event status mask.
 *
 * This value is used to initialize event bits that come into context or are
 * rearmed while it is set.
 *
 * @param mask The new mask to set, or 0xFFFF to clear the override.
 */
void ThresholdSensor::nominal_event_status_override(uint16_t mask) {
	MutexGuard<false> lock(this->value_mutex, true);
	this->nominal_event_status_override_value = mask;
}

/**
 * Retrieve the override value for the nominal event status mask.
 *
 * This value is used to initialize event bits that come into context or are
 * rearmed while it is set.
 *
 * @return The override mask, or 0xFFFF if disabled.
 */
uint16_t ThresholdSensor::nominal_event_status_override() {
	MutexGuard<false> lock(this->value_mutex, true);
	return this->nominal_event_status_override_value;
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
	value.active_events = this->active_events;
	value.event_context = this->event_context;
	value.enabled_assertions = this->last_enabled_assertions;
	value.enabled_deassertions = this->last_enabled_deassertions;
	if (now >= this->value_expiration) {
		value.float_value = NAN;
		value.active_events = 0;
		value.event_context = 0;
		value.enabled_assertions = 0;
		value.enabled_deassertions = 0;
	}
	lock.release();
	if (std::isnan(value.float_value))
		return value;
	std::shared_ptr<const SensorDataRecordSensor> sdr = std::dynamic_pointer_cast<const SensorDataRecordSensor>(device_sdr_repo.find(this->sdr_key));
	if (!sdr) {
		this->logunique.logUnique(stdsprintf("Unable to locate sensor %s in the Device SDR Repository!", this->sensor_identifier().c_str()), LogTree::LOG_ERROR);
		return value; // No exceptions available, so we'll send what we have, and byte_value can be something blatantly bad.
	}
	std::shared_ptr<const SensorDataRecordReadableSensor> sdr_readable = std::dynamic_pointer_cast<const SensorDataRecordReadableSensor>(sdr);
	if (!sdr_readable) {
		this->logunique.logUnique(stdsprintf("Sensor %s is not a readable (Type 01/02) sensor in the Device SDR Repository!", this->sensor_identifier().c_str()), LogTree::LOG_ERROR);
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
		*reading_good = !std::isnan(value.float_value);
	return value.active_events;
}

void ThresholdSensor::rearm() {
	/* Clearing our value and IPMI events such that the next update will do
	 * everything that is needed of us.
	 */
	MutexGuard<false> lock(this->value_mutex, true);
	this->last_value = NAN;
	this->value_expiration = UINT64_MAX;
	this->active_events = 0;
	this->event_context = 0; // We'll put this out of context so its active events are reinitialized to nominal rather than spewing deasserts on SER/etc.
	lock.release();
	this->log.log(stdsprintf("Sensor %s rearmed!", this->sensor_identifier().c_str()), LogTree::LOG_INFO);
}
