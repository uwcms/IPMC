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

#include <core.h>
#include <libs/printf.h>
#include <services/ipmi/sensor/SensorSet.h>
#include <services/ipmi/sensor/ThresholdSensor.h>
#include <exception>
#include <libs/threading.h>
#include <payload_manager.h>
#include <services/console/consolesvc.h>
#include <services/ipmi/sdr/sensor_data_record_readable_sensor.h>
#include <services/persistentstorage/persistent_storage.h>

#define MZ_HOLDOFF_TICKS 140
#define CONTEXT_EVENT_MASK 0xfc0 // mask only lower events
#define OOC_NOMINAL_EVENT_STATUS 0x555 // When "out of context", presume the sensor value is zero, below all thresholds

SemaphoreHandle_t LinkDescriptor::oem_guid_mutex = nullptr;
std::map< uint8_t, std::vector<uint8_t> > LinkDescriptor::oem_guids;
std::map<std::string, PayloadManager::ADCSensor> PayloadManager::adc_sensors;

LinkDescriptor::LinkDescriptor() :
		enabled(false), LinkGroupingID(0), LinkTypeExtension(0), LinkType(0),
		IncludedPorts(0), Interface(IF_RESERVED), ChannelNumber(0) {
}

LinkDescriptor::LinkDescriptor(
		uint8_t LinkGroupingID,
		uint8_t LinkTypeExtension,
		uint8_t LinkType,
		uint8_t IncludedPorts,
		enum Interfaces Interface,
		uint8_t ChannelNumber) :
		enabled(false), LinkGroupingID(LinkGroupingID),
		LinkTypeExtension(LinkTypeExtension), LinkType(LinkType),
		IncludedPorts(IncludedPorts), Interface(Interface),
		ChannelNumber(ChannelNumber) {
}

LinkDescriptor::LinkDescriptor(const std::vector<uint8_t> &bytes, bool enabled) :
	enabled(enabled) {
	if (bytes.size() < 4) {
		throw std::domain_error("A Link Descriptor must be a four byte field.");
	}

	this->LinkGroupingID = bytes[3];
	this->LinkTypeExtension = bytes[2] >> 4;
	this->LinkType = ((bytes[2] & 0x0F) << 4) | ((bytes[1] & 0xF0) >> 4);
	this->IncludedPorts = bytes[1] & 0x0F;
	this->Interface = static_cast<Interfaces>(bytes[0] >> 6);
	this->ChannelNumber = bytes[0] & 0x3F;
}

LinkDescriptor::operator std::vector<uint8_t>() const {
	std::vector<uint8_t> out = { 0, 0, 0, 0 };
	out[0] |= (this->ChannelNumber & 0x3F) << 0;
	out[0] |= (this->Interface & 0x03) << 6;
	out[1] |= (this->IncludedPorts & 0x0F) << 0;
	out[1] |= (this->LinkType & 0x0F) << 4;
	out[2] |= (this->LinkType & 0xF0) >> 4;
	out[2] |= (this->LinkTypeExtension & 0x0F) << 4;
	out[3] = this->LinkGroupingID;
	return out;
}

uint8_t LinkDescriptor::map_oem_LinkType_guid(const std::vector<uint8_t> &oem_guid) {
	if (oem_guid.size() != 16) {
		throw std::domain_error("OEM LinkType GUIDs are 16 byte values.");
	}

	safe_init_static_mutex(oem_guid_mutex, false);
	MutexGuard<false> lock(oem_guid_mutex, true);
	for (auto it = oem_guids.begin(), eit = oem_guids.end(); it != eit; ++it) {
		if (it->second == oem_guid) {
			return it->first;
		}
	}

	// Or if not found, attempt to register.
	for (uint8_t mapping = 0xF0; mapping < 0xFF; ++mapping) {
			if (oem_guids.count(mapping)) continue;

			oem_guids[mapping] = oem_guid;
			return mapping;
	}

	// Or, if there was simply no room left:
	throw std::out_of_range("No remaining OEM LinkType GUID slots available. (Only 15 can be specified in FRU Data, by §3.7.2.3 ¶318)");
}

std::vector<uint8_t> LinkDescriptor::lookup_oem_LinkType_guid(uint8_t LinkType) {
	safe_init_static_mutex(oem_guid_mutex, false);
	MutexGuard<false> lock(oem_guid_mutex, true);
	return oem_guids.at(LinkType);
}

PayloadManager::PayloadManager(MStateMachine *mstate_machine, LogTree &log)
	: mstate_machine(mstate_machine), log(log), sp_context_update_timer(nullptr) {
	configASSERT(this->mutex = xSemaphoreCreateRecursiveMutex());

	struct IPMILED::Action ledstate;
	ledstate.min_duration = 0;
	ledstate.effect = IPMILED::OFF;
	ipmi_leds[2]->submit(ledstate);
}

