/*
 * PayloadManager.cpp
 *
 *  Created on: Dec 4, 2018
 *      Author: jtikalsky
 */

#include <PayloadManager.h>
#include <libs/ThreadingPrimitives.h>
#include <libs/printf.h>
#include <services/console/ConsoleSvc.h>
#include <services/ipmi/sensor/SensorSet.h>
#include <services/ipmi/sensor/ThresholdSensor.h>
#include <services/ipmi/sdr/SensorDataRecordReadableSensor.h>
#include <exception>
#include <IPMC.h>

/**
 * Instantiate the PayloadManager and perform all required initialization.
 *
 * @param mstate_machine The MStateMachine to register
 * @param log The LogTree to use
 */
PayloadManager::PayloadManager(MStateMachine *mstate_machine, LogTree &log)
	: mstate_machine(mstate_machine), log(log) {
	configASSERT(this->mutex = xSemaphoreCreateRecursiveMutex());

	/* Here we define E-Keying links.  These will be checked by the Shelf Manager
	 * and enabled with an IPMI command which routes to the update_link_enable
	 * method of this PayloadManager.  Once the appropriate link has been enabled,
	 * you are free to power it up and start communication on it.  According to
	 * the specifications, you should not enable a link that has not been enabled
	 * through E-Keying, and must disable any link that has been subsequently
	 * disabled through E-Keying.
	 *
	 * The IPMC will automatically generate FRU Data records for these links,
	 * as a part of the FRU Data initialization, however if the list of link
	 * descriptors is changed, it may be necessary to delete or update the
	 * persistent FRU Data storage in EEPROM, using the eeprom.delete_section
	 * console commands prior to the firmware upgrade (or other reboot).
	 */

	// Define E-Keying Links: 1G to Hub Slots
	this->links.push_back(LinkDescriptor(0, 0, 1, 1, LinkDescriptor::IF_BASE, 1));
	this->links.push_back(LinkDescriptor(0, 0, 1, 1, LinkDescriptor::IF_BASE, 2));

	// Set up Management Zones
	for (int i = 0; i < XPAR_MGMT_ZONE_CTRL_0_MZ_CNT; ++i) {
		this->mgmt_zones[i] = new MGMT_Zone(XPAR_MGMT_ZONE_CTRL_0_DEVICE_ID, i);
		this->mgmt_zone_expected_states[i] = false;
	}

	SuspendGuard suspend(true);
	this->mstate_machine->deactivate_payload = [this]() -> void {
		// Turn off power.
		this->set_power_level(0, 0);

		// Disable all E-Keying links.
		// (IPMI commands will not be sent for this when proceeding through M6)
		std::vector<LinkDescriptor> links = this->get_links();
		for (auto it = links.begin(), eit = links.end(); it != eit; ++it) {
			if (it->enabled) {
				it->enabled = false;
				this->update_link_enable(*it);
			}
		}

		// In our specific backend implementation the above is all synchronous.
		this->mstate_machine->payload_deactivation_complete();
	};
	suspend.release();

	ADC::Channel::Callback tmp36   = [](float r) -> float { return (r - 0.5) * 100.0; };
	ADC::Channel::Callback tmp36_r = [](float r) -> float { return (r/100.0) + 0.5; };
	ADC::Channel::Callback imon    = [](float r) -> float { return r * 1000.0 / 80.0; };
	ADC::Channel::Callback imon_r  = [](float r) -> float { return r / 1000.0 * 80.0; };

	this->adc_sensors = std::map<std::string, ADCSensor>{
		{ "+12VPYLD",      ADCSensor("+12VPYLD",      0, 0, ADC::Channel(*adc[0], 0, 5.640),          0,   0) },
		{ "+5VPYLD",       ADCSensor("+5VPYLD",       0, 0, ADC::Channel(*adc[0], 1, 2.467),          1,   1) },
		{ "+3.55VBULK",    ADCSensor("+3.55VBULK",    0, 0, ADC::Channel(*adc[0], 2, 1.749),          2,   1) },
		{ "+3.3VMP2",      ADCSensor("+3.3VMP2",      0, 0, ADC::Channel(*adc[0], 3, 1.649),          3,   0) },
		{ "+3.3VFFLY5",    ADCSensor("+3.3VFFLY5",    0, 0, ADC::Channel(*adc[0], 4, 1.649),          4,   1) },
		{ "+3.3VFFLY4",    ADCSensor("+3.3VFFLY4",    0, 0, ADC::Channel(*adc[0], 5, 1.649),          5,   1) },
		{ "+3.3VFFLY3",    ADCSensor("+3.3VFFLY3",    0, 0, ADC::Channel(*adc[0], 6, 1.649),          6,   1) },
		{ "+3.3VFFLY2",    ADCSensor("+3.3VFFLY2",    0, 0, ADC::Channel(*adc[0], 7, 1.649),          7,   1) },

		{ "+3.3VFFLY1",    ADCSensor("+3.3VFFLY1",    1, 0, ADC::Channel(*adc[1], 7, 1.649),          8,   1) },
		{ "+3.3VDD",       ADCSensor("+3.3VDD",       1, 0, ADC::Channel(*adc[1], 6, 1.649),          9,   1) },
		{ "+2.5VXPT",      ADCSensor("+2.5VXPT",      1, 0, ADC::Channel(*adc[1], 5, 1.299),          10,  1) },
		{ "+1.95VBULK",    ADCSensor("+1.95VBULK",    1, 0, ADC::Channel(*adc[1], 4),                 11,  1) },
		{ "+1.8VFFLY2",    ADCSensor("+1.8VFFLY2",    1, 0, ADC::Channel(*adc[1], 3),                 12,  1) },
		{ "+1.8VFFLY1",    ADCSensor("+1.8VFFLY1",    1, 0, ADC::Channel(*adc[1], 2),                 13,  1) },
		{ "+1.8VDD",       ADCSensor("+1.8VDD",       1, 0, ADC::Channel(*adc[1], 1),                 14,  1) },

		{ "+0.9VMGTT",     ADCSensor("+0.9VMGTT",     2, 0, ADC::Channel(*adc[2], 0),                 15,  1) },
		{ "+0.9VMGTB",     ADCSensor("+0.9VMGTB",     2, 0, ADC::Channel(*adc[2], 1),                 16,  1) },
		{ "+0.85VDD",      ADCSensor("+0.85VDD",      2, 0, ADC::Channel(*adc[2], 2),                 17,  1) },
		{ "MGT0.9VT_IMON", ADCSensor("MGT0.9VT_IMON", 2, 0, ADC::Channel(*adc[2], 3, imon, imon_r),   18,  1) },
		{ "MGT0.9VB_IMON", ADCSensor("MGT0.9VB_IMON", 2, 0, ADC::Channel(*adc[2], 4, imon, imon_r),   19,  1) },
		{ "MGT1.2VT_IMON", ADCSensor("MGT1.2VT_IMON", 2, 0, ADC::Channel(*adc[2], 5, imon, imon_r),   20,  1) },
		{ "MGT1.2VB_IMON", ADCSensor("MGT1.2VB_IMON", 2, 0, ADC::Channel(*adc[2], 6, imon, imon_r),   21,  1) },
		{ "T_BOARD1",      ADCSensor("T_BOARD1",      2, 0, ADC::Channel(*adc[2], 7, tmp36, tmp36_r), -1, -1) },

		{ "+1.35VMGTT",    ADCSensor("+1.35VMGTT",    2, 1, ADC::Channel(*adc[3], 0),                 22,  1) },
		{ "+1.35VMGTB",    ADCSensor("+1.35VMGTB",    2, 1, ADC::Channel(*adc[3], 1),                 23,  1) },
		{ "+1.2VPHY",      ADCSensor("+1.2VPHY",      2, 1, ADC::Channel(*adc[3], 2),                 24,  1) },
		{ "+1.2VMGTT",     ADCSensor("+1.2VMGTT",     2, 1, ADC::Channel(*adc[3], 3),                 25,  1) },
		{ "+1.2VMGTB",     ADCSensor("+1.2VMGTB",     2, 1, ADC::Channel(*adc[3], 4),                 26,  1) },
		{ "VLUTVDDIO",     ADCSensor("VLUTVDDIO",     2, 1, ADC::Channel(*adc[3], 5),                 27,  3) },
		{ "+1.05VMGTT",    ADCSensor("+1.05VMGTT",    2, 1, ADC::Channel(*adc[3], 6),                 28,  1) },
		{ "+1.05VMGTB",    ADCSensor("+1.05VMGTB",    2, 1, ADC::Channel(*adc[3], 7),                 29,  1) },

		{ "+1.8VFFLY3",    ADCSensor("+1.8VFFLY3",    2, 2, ADC::Channel(*adc[4], 0),                 30,  1) },
		{ "+1.8VFFLY4",    ADCSensor("+1.8VFFLY4",    2, 2, ADC::Channel(*adc[4], 1),                 31,  1) },
		{ "+1.8VFFLY5",    ADCSensor("+1.8VFFLY5",    2, 2, ADC::Channel(*adc[4], 2),                 32,  1) },
		{ "T_BOARD2",      ADCSensor("T_BOARD2",      2, 2, ADC::Channel(*adc[4], 3, tmp36, tmp36_r), -1, -1) },
	};

	auto calc_hf_mask = [this](std::vector<std::string> sensors) -> uint64_t {
		uint64_t mask = 0;
		for (auto&& name : sensors) {
			try {
				ADCSensor &sensor = this->adc_sensors.at(name);
				if (sensor.sensor_processor_id >= 0)
					mask |= 1 << sensor.sensor_processor_id;
			}
			catch (std::out_of_range) {
				this->log.log(stdsprintf("Unable find sensor %s in payload_manager->adc_sensors: No hardfault protection available.", name.c_str()), LogTree::LOG_ERROR);
			}
		}
		return mask;
	};

	std::vector<MGMT_Zone::OutputConfig> pen_config;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
	const uint64_t hfm_PWRENA_0 = calc_hf_mask({"+12VPYLD"});
	const uint64_t hfm_PWRENA_1 = calc_hf_mask({"+0.85VDD"});
	const uint64_t hfm_PWRENA_2 = calc_hf_mask({
		"+1.35VMGTT",
		"+1.35VMGTB",
		"+1.05VMGTT",
		"+1.05VMGTB",
		"+1.95VBULK",
		"+3.55VBULK",
	});
	const uint64_t hfm_PWRENA_3 = calc_hf_mask({"+3.3VDD", "+1.8VDD"});
	const uint64_t hfm_PWRENA_LUT = calc_hf_mask({"+12VPYLD", "VLUTVDDIO"});
	const uint64_t hfm_PWRENA_4 = calc_hf_mask({"+0.9VMGTT", "+0.9VMGTB"});
	const uint64_t hfm_PWRENA_5 = calc_hf_mask({
		"+1.2VMGTT",
		"+1.2VMGTB",
		"+1.8VFFLY2",
		"+1.8VFFLY1",
		"+1.8VFFLY3",
		"+1.8VFFLY4",
		"+1.8VFFLY5",
		"+1.2VPHY",
	});
	const uint64_t hfm_PWRENA_6 = calc_hf_mask({
		"+3.3VFFLY1",
		"+3.3VFFLY2",
		"+3.3VFFLY3",
		"+3.3VFFLY4",
		"+3.3VFFLY5",
		"+2.5VXPT",
	});
	const uint64_t hfm_PWRENA_ELM = calc_hf_mask({"+12VPYLD"});
	const uint64_t hfm_PWRENA_7 = calc_hf_mask({"+5VPYLD"});
	const uint64_t hfm_PWRENA_RTM_MGMT = calc_hf_mask({"+3.3VMP2"});
	const uint64_t hfm_PWRENA_RTM_PYLD = calc_hf_mask({"+12VPYLD"});
#pragma GCC diagnostic pop

	{
		// Management Zone 0, +12V power
		this->mgmt_zones[0]->set_hardfault_mask(hfm_PWRENA_0, 140);
		this->mgmt_zones[0]->get_pen_config(pen_config);

		// PWRENA_0#: +12VPYLD
		pen_config[0].drive_enabled = true;
		pen_config[0].active_high = false;
		pen_config[0].enable_delay = 0;

		// PWRENA_ACTVn
		// TODO: Consider moving this. It is used for the timing circuit.
		/*pen_config[11].active_high = false;
		pen_config[11].drive_enabled = true;
		pen_config[11].enable_delay = 0;*/

		this->mgmt_zones[0]->set_pen_config(pen_config);
	}

	{
		// Management Zone 1, VU9P
		this->mgmt_zones[1]->set_hardfault_mask(
				hfm_PWRENA_0| /* MZ0 */
				hfm_PWRENA_1|
				hfm_PWRENA_2|
				hfm_PWRENA_3|
				hfm_PWRENA_4|
				hfm_PWRENA_5|
				hfm_PWRENA_6|
				hfm_PWRENA_7,
				140);
		this->mgmt_zones[1]->get_pen_config(pen_config);

		// PWRENA_1: +0.85VDD
		pen_config[1].drive_enabled = true;
		pen_config[1].active_high = true;
		pen_config[1].enable_delay = 10;

		// PWRENA_2: +1.05VMGTT, +1.35VMGTT, +1.05VMGTB, +1.35VMGTB, +1.95VBULK, +3.55VBULK
		pen_config[2].drive_enabled = true;
		pen_config[2].active_high = true;
		pen_config[2].enable_delay = 20;

		// PWRENA_3: +3.3VDD, +1.8VDD
		pen_config[3].drive_enabled = true;
		pen_config[3].active_high = true;
		pen_config[3].enable_delay = 30;

		// PWRENA_4: +0.9VMGTT, +0.9VMGTB
		pen_config[4].drive_enabled = true;
		pen_config[4].active_high = true;
		pen_config[4].enable_delay = 40;

		// PWRENA_5: +1.2VMGTT, +1.2VMGTB, +1.8VFFLY2, +1.8VFFLY1, +1.8VFFLY3, +1.8VFFLY4, +1.8VFFLY5, +1.2VPHY
		pen_config[5].drive_enabled = true;
		pen_config[5].active_high = true;
		pen_config[5].enable_delay = 50;

		// PWRENA_6: +3.3VFFLY1, +3.3VFFLY2, +3.3VFFLY3, +3.3VFFLY4, +3.3VFFLY5, +2.5VXPT
		pen_config[6].drive_enabled = true;
		pen_config[6].active_high = true;
		pen_config[6].enable_delay = 50; // In phase with PWRENA_5

		// PWRENA_7: +5VUSBFAN
		pen_config[7].drive_enabled = true;
		pen_config[7].active_high = true;
		pen_config[7].enable_delay = 50; // In phase with PWRENA_5

		// FANENA
		pen_config[9].drive_enabled = true;
		pen_config[9].active_high = true;
		pen_config[9].enable_delay = 60;

		this->mgmt_zones[1]->set_pen_config(pen_config);
	}

	{
		// Management Zone 2, ELM
		this->mgmt_zones[2]->set_hardfault_mask(
				hfm_PWRENA_0| /* MZ0 */
				hfm_PWRENA_ELM,
				140);
		this->mgmt_zones[2]->get_pen_config(pen_config);

		// PWRENA_ELM
		pen_config[11].drive_enabled = true;
		pen_config[11].active_high = true;
		pen_config[11].enable_delay = 70;

		this->mgmt_zones[2]->set_pen_config(pen_config);
	}

	{
		// Management Zone 3, LLUT
		this->mgmt_zones[3]->set_hardfault_mask(
				hfm_PWRENA_0| /* MZ0 */
				hfm_PWRENA_1| /* MZ1 */
				hfm_PWRENA_2| /* MZ1 */
				hfm_PWRENA_3| /* MZ1 */
				hfm_PWRENA_4| /* MZ1 */
				hfm_PWRENA_5| /* MZ1 */
				hfm_PWRENA_6| /* MZ1 */
				hfm_PWRENA_7| /* MZ1 */
				hfm_PWRENA_LUT,
				140);
		this->mgmt_zones[3]->get_pen_config(pen_config);

		// PWRENA_LUT, VLUTVDDIO
		pen_config[8].drive_enabled = true;
		pen_config[8].active_high = true;
		pen_config[8].enable_delay = 30;

		// LLUT_PWREN (enable line)
		pen_config[10].drive_enabled = true;
		pen_config[10].active_high = true;
		pen_config[10].enable_delay = 40;

		this->mgmt_zones[3]->set_pen_config(pen_config);
	}

	{
		// Management Zone 4, RTM
		this->mgmt_zones[4]->set_hardfault_mask(
				hfm_PWRENA_0| /* MZ0 */
				hfm_PWRENA_1| /* MZ1 */
				hfm_PWRENA_2| /* MZ1 */
				hfm_PWRENA_3| /* MZ1 */
				hfm_PWRENA_4| /* MZ1 */
				hfm_PWRENA_5| /* MZ1 */
				hfm_PWRENA_6| /* MZ1 */
				hfm_PWRENA_7| /* MZ1 */
				hfm_PWRENA_RTM_MGMT|
				hfm_PWRENA_RTM_PYLD,
				140);
		this->mgmt_zones[4]->get_pen_config(pen_config);

		// PWRENA_RTM_MGMTn
		pen_config[13].drive_enabled = true;
		pen_config[13].enable_delay = 80;

		// PWRENA_RTM_PYLDn
		pen_config[12].drive_enabled = true;
		pen_config[12].enable_delay = 100;

		this->mgmt_zones[4]->set_pen_config(pen_config);
	}

	std::vector<ADC::Channel*> sensor_processor_channels(XPAR_IPMI_SENSOR_PROC_0_SENSOR_CNT);
	for (auto &adcsensor : this->adc_sensors)
		if (adcsensor.second.sensor_processor_id >= 0)
			sensor_processor_channels[adcsensor.second.sensor_processor_id] = &adcsensor.second.adc;
	this->sensor_processor = new SensorProcessor(0, 66 /* XXX This needs a constant, which needs firmware improvements. */, sensor_processor_channels);

	// The sensor processor is configured and enabled by the sensor linkage
	// update, as this is where thresholds become available.
}

