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
#include <exception>

PayloadManager::PayloadManager(MStateMachine *mstate_machine, LogTree &log)
	: mstate_machine(mstate_machine), log(log) {
	for (int i = 0; i < XPAR_MGMT_ZONE_CTRL_0_MZ_CNT; ++i)
		this->mgmt_zones[i] = new MGMT_Zone(XPAR_MGMT_ZONE_CTRL_0_DEVICE_ID, i);


	std::vector<MGMT_Zone::OutputConfig> pen_config;
	uint64_t hf_mask = 0
			| (1<<0) // PGOOD_2V5ETH
			| (1<<1) // PGOOD_1V0ETH
			| (1<<2) // PGOOD_3V3PYLD
			| (1<<3) // PGOOD_5V0PYLD
			| (1<<4);// PGOOD_1V2PHY
#warning "Hardfault Safety is Disabled (Hacked out)"
	hf_mask = 0; // XXX Disable Hardfault Safety
	this->mgmt_zones[0]->set_hardfault_mask(hf_mask, 140);
	this->mgmt_zones[0]->get_pen_config(pen_config);
	for (int i = 0; i < 6; ++i) {
		pen_config[i].active_high = true;
		pen_config[i].drive_enabled = true;
	}
	// +12VPYLD
	pen_config[0].enable_delay = 10;
	// +2V5ETH
	pen_config[1].enable_delay = 20;
	// +1V0ETH
	pen_config[2].enable_delay = 20;
	// +3V3PYLD
	// +1V8PYLD
	// +3V3FFTX_TX
	// +3V3FFTX_RX
	// +3V3FFRX_TX
	// +3V3FFRX_RX
	pen_config[3].enable_delay = 30;
	// +5V0PYLD
	pen_config[4].enable_delay = 30;
	// +1V2PHY
	pen_config[5].enable_delay = 40;
	this->mgmt_zones[0]->set_pen_config(pen_config);
	// TODO: confirm unnecessary: this->mgmt_zones[0]->set_power_state(MGMT_Zone::ON); // Immediately power up.  Xil ethernet driver asserts otherwise.

	hf_mask = 0
			| (1<<5);// ELM_PFAIL
#warning "Hardfault Safety is Disabled (Hacked out)"
	hf_mask = 0; // XXX Disable Hardfault Safety
	this->mgmt_zones[1]->set_hardfault_mask(hf_mask, 150);
	this->mgmt_zones[1]->get_pen_config(pen_config);
	// ELM_PWR_EN_I
	pen_config[6].active_high = true;
	pen_config[6].drive_enabled = true;
	pen_config[6].enable_delay = 50;
	this->mgmt_zones[1]->set_pen_config(pen_config);
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

void PayloadManager::set_power_level(uint8_t fru, uint8_t level) {
	if (fru != 0)
		throw std::domain_error("This FRU is not known.");

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

void PayloadManager::implement_power_level(uint8_t level) {
	if (level == 0) {
		// Power OFF!
		this->log.log("Implement Power Level 0: Shutting down.", LogTree::LOG_DIAGNOSTIC);
		// Shut down ELM & wait
		this->mgmt_zones[1]->set_power_state(MGMT_Zone::OFF);
		vTaskDelay(50); // The total delay in the PEN config is 50ms.
		// Shut down ETH & wait
		this->mgmt_zones[0]->set_power_state(MGMT_Zone::OFF);
		vTaskDelay(40); // The total delay in the PEN config is 40ms.
		// If we were waiting in M3, go to M4. (Skipping E-Keying for now)
		this->log.log("Implement Power Level 0: Shutdown complete.", LogTree::LOG_DIAGNOSTIC);
		this->mstate_machine->payload_deactivation_complete();
	}
	else if (level == 1) {
		// We only support one non-off power state.
		this->log.log("Implement Power Level 1: Powering up backend.", LogTree::LOG_DIAGNOSTIC);
		this->mgmt_zones[0]->set_power_state(MGMT_Zone::ON);
		this->mgmt_zones[1]->set_power_state(MGMT_Zone::ON);
		// If we were waiting in M3, go to M4. (Skipping E-Keying for now)
		this->log.log("Implement Power Level 1: Backend powered up.", LogTree::LOG_DIAGNOSTIC);
		this->mstate_machine->payload_activation_complete();
	}
}

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

/**
 * Register console commands related to the PayloadManager.
 * @param parser The command parser to register to.
 * @param prefix A prefix for the registered commands.
 */
void PayloadManager::register_console_commands(CommandParser &parser, const std::string &prefix) {
	parser.register_command(prefix + "power_level", std::make_shared<ConsoleCommand_PayloadManager_power_level>(*this));
}

/**
 * Unregister console commands related to the PayloadManager.
 * @param parser The command parser to unregister from.
 * @param prefix A prefix for the registered commands.
 */
void PayloadManager::deregister_console_commands(CommandParser &parser, const std::string &prefix) {
	parser.register_command(prefix + "power_level", NULL);
}