void PayloadManager::finishConfig() {
	std::vector<ADC::Channel*> sensor_processor_channels(XPAR_IPMI_SENSOR_PROC_0_SENSOR_CNT);
	for (auto &adcsensor : PayloadManager::adc_sensors) {
		if (adcsensor.second.sensor_processor_id >= 0) {
			sensor_processor_channels[adcsensor.second.sensor_processor_id] = &adcsensor.second.adc;
		}
	}

	// TODO: Maybe this can't be common, check later.
	this->sensor_processor = new SensorProcessor(XPAR_IPMI_SENSOR_PROC_0_DEVICE_ID, XPAR_FABRIC_IPMI_SENSOR_PROC_0_IRQ_O_INTR, sensor_processor_channels);

	// The sensor processor is configured and enabled by the sensor linkage
	// update, as this is where thresholds become available.

	runTask("pyld_adcsensors", TASK_PRIORITY_SERVICE, [this]()->void{this->runSensorThread();});
}

PayloadManager::~PayloadManager() {
	/* We definitely want to kill all zones as simultaneously as possible, and
	 * the "kill zone" operation is just a single register write, therefore
	 * critical section.
	 */
	CriticalGuard critical(true);
	for (int i = 0; i < XPAR_MGMT_ZONE_CTRL_0_MZ_CNT; ++i) {
		this->mgmt_zones[i]->setPowerState(ZoneController::Zone::KILL);
		delete this->mgmt_zones[i];
	}

	SuspendGuard suspend(true);
	this->mstate_machine->deactivate_payload = nullptr;
	suspend.release();

	configASSERT(0); // TODO: We don't have a clean way to shut down the sensor thread.

	vSemaphoreDelete(this->mutex);
}

void PayloadManager::updateLinkEnable(const LinkDescriptor &descriptor) {
	MutexGuard<true> lock(this->mutex, true);
	for (auto it = this->links.begin(), eit = this->links.end(); it != eit; ++it) {
		if (*it == descriptor && it->enabled != descriptor.enabled) {
			it->enabled = descriptor.enabled;

			// A new link was enabled (or disabled), (de?)activate it!

			/* We are ignoring E-Keying, in the CDB edition of this code, so
			 * nothing will happen here, but we could notify a processor that
			 * the link is available, or hesitate to actually power one up
			 * before a link that it uses unconditionally is confirmed.
			 */
			if (it->enabled) {
				this->log.log(stdsprintf("E-Keying port enabled on Interface %hhu, Channel %hhu.", static_cast<uint8_t>(it->Interface), it->ChannelNumber), LogTree::LOG_INFO);
			} else {
				this->log.log(stdsprintf("E-Keying port disabled on Interface %hhu, Channel %hhu.", static_cast<uint8_t>(it->Interface), it->ChannelNumber), LogTree::LOG_INFO);
			}
		}
	}
}

std::vector<LinkDescriptor> PayloadManager::getLinks() const {
	MutexGuard<true> lock(this->mutex, true);
	return this->links;
}