#include <IPMC.h>
void payload_manager_apd_bringup_poweroff_hack() { // XXX
	if (payload_manager)
		payload_manager->implement_power_level(0);
}

PayloadManager::~PayloadManager() {
	/* We definitely want to kill all zones as simultaneously as possible, and
	 * the "kill zone" operation is just a single register write, therefore
	 * critical section.
	 */
	CriticalGuard critical(true);
	for (int i = 0; i < XPAR_MGMT_ZONE_CTRL_0_MZ_CNT; ++i) {
		this->mgmt_zones[i]->set_power_state(MGMT_Zone::KILL);
		delete this->mgmt_zones[i];
	}

	SuspendGuard suspend(true);
	this->mstate_machine->deactivate_payload = NULL;
	suspend.release();

	configASSERT(0); // We don't have a clean way to shut down the sensor thread.

	vSemaphoreDelete(this->mutex);
}

/**
 * Retrieve the current power properties & negotiated status for the payload.
 *
 * @param fru The FRU to operate on.
 * @param recompute true if Compute Power Properties is requesting that power
 *                  properties be recomputed if desired, else false
 * @return A set of power properties for IPMI.
 */
PayloadManager::PowerProperties PayloadManager::get_power_properties(uint8_t fru, bool recompute) {
	if (fru != 0)
		throw std::domain_error("This FRU is not known.");

	MutexGuard<true> lock(this->mutex, true);

	if (recompute || this->power_properties.power_levels.empty()) {
		// We need to compute our power properties.
		// Nothing we do is dynamic at this time, so we'll just fill in the statics.

		// I suppose we can support this.  We don't have multiple power levels anyway.
		this->power_properties.dynamic_reconfiguration = true;

		// We don't make use of a startup power level.
		this->power_properties.delay_to_stable_power = 0;

		// We'll use 1W units.
		this->power_properties.power_multiplier = 1;

		this->power_properties.power_levels.clear();
		this->power_properties.early_power_levels.clear();

		// We require 75W for our fully loaded CDB. (First 10W is free: PICMG 3.0 §3.9.1.3 ¶419)
		this->power_properties.power_levels.push_back(65);
		this->power_properties.early_power_levels.push_back(65);

		// We always want to be on, but only have one 'on'.
		this->power_properties.desired_power_level = 1;
	}

	// We COULD track this separately, but we don't.
	//this->power_properties.current_power_level = 0;

	this->power_properties.remaining_delay_to_stable_power = 0; // We don't do early power draw.

	return this->power_properties;
}

