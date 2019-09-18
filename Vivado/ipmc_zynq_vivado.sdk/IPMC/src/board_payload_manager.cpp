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

#include "board_payload_manager.h"

BoardPayloadManager::BoardPayloadManager(MStateMachine *mstate_machine, LogTree &log) :
	PayloadManager(mstate_machine, log) {
}

BoardPayloadManager::~BoardPayloadManager() {
}

void BoardPayloadManager::config() {
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

	ZoneController *zonectrl = new ZoneController(XPAR_MGMT_ZONE_CTRL_0_DEVICE_ID);
	zonectrl->registerConsoleCommands(console_command_parser, "zonectrl.");

	// Set up Management Zones
	for (int i = 0; i < XPAR_MGMT_ZONE_CTRL_0_MZ_CNT; ++i)
		this->mgmt_zones[i] = new ZoneController::Zone(*zonectrl, i);

	SuspendGuard suspend(true);
	this->mstate_machine->deactivate_payload = [this]() -> void {
		// Turn off power.
		this->setPowerLevel(0, 0);

		// Disable all E-Keying links.
		// (IPMI commands will not be sent for this when proceeding through M6)
		std::vector<LinkDescriptor> links = this->getLinks();
		for (auto it = links.begin(), eit = links.end(); it != eit; ++it) {
			if (it->enabled) {
				it->enabled = false;
				this->updateLinkEnable(*it);
			}
		}

		// In our specific backend implementation the above is all synchronous.
		this->mstate_machine->payloadDeactivationComplete();
	};
	suspend.release();

	auto calc_hf_mask = [this](std::vector<std::string> sensors) -> uint64_t {
		uint64_t mask = 0;
		for (std::string& name : sensors) {
			try {
				ADCSensor &sensor = PayloadManager::adc_sensors.at(name);
				if (sensor.sensor_processor_id >= 0)
					mask |= 1 << sensor.sensor_processor_id;
			}
			catch (std::out_of_range) {
				this->log.log(stdsprintf("Unable find sensor %s in payload_manager->adc_sensors: No hardfault protection available.", name.c_str()), LogTree::LOG_ERROR);
			}
		}
		return mask;
	};

	std::vector<ZoneController::Zone::OutputConfig> pen_config;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
	const uint64_t hfm_TEMP = calc_hf_mask({"T_TOP", "T_BOT"});
	const uint64_t hfm_PWRENA_0 = calc_hf_mask({"+12VPYLD"}); // PWREN_12VPYLD
	const uint64_t hfm_PWRENA_1 = calc_hf_mask({"+2.5VETH"}); // PWREN_2V5ETH
	const uint64_t hfm_PWRENA_2 = calc_hf_mask({"+1.0VETH"}); // PWREN_1V0ETH
	const uint64_t hfm_PWRENA_3 = calc_hf_mask({"+3.3VPYLD"}); // PWREN_3V3PYLD
	const uint64_t hfm_PWRENA_4 = calc_hf_mask({"+5VPYLD"}); // PWREN_5V0PYLD
	const uint64_t hfm_PWRENA_5 = calc_hf_mask({"+1.2VPHY"}); // PWREN_1V2PHY
	const uint64_t hfm_PWRENA_6 = calc_hf_mask({"+12VPYLD"}); // ELM_PWR_EN

#pragma GCC diagnostic pop

	this->mz_hf_vectors[0] = hfm_TEMP | hfm_PWRENA_0;
	this->mz_hf_vectors[1] = this->mz_hf_vectors[0] | hfm_PWRENA_1 | hfm_PWRENA_2; // Ethernet Switch
	this->mz_hf_vectors[2] = this->mz_hf_vectors[0] | hfm_PWRENA_3 | hfm_PWRENA_4; // Peripherals
	this->mz_hf_vectors[3] = this->mz_hf_vectors[0] | hfm_PWRENA_5; // 10G Phy

	// Zone 0 and 2 need to be on for ELM to turn on
	this->mz_hf_vectors[4] = this->mz_hf_vectors[0] | this->mz_hf_vectors[2];

	{
		// Management Zone 0, +12V power
		this->mgmt_zones[0]->setName("+12V Power");
		this->mgmt_zones[0]->setHardfaultMask(this->mz_hf_vectors[0], 0);
		this->mgmt_zones[0]->getPowerEnableConfig(pen_config);

		// PWRENA_0#: +12VPYLD
		pen_config[0].drive_enabled = true;
		pen_config[0].active_high = true;
		pen_config[0].enable_delay = 0;

		this->mgmt_zones[0]->setPowerEnableConfig(pen_config);
	}

	{
		// Management Zone 1, Ethernet Switch power
		this->mgmt_zones[1]->setName("Ethernet Switch");
		this->mgmt_zones[1]->setHardfaultMask(this->mz_hf_vectors[1], 0);
		this->mgmt_zones[1]->getPowerEnableConfig(pen_config);

		// PWRENA_1: +2.5VETH
		pen_config[1].drive_enabled = true;
		pen_config[1].active_high = true;
		pen_config[1].enable_delay = 10;

		// PWRENA_2: +1.0VETH
		pen_config[2].drive_enabled = true;
		pen_config[2].active_high = true;
		pen_config[2].enable_delay = 10;

		this->mgmt_zones[1]->setPowerEnableConfig(pen_config);
	}

	{
		// Management Zone 2, SSD, Firefly and USB power
		this->mgmt_zones[2]->setName("SSD/Firefly/USB");
		this->mgmt_zones[2]->setHardfaultMask(this->mz_hf_vectors[2], 0);
		this->mgmt_zones[2]->getPowerEnableConfig(pen_config);

		// PWRENA_3: +3.3VPYLD
		pen_config[3].drive_enabled = true;
		pen_config[3].active_high = true;
		pen_config[3].enable_delay = 20;

		// PWRENA_4: +5VPYLD
		pen_config[4].drive_enabled = true;
		pen_config[4].active_high = true;
		pen_config[4].enable_delay = 20;

		this->mgmt_zones[2]->setPowerEnableConfig(pen_config);
	}

	{
		// Management Zone 3, 10G PHY power
		this->mgmt_zones[3]->setName("10G PHY");
		this->mgmt_zones[3]->setHardfaultMask(this->mz_hf_vectors[3], 0);
		this->mgmt_zones[3]->getPowerEnableConfig(pen_config);

		// PWRENA_5: +1.2VPHY
		pen_config[5].drive_enabled = true;
		pen_config[5].active_high = true;
		pen_config[5].enable_delay = 30;

		this->mgmt_zones[3]->setPowerEnableConfig(pen_config);
	}

	{
		// Management Zone 4, ELM power enable
		this->mgmt_zones[4]->setName("ELM Power Enable");
		this->mgmt_zones[4]->setHardfaultMask(this->mz_hf_vectors[4], 0);
		this->mgmt_zones[4]->getPowerEnableConfig(pen_config);

		// PWRENA_6: ELM_PWR_EN
		pen_config[6].drive_enabled = true;
		pen_config[6].active_high = true;
		pen_config[6].enable_delay = 40;

		this->mgmt_zones[4]->setPowerEnableConfig(pen_config);
	}

	// Finalize configuration
	this->finishConfig();
}