void PayloadManager::runSensorThread() {
	while (true) {
		// Wait for scheduled or priority execution, then read any events.
		TickType_t timeout = pdMS_TO_TICKS(100);
		bool event_received;
		do {
			SensorProcessor::Event event;
			event_received = this->sensor_processor->getISREvent(event, timeout);
			if (event_received) {
				MutexGuard<true> lock(this->mutex, true);
				for (auto &adcsensor : this->adc_sensors) {
					if (adcsensor.second.sensor_processor_id >= 0 && (uint32_t)adcsensor.second.sensor_processor_id == event.channel) {
						std::shared_ptr<ThresholdSensor> ipmisensor = adcsensor.second.ipmi_sensor.lock();
						if (!ipmisensor) {
							this->log.log(stdsprintf("Sensor %s (Proc[%d]) has no matching Sensor object (are SDRs configured correctly?), not processing received event for this sensor.", adcsensor.second.name.c_str(), adcsensor.second.sensor_processor_id), LogTree::LOG_ERROR);
							continue;
						}
						bool in_context = this->isMZInContext(adcsensor.second.mz_context);
						ipmisensor->nominal_event_status_override(in_context ? 0xFFFF /* disable override */ : OOC_NOMINAL_EVENT_STATUS);
						ipmisensor->log.log(stdsprintf("Sensor Processor event for %s at reading %f: +0x%04x -0x%04x", ipmisensor->sensor_identifier().c_str(), adcsensor.second.adc.rawToFloat(event.reading_from_isr), event.event_thresholds_assert, event.event_thresholds_deassert), LogTree::LOG_DIAGNOSTIC);
						ipmisensor->update_value(adcsensor.second.adc.rawToFloat(event.reading_from_isr), (in_context ? 0xfff : CONTEXT_EVENT_MASK), UINT64_MAX, event.event_thresholds_assert, event.event_thresholds_deassert);
					}
				}
			}
			timeout = 0; // Now to process any remaining queued events promptly before proceeding on.
		} while (event_received);

		enum SeveritySensor::Level alarm_level = SeveritySensor::OK;
		{
			MutexGuard<true> lock(this->mutex, true);

			// If we made it to NR, we faulted, and we're staying in NR.
			if (this->alarmlevel_sensor && this->alarmlevel_sensor->get_raw_severity_level() == SeveritySensor::NR) {
				alarm_level = SeveritySensor::NR;
			}

			// If we are in M1 or at power level 0, and fault lock was cleared, we're returning to OK.
			if ((this->mstate_machine->mstate() == 1 || this->power_properties.current_power_level == 0) && !mstate_machine->fault_lock()) {
				alarm_level = SeveritySensor::OK;
			}
		}

		// Alert on fault transitions.
		{
			MutexGuard<true> lock(this->mutex, true);
			for (unsigned i = 0; i < XPAR_MGMT_ZONE_CTRL_0_MZ_CNT; ++i) {
				bool state = false, transition = false;
				state = this->mgmt_zones[i]->getPowerState(&transition);
				if (transition) continue;

				if (state != this->mgmt_zones[i]->getDesiredPowerState()) {
					this->log.log(stdsprintf("Management Zone %u has faulted!  The global power enable state is %lu at time of software processing.", i, this->mgmt_zones[i]->getPowerEnableStatus(false)), LogTree::LOG_ERROR);
					// Acknowledge the fault by setting desired state to off.
					this->mgmt_zones[i]->setPowerState(ZoneController::Zone::OFF);
					alarm_level = SeveritySensor::NR;
					// Set the fault lock flag in the MStateMachine so we can't go M1->M2 without the handle going out first.
					this->mstate_machine->fault_lock(true);
				}
			}
			/* We did this before any standard sensor processing, so we won't
			 * report soft-faults derived from that until next cycle.  This is
			 * fine, however, since only temperature sensors are soft faulting.
			 */
		}

		MutexGuard<true> lock(this->mutex, true);
		for (auto &adcsensor : this->adc_sensors) {
			std::shared_ptr<ThresholdSensor> ipmisensor = adcsensor.second.ipmi_sensor.lock();
			if (!ipmisensor) continue; // Can't update an unlinked sensor.

			float reading = (float)adcsensor.second.adc.readFloat();
			bool in_context = this->isMZInContext(adcsensor.second.mz_context);
			ipmisensor->nominal_event_status_override(in_context ? 0xFFFF /* disable override */ : OOC_NOMINAL_EVENT_STATUS);
			//ipmisensor->log.log(stdsprintf("Standard ADC read for %s shows %f %s context.", ipmisensor->sensor_identifier().c_str(), reading, (in_context ? "in" : "out of")), LogTree::LOG_TRACE);
			ipmisensor->update_value(reading, (in_context ? 0xfff : CONTEXT_EVENT_MASK));

			ThresholdSensor::Value value = ipmisensor->get_value();
			uint16_t active_events = value.active_events & value.event_context & value.enabled_assertions;

			if ((active_events & 0x081) && alarm_level < SeveritySensor::NC) {
				alarm_level = SeveritySensor::NC;
			}

			if ((active_events & 0x204) && alarm_level < SeveritySensor::CR) {
				alarm_level = SeveritySensor::CR;
			}

			if ((active_events & 0x810) && alarm_level < SeveritySensor::NR) {
				alarm_level = SeveritySensor::NR;
			}

			if (active_events & 0x810) {
				/* We have NR events on this sensor, and must fault any zones it is part of.
				 *
				 * This will cover the case where we hit NR from a software check
				 * on a sensor that has not hit NR on the firmware check due to
				 * the difference in software vs firmware resolutions.
				 *
				 * This will allow us to keep a slightly more consistent state.
				 *
				 * i.e. If the threshold is met and the alarm sensor is NR, we
				 * will ensure the relevant backend has been killed, even if we
				 * didn't quite make it to the exact value of the higher
				 * resolution firmware setting.
				 */
				for (int i = 0; i < XPAR_MGMT_ZONE_CTRL_0_MZ_CNT; ++i) {
					if (adcsensor.second.sensor_processor_id >= 0 &&
							this->mz_hf_vectors[i] & (1 << adcsensor.second.sensor_processor_id)) {
						this->mgmt_zones[i]->setPowerState(ZoneController::Zone::KILL); // Issue software fault.
					} else {
						this->processNonManagedADCSensor(adcsensor);
					}
				}
			}
		}

		if (this->alarmlevel_sensor) {
			SeveritySensor::Level old_level = this->alarmlevel_sensor->get_raw_severity_level();
			this->alarmlevel_sensor->transition(alarm_level);

			if (alarm_level != old_level) {
				struct IPMILED::Action ledstate;
				ledstate.min_duration = 0;
				ledstate.period_ms = 1000;

				switch (alarm_level) {
				case SeveritySensor::OK:
					ledstate.effect = IPMILED::OFF;
					break;
				case SeveritySensor::NC:
					ledstate.effect = IPMILED::BLINK;
					ledstate.time_on_ms = 100;
					break;
				case SeveritySensor::CR:
					ledstate.effect = IPMILED::BLINK;
					ledstate.time_on_ms = 900;
					break;
				case SeveritySensor::NR:
					ledstate.effect = IPMILED::ON;
					break;
				default:
					ledstate.effect = IPMILED::BLINK;
					ledstate.time_on_ms = 500;
				}

				ipmi_leds[1]->submit(ledstate);
			}
		}
	}
}