/**
 * Set the power utilization for the specified FRU to the value previously
 * calculated for the selected level.
 *
 * @param fru The FRU to manage
 * @param level The pre-calculated power level to set
 */
void PayloadManager::set_power_level(uint8_t fru, uint8_t level) {
	if (fru != 0)
		throw std::domain_error("This FRU is not known.");

	MutexGuard<true> lock(this->mutex, true);

	this->power_properties.current_power_level = level;
	if (level == 0) {
		// Power OFF!
		this->log.log("Power Level set to 0 by shelf.", LogTree::LOG_INFO);
		this->implement_power_level(0);
		this->mstate_machine->payload_deactivation_complete();
	}
	else if (level == 1) {
		// We only support one non-off power state.
		this->log.log("Power Level set to 1 by shelf.", LogTree::LOG_INFO);
		this->implement_power_level(1);
		this->mstate_machine->payload_activation_complete();
	}
	else {
		throw std::domain_error(stdsprintf("Power level %hhu is not supported.", level));
	}
}

/**
 * A helper to physically apply a specified power level.
 *
 * @param level The level to apply
 */
void PayloadManager::implement_power_level(uint8_t level) {
	MutexGuard<true> lock(this->mutex, true);

	if (level == 0) {
		// Power OFF!
		this->log.log("Implement Power Level 0: Shutting down.", LogTree::LOG_DIAGNOSTIC);
		for (int i = 3; i >= 0; --i) {
			/* These are sequenced with delays in firmware so that all can be
			 * disabled at once and the right things will happen.
			 */
			this->mgmt_zones[i]->set_power_state(MGMT_Zone::OFF);
			this->mgmt_zone_expected_states[i] = false;
		}
		this->log.log("Implement Power Level 0: Shutdown complete.", LogTree::LOG_DIAGNOSTIC);
#if 1 // XXX
		struct IPMI_LED::Action ledstate;
		ledstate.min_duration = 0;
		ledstate.effect = IPMI_LED::OFF;
		ipmi_leds[2]->submit(ledstate);
		ledstate.effect = IPMI_LED::ON;
		ipmi_leds[3]->submit(ledstate);
#endif
	}
	else if (level == 1) {
		// We only support one non-off power state.
		this->log.log("Implement Power Level 1: Powering up backend.", LogTree::LOG_DIAGNOSTIC);
		for (int i = 0; i <= 3; ++i) {
			/* These are sequenced with delays in firmware so that all can be
			 * enabled at once and the right things will happen.
			 */
			this->mgmt_zones[i]->set_power_state(MGMT_Zone::ON);
			this->mgmt_zone_expected_states[i] = true;
		}
		// If we were waiting in M3, go to M4. (Skipping E-Keying for now)
		this->log.log("Implement Power Level 1: Backend powered up.", LogTree::LOG_DIAGNOSTIC);
#if 1 // XXX
		struct IPMI_LED::Action ledstate;
		ledstate.min_duration = 0;
		ledstate.effect = IPMI_LED::ON;
		ipmi_leds[2]->submit(ledstate);
		ledstate.effect = IPMI_LED::OFF;
		ipmi_leds[3]->submit(ledstate);
#endif
		this->mstate_machine->payload_activation_complete();
	}
}