/**
 * Retrieve the current power properties & negotiated status for the payload.
 *
 * @param fru The FRU to operate on.
 * @param recompute true if Compute Power Properties is requesting that power
 *                  properties be recomputed if desired, else false.
 * @return A set of power properties for IPMI.
 */
PowerProperties BoardPayloadManager::getPowerProperties(uint8_t fru, bool recompute) {
	if (fru != 0) {
		throw std::domain_error("This FRU is not known.");
	}

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
 * @param fru The FRU to manage.
 * @param level The pre-calculated power level to set.
 */
void BoardPayloadManager::setPowerLevel(uint8_t fru, uint8_t level) {
	if (fru != 0)
		throw std::domain_error("This FRU is not known.");

	MutexGuard<true> lock(this->mutex, true);

	this->power_properties.current_power_level = level;
	if (level == 0) {
		// Power OFF!
		this->log.log("Power Level set to 0 by shelf.", LogTree::LOG_INFO);
		this->implementPowerLevel(0);
		this->mstate_machine->payloadDeactivationComplete();
	}
	else if (level == 1) {
		// We only support one non-off power state.
		this->log.log("Power Level set to 1 by shelf.", LogTree::LOG_INFO);
		this->implementPowerLevel(1);
		this->mstate_machine->payloadActivationComplete();
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
void BoardPayloadManager::implementPowerLevel(uint8_t level) {
	MutexGuard<true> lock(this->mutex, true);

	if (level == 0) {
		// Power OFF!
		this->log.log("Implement Power Level 0: Shutting down.", LogTree::LOG_DIAGNOSTIC);

		// We need to put things out of context in advance, so they don't fault at the start of the sequence.
		for (int i = 4; i >= 0; --i)
			this->mgmt_zones[i]->resetLastTransitionStart();
		this->updateSensorProcessorContexts();

		for (int i = 4; i >= 0; --i) {
			/* These are sequenced with delays in firmware so that all can be
			 * disabled at once and the right things will happen.
			 */
			this->mgmt_zones[i]->setPowerState(ZoneController::Zone::OFF);
		}
		this->log.log("Implement Power Level 0: Shutdown complete.", LogTree::LOG_DIAGNOSTIC);

		struct IPMILED::Action ledstate;
		ledstate.min_duration = 0;
		ledstate.effect = IPMILED::OFF;
		ipmi_leds[2]->submit(ledstate);
	} else if (level == 1) {
		// We only support one non-off power state.
		this->log.log("Implement Power Level 1: Powering up backend.", LogTree::LOG_DIAGNOSTIC);
		for (int i = 0; i <= 4; ++i) {
			/* These are sequenced with delays in firmware so that all can be
			 * enabled at once and the right things will happen.
			 */
			this->mgmt_zones[i]->setPowerState(ZoneController::Zone::ON);
		}
		// We need to start the clock on putting these zones back into context for fault detection.
		this->updateSensorProcessorContexts();
		// If we were waiting in M3, go to M4. (Skipping E-Keying for now)
		this->log.log("Implement Power Level 1: Backend powered up.", LogTree::LOG_DIAGNOSTIC);
		struct IPMILED::Action ledstate;
		ledstate.min_duration = 0;
		ledstate.effect = IPMILED::ON;
		ipmi_leds[2]->submit(ledstate);
		this->mstate_machine->payloadActivationComplete();
	}

	this->power_properties.current_power_level = level;
}