void PayloadManager::processNonManagedADCSensor(std::pair<std::string, ADCSensor> sensor) {
	this->log.log(std::string("Non-managed Sensor ") + sensor.second.name.c_str() + " has triggered a NR state but there is no handler defined.", LogTree::LOG_ERROR);
}

bool PayloadManager::isMZInContext(int mz) const {
	if (mz < 0) return true; // -1 = no mz context concept for this sensor

	ZoneController::Zone *zone = this->mgmt_zones[mz];
	if (zone->getDesiredPowerState() == false) {
		return false;
	}

	if ((zone->getLastTransitionStart() + MZ_HOLDOFF_TICKS) > get_tick64()) {
		return false;
	}
	return true;
}

void PayloadManager::updateSensorProcessorContexts() {
	MutexGuard<true> lock(this->mutex, true);
	AbsoluteTimeout next_update(UINT64_MAX);
	uint64_t start_of_run = get_tick64();

	if (this->sp_context_update_timer) {
		this->sp_context_update_timer->cancel();
		this->sp_context_update_timer = nullptr;
	}

	for (auto& adcsensor : this->adc_sensors) {
		std::shared_ptr<ThresholdSensor> sensor = adcsensor.second.ipmi_sensor.lock();
		if (!sensor) {
			continue; // We have no supported assertion mask to enable.
		}

		if (adcsensor.second.mz_context < 0) {
			continue; // This sensor doesn't do contexts
		}

		uint16_t desired_assertions = 0;
		uint16_t desired_deassertions = 0;

		std::shared_ptr<const SensorDataRecordReadableSensor> sdr = std::dynamic_pointer_cast<const SensorDataRecordReadableSensor>(device_sdr_repo.find(sensor->sdr_key));
		if (sdr) {
			desired_assertions   = sensor->assertion_events_enabled() & sdr->assertion_lower_threshold_reading_mask();
			desired_deassertions = sensor->deassertion_events_enabled() & sdr->deassertion_upper_threshold_reading_mask();

			if (!this->isMZInContext(adcsensor.second.mz_context)) {
				desired_assertions &= CONTEXT_EVENT_MASK;
				desired_deassertions &= CONTEXT_EVENT_MASK;
				uint64_t holdoff_end = this->mgmt_zones[adcsensor.second.mz_context]->getLastTransitionStart() + MZ_HOLDOFF_TICKS;
				//this->log.log(stdsprintf("MZ%d is out of context for sensor %s", adcsensor.second.mz_context, sensor->sensor_identifier().c_str()), LogTree::LOG_DIAGNOSTIC);
				if (next_update.getTimeout64() > holdoff_end) {
					next_update.setAbsTimeout(holdoff_end);
				}
			} else {
				//this->log.log(stdsprintf("MZ%d is in context for sensor %s", adcsensor.second.mz_context, sensor->sensor_identifier().c_str()), LogTree::LOG_DIAGNOSTIC);
			}

			this->sensor_processor->setEventEnable(adcsensor.second.sensor_processor_id, desired_assertions, desired_deassertions);
		}
	}

	if (next_update.getTimeout64() > start_of_run && next_update.getTimeout64() != UINT64_MAX) {
		// XSDK seems to crash if you put a lambda in a std::make_shared() directly.
		// A lambda is required in order to allow capture and use of *this, however.
		std::function<void(void)> self = [this]() -> void { this->updateSensorProcessorContexts(); };
		this->sp_context_update_timer = std::make_shared<TimerService::Timer>(self, next_update);
		TimerService::globalTimer().submit(this->sp_context_update_timer);
	}
}