PayloadManager::LinkDescriptor::LinkDescriptor() :
		enabled(false), LinkGroupingID(0), LinkTypeExtension(0), LinkType(0),
		IncludedPorts(0), Interface(IF_RESERVED), ChannelNumber(0) {
}

PayloadManager::LinkDescriptor::LinkDescriptor(
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

PayloadManager::LinkDescriptor::LinkDescriptor(const std::vector<uint8_t> &bytes, bool enabled) :
	enabled(enabled) {
	if (bytes.size() < 4)
		throw std::domain_error("A Link Descriptor must be a four byte field.");
	this->LinkGroupingID = bytes[3];
	this->LinkTypeExtension = bytes[2] >> 4;
	this->LinkType = ((bytes[2] & 0x0F) << 4) | ((bytes[1] & 0xF0) >> 4);
	this->IncludedPorts = bytes[1] & 0x0F;
	this->Interface = static_cast<Interfaces>(bytes[0] >> 6);
	this->ChannelNumber = bytes[0] & 0x3F;
}

PayloadManager::LinkDescriptor::operator std::vector<uint8_t>() const {
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

/**
 * Register or look up an OEM LinkType GUID, and return the LinkType index
 * associated with it.
 *
 * @param oem_guid The GUID (16 bytes) to register.
 * @return The LinkType index associated with this GUID.
 * @throws std::domain_error if an invalid GUID is supplied
 * @throws std::out_of_range if there is no more space in the table
 */
uint8_t PayloadManager::LinkDescriptor::map_oem_LinkType_guid(const std::vector<uint8_t> &oem_guid) {
	if (oem_guid.size() != 16)
		throw std::domain_error("OEM LinkType GUIDs are 16 byte values.");
	safe_init_static_mutex(oem_guid_mutex, false);
	MutexGuard<false> lock(oem_guid_mutex, true);
	for (auto it = oem_guids.begin(), eit = oem_guids.end(); it != eit; ++it)
		if (it->second == oem_guid)
			return it->first;

	// Or if not found, attempt to register.
	for (uint8_t mapping = 0xF0; mapping < 0xFF; ++mapping) {
			if (oem_guids.count(mapping))
				continue;
			oem_guids[mapping] = oem_guid;
			return mapping;
	}

	// Or, if there was simply no room left:
	throw std::out_of_range("No remaining OEM LinkType GUID slots available. (Only 15 can be specified in FRU Data, by §3.7.2.3 ¶318)");
}

/**
 * Looks up an OEM LinkType index and converts it to the appropriate OEM GUID.
 * @param LinkType The LinkType index
 * @return The OEM GUID
 * @throws std::out_of_range if the LinkType is not registered.
 */
std::vector<uint8_t> PayloadManager::LinkDescriptor::lookup_oem_LinkType_guid(uint8_t LinkType) {
	safe_init_static_mutex(oem_guid_mutex, false);
	MutexGuard<false> lock(oem_guid_mutex, true);
	return oem_guids.at(LinkType);
}
SemaphoreHandle_t PayloadManager::LinkDescriptor::oem_guid_mutex = NULL;
std::map< uint8_t, std::vector<uint8_t> > PayloadManager::LinkDescriptor::oem_guids;

void PayloadManager::update_link_enable(const LinkDescriptor &descriptor) {
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
			if (it->enabled)
				this->log.log(stdsprintf("E-Keying port enabled on Interface %hhu, Channel %hhu.", static_cast<uint8_t>(it->Interface), it->ChannelNumber), LogTree::LOG_INFO);
			else
				this->log.log(stdsprintf("E-Keying port disabled on Interface %hhu, Channel %hhu.", static_cast<uint8_t>(it->Interface), it->ChannelNumber), LogTree::LOG_INFO);
		}
	}
}