void PayloadManager::refreshSensorLinkage() {
	MutexGuard<true> lock(this->mutex, true);
	this->alarmlevel_sensor = std::dynamic_pointer_cast<SeveritySensor>(ipmc_sensors.find_by_name("Alarm Level"));

	for (auto& adcsensor : this->adc_sensors) {
		std::shared_ptr<ThresholdSensor> sensor = std::dynamic_pointer_cast<ThresholdSensor>(ipmc_sensors.find_by_name(adcsensor.second.name));
		adcsensor.second.ipmi_sensor = sensor;
		if (adcsensor.second.sensor_processor_id < 0) {
			continue;
		}

		if (sensor) {
			std::shared_ptr<const SensorDataRecordReadableSensor> sdr = std::dynamic_pointer_cast<const SensorDataRecordReadableSensor>(device_sdr_repo.find(sensor->sdr_key));
			if (sdr) {
				uint16_t desired_assertions = sensor->assertion_events_enabled() & sdr->assertion_lower_threshold_reading_mask();
				uint16_t desired_deassertions = sensor->deassertion_events_enabled() & sdr->deassertion_upper_threshold_reading_mask();

				if (!this->isMZInContext(adcsensor.second.mz_context)) {
					desired_assertions &= CONTEXT_EVENT_MASK;
					desired_deassertions &= CONTEXT_EVENT_MASK;
				}

				// Disable all event enables not common to the active and new sets during reconfiguration.
				uint16_t assert, deassert;
				this->sensor_processor->getEventEnable(adcsensor.second.sensor_processor_id, assert, deassert);
				this->sensor_processor->setEventEnable(adcsensor.second.sensor_processor_id, assert & desired_assertions, deassert & desired_deassertions);

				// This is only going to work on increasing linear sensors, but that's all we can support at the moment.
				uint16_t hystunit = adcsensor.second.adc.floatToRaw(sdr->toFloat(1)) - adcsensor.second.adc.floatToRaw(sdr->toFloat(0));
				this->sensor_processor->setHysteresis(adcsensor.second.sensor_processor_id, sdr->hysteresis_high() * hystunit, sdr->hysteresis_low() * hystunit);

				Thr_Cfg thresholds;
				thresholds.LNR = (sensor->thresholds.lnr == 0) ? 0 : adcsensor.second.adc.floatToRaw(sdr->toFloat(sensor->thresholds.lnr));
				thresholds.LCR = (sensor->thresholds.lcr == 0) ? 0 : adcsensor.second.adc.floatToRaw(sdr->toFloat(sensor->thresholds.lcr));
				thresholds.LNC = (sensor->thresholds.lnc == 0) ? 0 : adcsensor.second.adc.floatToRaw(sdr->toFloat(sensor->thresholds.lnc));
				thresholds.UNC = (sensor->thresholds.unc == 0xFF) ? 0xFFFF : adcsensor.second.adc.floatToRaw(sdr->toFloat(sensor->thresholds.unc));
				thresholds.UCR = (sensor->thresholds.ucr == 0xFF) ? 0xFFFF : adcsensor.second.adc.floatToRaw(sdr->toFloat(sensor->thresholds.ucr));
				thresholds.UNR = (sensor->thresholds.unr == 0xFF) ? 0xFFFF : adcsensor.second.adc.floatToRaw(sdr->toFloat(sensor->thresholds.unr));
				this->sensor_processor->setThresholds(adcsensor.second.sensor_processor_id, thresholds);

				this->sensor_processor->setEventEnable(adcsensor.second.sensor_processor_id, desired_assertions, desired_deassertions);

				Hyst_Cfg hysteresis = this->sensor_processor->getHysteresis(adcsensor.second.sensor_processor_id);
				thresholds = this->sensor_processor->getThresholds(adcsensor.second.sensor_processor_id);
				this->sensor_processor->getEventEnable(adcsensor.second.sensor_processor_id, assert, deassert);
				this->log.log(stdsprintf("Sensor %s [%d] Thr: 0x%04hx 0x%04hx 0x%04hx 0x%04hx 0x%04hx 0x%04hx Hyst: +0x%04hx -0x%04hx Ena: +0x%04hx -0x%04hx",
						sensor->sensor_identifier().c_str(), adcsensor.second.sensor_processor_id,
						thresholds.LNC, thresholds.LCR, thresholds.LNR,
						thresholds.UNC, thresholds.UCR, thresholds.UNR,
						hysteresis.hyst_pos, hysteresis.hyst_neg,
						assert, deassert), LogTree::LOG_DIAGNOSTIC);
			} else {
				sensor->log.log(stdsprintf("Sensor %s (Proc[%d]) has no matching SDR.  Not updating sensor processor configuration for this sensor.", adcsensor.second.name.c_str(), adcsensor.second.sensor_processor_id), LogTree::LOG_WARNING);
			}
		} else {
			this->log.log(stdsprintf("Sensor %s (Proc[%d]) has no matching Sensor object (are SDRs configured correctly?), not updating hardfault configuration for this sensor.", adcsensor.second.name.c_str(), adcsensor.second.sensor_processor_id), LogTree::LOG_ERROR);
		}
	}
}

/// A backend power switch command
class PayloadManager::PowerLevelCommand : public CommandParser::Command {
public:
	PowerLevelCommand(PayloadManager &payloadmgr) : payloadmgr(payloadmgr) { };