std::vector<PayloadManager::LinkDescriptor> PayloadManager::get_links() const {
	MutexGuard<true> lock(this->mutex, true);
	return this->links;
}

void PayloadManager::run_sensor_thread() {
	while (true) {
		// Wait for scheduled or priority execution, then read any events.
		TickType_t timeout = pdMS_TO_TICKS(100);
		bool event_received;
		do {
			SensorProcessor::Event event;
			event_received = this->sensor_processor->get_isr_event(event, timeout);
			if (event_received) {
				for (auto &adcsensor : this->adc_sensors) {
					if (adcsensor.second.sensor_processor_id >= 0 && (uint32_t)adcsensor.second.sensor_processor_id == event.channel) {
						std::shared_ptr<ThresholdSensor> ipmisensor = adcsensor.second.ipmi_sensor.lock();
						if (!ipmisensor) {
							this->log.log(stdsprintf("Sensor %s (Proc[%d]) has no matching Sensor object (are SDRs configured correctly?), not processing received event for this sensor.", adcsensor.second.name.c_str(), adcsensor.second.sensor_processor_id), LogTree::LOG_ERROR);
							continue;
						}
						bool in_context = true;
						if (adcsensor.second.mz_context) {
							bool transitioning = false;
							bool active = this->mgmt_zones[adcsensor.second.mz_context]->get_power_state(&transitioning);
							in_context = active && !transitioning;
						}
						ipmisensor->update_value(adcsensor.second.adc.convert_from_raw(event.reading_from_isr), in_context, UINT64_MAX, event.event_thresholds_assert, event.event_thresholds_deassert);
					}
				}
			}
			timeout = 0; // Now to process any remaining queued events promptly before proceeding on.
		} while (event_received);

		// TODO: Update and alert on hardfault detection.
		//mgmt_zones[0]->get_hardfault_status();
		//mgmt_zones[0]->get_power_state();

		MutexGuard<true> lock(this->mutex, true);
		for (auto &adcsrec : this->adc_sensors) {
			std::shared_ptr<ThresholdSensor> ipmisensor = adcsrec.second.ipmi_sensor.lock();
			if (!ipmisensor)
				continue; // Can't update an unlinked sensor.

			float reading = (float)adcsrec.second.adc;
			bool in_context = true;
			if (adcsrec.second.mz_context) {
				bool transitioning = false;
				bool active = this->mgmt_zones[adcsrec.second.mz_context]->get_power_state(&transitioning);
				in_context = active && !transitioning;
			}
			ipmisensor->update_value(reading, in_context);
		}
	}
}

/**
 * This refreshes the ADC Sensor to IPMI Sensor linkage by doing a name-based
 * lookup for each ADC Sensor in the global ipmc_sensors SensorSet.
 */
void PayloadManager::refresh_sensor_linkage() {
	MutexGuard<true> lock(this->mutex, true);
	for (auto& adcsensor : this->adc_sensors) {
		std::shared_ptr<ThresholdSensor> sensor = std::dynamic_pointer_cast<ThresholdSensor>(ipmc_sensors.find_by_name(adcsensor.second.name));
		adcsensor.second.ipmi_sensor = sensor;
		if (adcsensor.second.sensor_processor_id < 0)
			continue;
		if (sensor) {
			std::shared_ptr<const SensorDataRecordReadableSensor> sdr = std::dynamic_pointer_cast<const SensorDataRecordReadableSensor>(device_sdr_repo.find(sensor->sdr_key));
			if (sdr) {
				// Disable all event enables not common to the active and new sets during reconfiguration.
				uint16_t assert, deassert;
				this->sensor_processor->get_event_enable(adcsensor.second.sensor_processor_id, assert, deassert);
				this->sensor_processor->set_event_enable(adcsensor.second.sensor_processor_id, assert & sdr->ext_assertion_events_enabled(), deassert & sdr->ext_deassertion_events_enabled());

				// This is only going to work on increasing linear sensors, but that's all we can support at the moment.
				uint16_t hystunit = adcsensor.second.adc.convert_to_raw(sdr->to_float(1)) - adcsensor.second.adc.convert_to_raw(sdr->to_float(0));
				this->sensor_processor->set_hysteresis(adcsensor.second.sensor_processor_id, sdr->hysteresis_high() * hystunit, sdr->hysteresis_low() * hystunit);

				Thr_Cfg thresholds;
				thresholds.LNR = (sensor->thresholds.lnr == 0) ? 0 : adcsensor.second.adc.convert_to_raw(sdr->to_float(sensor->thresholds.lnr));
				thresholds.LCR = (sensor->thresholds.lcr == 0) ? 0 : adcsensor.second.adc.convert_to_raw(sdr->to_float(sensor->thresholds.lcr));
				thresholds.LNC = (sensor->thresholds.lnc == 0) ? 0 : adcsensor.second.adc.convert_to_raw(sdr->to_float(sensor->thresholds.lnc));
				thresholds.UNC = (sensor->thresholds.unc == 0xFF) ? 0xFFFF : adcsensor.second.adc.convert_to_raw(sdr->to_float(sensor->thresholds.unc));
				thresholds.UCR = (sensor->thresholds.ucr == 0xFF) ? 0xFFFF : adcsensor.second.adc.convert_to_raw(sdr->to_float(sensor->thresholds.ucr));
				thresholds.UNR = (sensor->thresholds.unr == 0xFF) ? 0xFFFF : adcsensor.second.adc.convert_to_raw(sdr->to_float(sensor->thresholds.unr));
				this->sensor_processor->set_thresholds(adcsensor.second.sensor_processor_id, thresholds);

				this->sensor_processor->set_event_enable(adcsensor.second.sensor_processor_id, sdr->ext_assertion_events_enabled(), sdr->ext_deassertion_events_enabled());
			}
			else {
				sensor->log.log(stdsprintf("Sensor %s (Proc[%d]) has no matching SDR.  Not updating hardfault configuration configuration for this sensor.", adcsensor.second.name.c_str(), adcsensor.second.sensor_processor_id), LogTree::LOG_WARNING);
			}
		}
		else {
			this->log.log(stdsprintf("Sensor %s (Proc[%d]) has no matching Sensor object (are SDRs configured correctly?), not updating hardfault configuration for this sensor.", adcsensor.second.name.c_str(), adcsensor.second.sensor_processor_id), LogTree::LOG_ERROR);
		}
	}
}