	virtual std::string get_helptext(const std::string &command) const {
		return command + " [$new_power_level [$force]]\n"
				"  $new_power_level corresponds to an IPMI payload power level:\n"
				"    0 = off\n"
				"    1 = all backend power on\n"
				"  $force = \"true\" orders the IPMC to disregard the currently negotiated maximum power level\n"
				"\n"
				"This command changes our backend power enables without affecting or overriding IPMI state.\n"
				"\n"
				"Without parameters, this will return power status.\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		if (parameters.nargs() == 1) {
			unsigned int negotiated_power_watts = 0;
			if (this->payloadmgr.power_properties.current_power_level) {
				negotiated_power_watts = this->payloadmgr.power_properties.power_levels[this->payloadmgr.power_properties.current_power_level-1];
			}

			negotiated_power_watts *= this->payloadmgr.power_properties.power_multiplier;
			uint32_t pen_state = this->payloadmgr.mgmt_zones[0]->getPowerEnableStatus(false);
			console->write(stdsprintf(
					"The current negotiated power budget is %hhu (%u watts)\n"
					"The power enables are currently at 0x%08lx\n",
					this->payloadmgr.power_properties.current_power_level, negotiated_power_watts,
					pen_state));
			return;
		}

		uint8_t new_level;
		bool force = false;
		// Parse $new_level parameter.
		if (!parameters.parseParameters(1, (parameters.nargs() == 2), &new_level)) {
			console->write("Invalid parameters.\n");
			return;
		}

		// Parse $force parameter.
		if (parameters.nargs() >= 3 && !parameters.parseParameters(2, true, &force)) {
			console->write("Invalid parameters.\n");
			return;
		}

		if (new_level >= 2) {
			console->write("Invalid power level.\n");
			return;
		}

		if (new_level > this->payloadmgr.power_properties.current_power_level && !force) {
			console->write("The requested power level is higher than our negotiated power budget.\n");
			return;
		}

		this->payloadmgr.implementPowerLevel(new_level);
	}

private:
	PayloadManager &payloadmgr;
};

/// A management zone power switch command
class PayloadManager::MzControlCommand : public CommandParser::Command {
public:
	MzControlCommand(PayloadManager &payloadmgr) : payloadmgr(payloadmgr) { };

	virtual std::string get_helptext(const std::string &command) const {
		return command + " [$mz_number [$on_off]]\n\n"
				"This command changes our MZ enables without affecting or overriding IPMI state.\n"
				"\n"
				"Without parameters, this will return all MZ status.\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		if (parameters.nargs() == 1) {
			// Show all MZ status
			for (size_t i = 0; i < XPAR_MGMT_ZONE_CTRL_0_MZ_CNT; i++) {
				bool active = this->payloadmgr.mgmt_zones[i]->getPowerState();
				console->write(stdsprintf("MZ %d is currently %s.", i, (active?"ON ":"OFF")));
				if (this->payloadmgr.mgmt_zones[i]->getName()[0] != '\0') {
					console->write(" [" + this->payloadmgr.mgmt_zones[i]->getName() + "]");
				}
				console->write("\n");
			}
		} else {
			uint8_t mz_number;

			// Parse $mz_number parameter.
			if (!parameters.parseParameters(1, false, &mz_number)) {
				console->write("Invalid parameters.\n");
				return;
			}

			if (mz_number >= XPAR_MGMT_ZONE_CTRL_0_MZ_CNT) {
				console->write("MZ number out-of-range.\n");
				return;
			}

			if (parameters.nargs() == 2) {
				// Show MZ status
				bool active = this->payloadmgr.mgmt_zones[mz_number]->getPowerState();
				console->write(stdsprintf("MZ %d", mz_number));
				if (this->payloadmgr.mgmt_zones[mz_number]->getName()[0] != '\0') {
					console->write(" [" + this->payloadmgr.mgmt_zones[mz_number]->getName() + "]");
				}
				console->write(stdsprintf(" is currently %s.\n", (active?"ON":"OFF")));

			} else {
				ZoneController::Zone::PowerAction action = ZoneController::Zone::OFF;

				if (!parameters.parameters[2].compare("on")) {
					action = ZoneController::Zone::ON;
				} else if (!parameters.parameters[2].compare("off")) {
					action = ZoneController::Zone::OFF;
				} else {
					console->write("$on_off needs to be 'on' or 'off'.\n");
					return;
				}

				this->payloadmgr.mgmt_zones[mz_number]->setPowerState(action);
			}
		}
	}

private:
	PayloadManager &payloadmgr;
};