// No anonymous namespace because it needs to be declared friend.
/// A backend power switch command
class ConsoleCommand_PayloadManager_power_level : public CommandParser::Command {
public:
	PayloadManager &payloadmgr;

	ConsoleCommand_PayloadManager_power_level(PayloadManager &payloadmgr) : payloadmgr(payloadmgr) { };
	virtual std::string get_helptext(const std::string &command) const {
		return stdsprintf(
				"%s [$new_power_level [$force]]\n"
				"  $new_power_level corresponds to an IPMI payload power level:\n"
				"    0 = off\n"
				"    1 = all backend power on\n"
				"  $force = \"true\" orders the IPMC to disregard the currently negotiated maximum power level\n"
				"\n"
				"This command changes our backend power enables without affecting or overriding IPMI state.\n"
				"\n"
				"Without parameters, this will return power status.\n", command.c_str());
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		if (parameters.nargs() == 1) {
			unsigned int negotiated_power_watts = 0;
			if (this->payloadmgr.power_properties.current_power_level)
				negotiated_power_watts = this->payloadmgr.power_properties.power_levels[this->payloadmgr.power_properties.current_power_level-1];
			negotiated_power_watts *= this->payloadmgr.power_properties.power_multiplier;
			uint32_t pen_state = this->payloadmgr.mgmt_zones[0]->get_pen_status(false);
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
		if (!parameters.parse_parameters(1, (parameters.nargs() == 2), &new_level)) {
			console->write("Invalid parameters.\n");
			return;
		}
		// Parse $force parameter.
		if (parameters.nargs() >= 3 && !parameters.parse_parameters(2, true, &force)) {
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
		this->payloadmgr.implement_power_level(new_level);
	}

	//virtual std::vector<std::string> complete(const CommandParser::CommandParameters &parameters) const { };
};

/// A management zone power switch command
class ConsoleCommand_PayloadManager_mz_control : public CommandParser::Command {
public:
	PayloadManager &payloadmgr;

	ConsoleCommand_PayloadManager_mz_control(PayloadManager &payloadmgr) : payloadmgr(payloadmgr) { };
	virtual std::string get_helptext(const std::string &command) const {
		return stdsprintf(
				"%s [$mz_number [$on_off]]\n"
				"\n"
				"This command changes our MZ enables without affecting or overriding IPMI state.\n"
				"\n"
				"Without parameters, this will return all MZ status.\n", command.c_str());
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		if (parameters.nargs() == 1) {
			// Show all MZ status
			for (size_t i = 0; i < XPAR_MGMT_ZONE_CTRL_0_MZ_CNT; i++) {
				bool active = this->payloadmgr.mgmt_zones[i]->get_power_state();
				console->write(stdsprintf("MZ %d is currently %s.\n", i, (active?"ON":"OFF")));
			}
		} else {
			uint8_t mz_number;
			// Parse $mz_number parameter.
			if (!parameters.parse_parameters(1, false, &mz_number)) {
				console->write("Invalid parameters.\n");
				return;
			}

			if (mz_number >= XPAR_MGMT_ZONE_CTRL_0_MZ_CNT) {
				console->write("MZ number out-of-range.\n");
				return;
			}

			if (parameters.nargs() == 2) {
				// Show MZ status
				bool active = this->payloadmgr.mgmt_zones[mz_number]->get_power_state();
				console->write(stdsprintf("MZ %d is currently %s.\n", mz_number, (active?"ON":"OFF")));
			} else {
				MGMT_Zone::PowerAction action = MGMT_Zone::OFF;

				if (!parameters.parameters[2].compare("on")) {
					action = MGMT_Zone::ON;
				} else if (!parameters.parameters[2].compare("off")) {
					action = MGMT_Zone::OFF;
				} else {
					console->write("$on_off needs to be 'on' or 'off'.\n");
					return;
				}

				this->payloadmgr.mgmt_zones[mz_number]->set_power_state(action);
			}
		}

	}

	//virtual std::vector<std::string> complete(const CommandParser::CommandParameters &parameters) const { };
};

namespace {
/// A sensor readout command
class ConsoleCommand_read_ipmi_sensors : public CommandParser::Command {
public:
	virtual std::string get_helptext(const std::string &command) const {
		return stdsprintf(
				"%s\n"
				"\n"
				"Read out the status of all IPMI sensors\n", command.c_str());
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		std::string out;
		for (auto &sensorinfo : SensorSet::container_type(ipmc_sensors)) {
			{ // Process if ThresholdSensor
				std::shared_ptr<ThresholdSensor> sensor = std::dynamic_pointer_cast<ThresholdSensor>(sensorinfo.second);
				if (sensor) {
					ThresholdSensor::Value value = sensor->get_value();
					out += stdsprintf("%30s %f (raw %hhu; event %hu; %s context)\n",
							sensorinfo.second->sensor_identifier().c_str(),
							value.float_value,
							value.byte_value,
							value.active_thresholds,
							(value.in_context ? "in" : "out of"));
					continue;
				}
			}
			{ // Process if HotswapSensor
				std::shared_ptr<HotswapSensor> sensor = std::dynamic_pointer_cast<HotswapSensor>(sensorinfo.second);
				if (sensor) {
					out += stdsprintf("%30s M%hhu\n",
							sensorinfo.second->sensor_identifier().c_str(),
							sensor->get_mstate());
					continue;
				}
			}
		}
		console->write(out);
	}

	//virtual std::vector<std::string> complete(const CommandParser::CommandParameters &parameters) const { };
};
} // anonymous namespace

/**
 * Register console commands related to the PayloadManager.
 * @param parser The command parser to register to.
 * @param prefix A prefix for the registered commands.
 */
void PayloadManager::register_console_commands(CommandParser &parser, const std::string &prefix) {
	parser.register_command(prefix + "power_level", std::make_shared<ConsoleCommand_PayloadManager_power_level>(*this));
	parser.register_command(prefix + "mz_control", std::make_shared<ConsoleCommand_PayloadManager_mz_control>(*this));
	parser.register_command(prefix + "read_ipmi_sensors", std::make_shared<ConsoleCommand_read_ipmi_sensors>());
}

/**
 * Unregister console commands related to the PayloadManager.
 * @param parser The command parser to unregister from.
 * @param prefix A prefix for the registered commands.
 */
void PayloadManager::deregister_console_commands(CommandParser &parser, const std::string &prefix) {
	parser.register_command(prefix + "power_level", NULL);
	parser.register_command(prefix + "mz_control", NULL);
	parser.register_command(prefix + "read_ipmi_sensors", NULL);
}