/// A sensor readout command
class PayloadManager::ReadIPMISensorsCommand : public CommandParser::Command {
public:
	virtual std::string get_helptext(const std::string &command) const {
		return command + "\n\n"
				"Read out the status of all IPMI sensors\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		std::string out;
		for (auto &sensorinfo : SensorSet::container_type(ipmc_sensors)) {
			std::string name = sensorinfo.second->sensor_identifier();
			{ // Common-ify the sensor name if possible.
				std::shared_ptr<const SensorDataRecordSensor> sdr = std::dynamic_pointer_cast<const SensorDataRecordSensor>(device_sdr_repo.find(sensorinfo.second->sdr_key));
				if (sdr) {
					name = sdr->id_string();
				}
			}
			{ // Process if ThresholdSensor
				std::shared_ptr<ThresholdSensor> sensor = std::dynamic_pointer_cast<ThresholdSensor>(sensorinfo.second);
				if (sensor) {
					ThresholdSensor::Value value = sensor->get_value();

					static const std::string threshold_names[12] = {
							"lnc-",
							"lnc+",
							"lcr-",
							"lcr+",
							"lnr-",
							"lnr+",
							"unc-",
							"unc+",
							"ucr-",
							"ucr+",
							"unr-",
							"unr+"
					};

					std::string thresholds;
					for (int i = 0; i < 12; ++i) {
						if (value.active_events & 0x0a95 /* don't report lower going-high and upper going-low */ & 1<<i) {
							thresholds += std::string(" ") + threshold_names[i];
						}
					}

					out += stdsprintf("%3d %-30s %9.6f (raw %3hhu; %3u%% in context; events 0x%03hu%s)\n",
							sensor->sdr_key[2],
							name.c_str(),
							value.float_value,
							value.byte_value,
							(100*__builtin_popcount(value.event_context))/12,
							value.active_events,
							thresholds.c_str());
					continue;
				}
			}
			{ // Process if HotswapSensor
				std::shared_ptr<HotswapSensor> sensor = std::dynamic_pointer_cast<HotswapSensor>(sensorinfo.second);
				if (sensor) {
					out += stdsprintf("%3d %-30s M%hhu\n",
							sensor->sdr_key[2],
							name.c_str(),
							sensor->get_mstate());
					continue;
				}
			}
			{ // Process if SeveritySensor
				std::shared_ptr<SeveritySensor> sensor = std::dynamic_pointer_cast<SeveritySensor>(sensorinfo.second);
				if (sensor) {
					uint8_t status = static_cast<uint8_t>(sensor->get_sensor_value());
					std::string label = "Invalid State";
					if (status < 9) {
						label = SeveritySensor::StateTransitionLabels[status];
					}

					out += stdsprintf("%3d %-30s State %hhu: %s\n",
							sensor->sdr_key[2],
							name.c_str(),
							status, label.c_str());
					continue;
				}
			}
		}
		console->write(out);
	}
};

/// A sensor readout command
class PayloadManager::GetSensorEventEnablesCommand : public CommandParser::Command {
public:
	virtual std::string get_helptext(const std::string &command) const {
		return command + " $sensor_number\n\n"
				"Retrieve a sensor's event enable & event supported status.\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		uint8_t sensor_number;
		if (!parameters.parseParameters(1, true, &sensor_number)) {
			console->write("Invalid parameters.  Try `help`.\n");
			return;
		}

		std::shared_ptr<Sensor> sensor = ipmc_sensors.get(sensor_number);
		if (!sensor) {
			console->write("Unknown sensor number.\n");
			return;
		}

		std::shared_ptr<const SensorDataRecordReadableSensor> sdr = std::dynamic_pointer_cast<const SensorDataRecordReadableSensor>(device_sdr_repo.find(sensor->sdr_key));

		uint16_t asserts_enabled = sensor->assertion_events_enabled();
		uint16_t deasserts_enabled = sensor->deassertion_events_enabled();

		std::string out;
		out += stdsprintf("Events Enabled Mask   (transient)  Assertions: 0x%03hx, Deassertions: 0x%03hx\n", asserts_enabled, deasserts_enabled);

		if (sdr) {
			uint16_t asserts_supported = sdr->assertion_lower_threshold_reading_mask();
			uint16_t deasserts_supported = sdr->deassertion_upper_threshold_reading_mask();

			out += stdsprintf("Events Supported Mask (persistent) Assertions: 0x%03hx, Deassertions: 0x%03hx\n", asserts_supported, deasserts_supported);
			out += stdsprintf("Effective Enabled Set (ena & sup)  Assertions: 0x%03hx, Deassertions: 0x%03hx\n", asserts_enabled & asserts_supported, deasserts_enabled & deasserts_supported);
		} else {
			out += stdsprintf("Type 01/02 SDR unavailable for sensor %hhu.\n", sensor_number);
		}
		console->write(out);
	}
};

/// A sensor readout command
class PayloadManager::SetSensorEventEnablesCommand : public CommandParser::Command {
public:
	SetSensorEventEnablesCommand(PayloadManager &payloadmgr) : payloadmgr(payloadmgr) { };

	virtual std::string get_helptext(const std::string &command) const {
		return command + " $sensor_number (enabled|supported) $assertion_mask $deassertion_mask\n\n"
				"Set a sensor's event enable or event supported status.\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		uint8_t sensor_number;
		std::string enable_type;
		CommandParser::CommandParameters::xint16_t assertmask, deassertmask;
		if (!parameters.parseParameters(1, true, &sensor_number, &enable_type, &assertmask, &deassertmask)) {
			console->write("Invalid parameters.  Try `help`.\n");
			return;
		}

		std::shared_ptr<Sensor> sensor = ipmc_sensors.get(sensor_number);
		if (!sensor) {
			console->write("Unknown sensor number.\n");
			return;
		}

		std::shared_ptr<const SensorDataRecordReadableSensor> sdr = std::dynamic_pointer_cast<const SensorDataRecordReadableSensor>(device_sdr_repo.find(sensor->sdr_key));

		if (enable_type == "enabled") {
			sensor->assertion_events_enabled(assertmask);
			sensor->deassertion_events_enabled(deassertmask);
			payload_manager->refreshSensorLinkage();
		} else if (enable_type == "supported") {
			if (!sdr) {
				console->write(stdsprintf("Type 01/02 SDR unavailable for sensor %hhu.\n", sensor_number));
				return;
			}

			std::shared_ptr<SensorDataRecordReadableSensor> mutable_sdr = std::dynamic_pointer_cast<SensorDataRecordReadableSensor>(sdr->interpret());
			if (!mutable_sdr) {
				console->write("Unable to reinterpret const SensorDataRecordReadableSensor as SensorDataRecordReadableSensor! Invariant failed! Aborted!\n");
				return;
			}

			mutable_sdr->assertion_lower_threshold_reading_mask(assertmask);
			mutable_sdr->deassertion_upper_threshold_reading_mask(deassertmask);

			device_sdr_repo.add(*mutable_sdr, 0);

			// Write changes to EEPROM
			VariablePersistentAllocation sdr_persist(*persistent_storage, PersistentStorageAllocations::WISC_SDR_REPOSITORY);
			sdr_persist.setData(device_sdr_repo.u8export());

			this->payloadmgr.refreshSensorLinkage();
		} else {
			console->write("Unknown enable type.  Try `help`.\n");
			return;
		}


		std::string out = "Configuration updated.\n\n";

		uint16_t asserts_enabled = sensor->assertion_events_enabled();
		uint16_t deasserts_enabled = sensor->deassertion_events_enabled();
		out += stdsprintf("Events Enabled Mask   (transient)  Assertions: 0x%03hx, Deassertions: 0x%03hx\n", asserts_enabled, deasserts_enabled);

		sdr = std::dynamic_pointer_cast<const SensorDataRecordReadableSensor>(device_sdr_repo.find(sensor->sdr_key));
		if (sdr) {
			uint16_t asserts_supported = sdr->assertion_lower_threshold_reading_mask();
			uint16_t deasserts_supported = sdr->deassertion_upper_threshold_reading_mask();

			out += stdsprintf("Events Supported Mask (persistent) Assertions: 0x%03hx, Deassertions: 0x%03hx\n", asserts_supported, deasserts_supported);
			out += stdsprintf("Effective Enabled Set (ena & sup)  Assertions: 0x%03hx, Deassertions: 0x%03hx\n", asserts_enabled & asserts_supported, deasserts_enabled & deasserts_supported);
		} else {
			out += stdsprintf("Type 01/02 SDR unavailable for sensor %hhu.\n", sensor_number);
		}
		console->write(out);
	}

private:
	PayloadManager &payloadmgr;
};


void PayloadManager::registerConsoleCommands(CommandParser &parser, const std::string &prefix) {
	parser.registerCommand(prefix + "power_level", std::make_shared<PayloadManager::PowerLevelCommand>(*this));
	parser.registerCommand(prefix + "mz_control", std::make_shared<PayloadManager::MzControlCommand>(*this));
	parser.registerCommand(prefix + "read_ipmi_sensors", std::make_shared<PayloadManager::ReadIPMISensorsCommand>());
	parser.registerCommand(prefix + "get_sensor_event_enables", std::make_shared<PayloadManager::GetSensorEventEnablesCommand>());
	parser.registerCommand(prefix + "set_sensor_event_enables", std::make_shared<PayloadManager::SetSensorEventEnablesCommand>(*this));
}

void PayloadManager::deregisterConsoleCommands(CommandParser &parser, const std::string &prefix) {
	parser.registerCommand(prefix + "power_level", nullptr);
	parser.registerCommand(prefix + "mz_control", nullptr);
	parser.registerCommand(prefix + "read_ipmi_sensors", nullptr);
	parser.registerCommand(prefix + "get_sensor_event_enables", nullptr);
	parser.registerCommand(prefix + "set_sensor_event_enables", nullptr);
}
