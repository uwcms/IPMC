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

// Only include driver if Management Zone IP is detected in the BSP.
#if XSDK_INDEXING || __has_include("mgmt_zone_ctrl.h")

#include "management_zone.h"
#include <libs/printf.h>
#include <libs/except.h>
#include <libs/threading.h>

ZoneController::ZoneController(uint16_t device_id) {
	if (XST_SUCCESS != Mgmt_Zone_Ctrl_Initialize(&this->zone, device_id)) {
		throw except::hardware_error("Unable to initialize ZoneController(device_id=" + std::to_string(device_id) + ")");
	}
}

ZoneController::~ZoneController() {
}

ZoneController::Zone::Zone(ZoneController &zone_ctrl, size_t zone_number)
	: zonectrl(zone_ctrl), kZoneNumber(zone_number), name(""), desired_power_state(false), last_transition_start_ts(0) {
	if (zone_number >= zone_ctrl.zone.mz_cnt) {
		throw std::out_of_range("Target zone number is out-of-range");
	}
};

void ZoneController::Zone::setHardfaultMask(uint64_t mask, uint32_t holdoff) {
	MZ_config config;
	Mgmt_Zone_Ctrl_Get_MZ_Cfg(&this->zonectrl.zone, this->kZoneNumber, &config);
	config.hardfault_mask = mask;
	config.fault_holdoff = holdoff;
	Mgmt_Zone_Ctrl_Set_MZ_Cfg(&this->zonectrl.zone, this->kZoneNumber, &config);
}

void ZoneController::Zone::getHardfaultMask(uint64_t *mask, uint32_t *holdoff) {
	MZ_config config;
	Mgmt_Zone_Ctrl_Get_MZ_Cfg(&this->zonectrl.zone, this->kZoneNumber, &config);
	if (mask)
		*mask = config.hardfault_mask;
	if (holdoff)
		*holdoff = config.fault_holdoff;
}

uint64_t ZoneController::Zone::getHardfaultStatus(bool apply_mask) {
	uint64_t mask = UINT64_MAX;
	if (apply_mask)
		this->getHardfaultMask(&mask, nullptr);
	return mask & Mgmt_Zone_Ctrl_Get_Hard_Fault_Status(&this->zonectrl.zone);
}


void ZoneController::Zone::setPowerEnableConfig(const std::vector<OutputConfig> &pen_config) {
	if (pen_config.size() != this->zonectrl.getPowerEnableCount()) {
		throw std::logic_error(stdsprintf("Supplied PEN config vector specifies an incorrect number of PENs (%u/%lu) for MZ %u", pen_config.size(), this->zonectrl.getPowerEnableCount(), this->kZoneNumber));
	}

	MZ_config config;
	Mgmt_Zone_Ctrl_Get_MZ_Cfg(&this->zonectrl.zone, this->kZoneNumber, &config);
	for (uint32_t i = 0; i < this->zonectrl.getPowerEnableCount(); ++i) {
		config.pwren_cfg[i] =
				((pen_config[i].drive_enabled)<<17) |
				((pen_config[i].active_high  )<<16) |
				((pen_config[i].enable_delay )<< 0) ;
	}
	Mgmt_Zone_Ctrl_Set_MZ_Cfg(&this->zonectrl.zone, this->kZoneNumber, &config);
}

void ZoneController::Zone::getPowerEnableConfig(std::vector<OutputConfig> &pen_config) {
	pen_config.clear();
	MZ_config config;
	Mgmt_Zone_Ctrl_Get_MZ_Cfg(&this->zonectrl.zone, this->kZoneNumber, &config);
	for (uint32_t i = 0; i < this->zonectrl.getPowerEnableCount(); ++i)
		pen_config.emplace_back(config.pwren_cfg[i] & (1<<16), config.pwren_cfg[i] & (1<<17), config.pwren_cfg[i]&0xffff);
}

uint32_t ZoneController::Zone::getPowerEnableStatus(bool apply_mask) {
	uint32_t mask = UINT32_MAX;
	if (apply_mask) {
		std::vector<OutputConfig> pencfg;
		this->getPowerEnableConfig(pencfg);
		mask = 0;
		for (uint32_t i = 0; i < this->zonectrl.getPowerEnableCount(); ++i)
			if (pencfg[i].drive_enabled)
				mask |= (1 << i);
	}
	return mask & Mgmt_Zone_Ctrl_Get_Pwr_En_Status(&this->zonectrl.zone);
}

void ZoneController::Zone::setPowerState(PowerAction action) {
	switch (action) {
	case ON:
		Mgmt_Zone_Ctrl_Pwr_ON_Seq(&this->zonectrl.zone, this->kZoneNumber);
		this->desired_power_state = true;
		this->last_transition_start_ts = get_tick64();
		break;
	case OFF:
		Mgmt_Zone_Ctrl_Pwr_OFF_Seq(&this->zonectrl.zone, this->kZoneNumber);
		this->desired_power_state = false;
		this->last_transition_start_ts = get_tick64();
		break;
	case KILL:
		Mgmt_Zone_Ctrl_Dispatch_Soft_Fault(&this->zonectrl.zone, this->kZoneNumber);
		break;
	default:
		throw std::domain_error(stdsprintf("Invalid PowerAction %u supplied to set_power_state() for MZ %u", static_cast<unsigned int>(action), this->kZoneNumber));
	}
}

bool ZoneController::Zone::getPowerState(bool *in_transition) {
	bool active;
	bool transitioning;

	switch (Mgmt_Zone_Ctrl_Get_MZ_Status(&this->zonectrl.zone, this->kZoneNumber)) {
	case MZ_PWR_OFF:       active = false; transitioning = false; break;
	case MZ_PWR_ON:        active = true;  transitioning = false; break;
	case MZ_PWR_TRANS_OFF: active = false; transitioning = true;  break;
	case MZ_PWR_TRANS_ON:  active = true;  transitioning = true;  break;
	default:               throw except::hardware_error("Invalid power state read from MGMT_Zone driver"); break;
	}
	if (in_transition)
		*in_transition = transitioning;
	return active;
}

class ZoneController::Override final : public CommandParser::Command {
public:
	Override(ZoneController &zonectrl) : zonectrl(zonectrl) { };

	virtual std::string get_helptext(const std::string &command) const {
		return command + " reset|enable|disable|drive|level enable_number [0|1]\n\n"
				"Enable, drive and set the override mode of specific power enable pins.\n";
	}
	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		// TODO: Uncomment when IP gets updated, otherwise this won't compile
//		if (parameters.nargs() == 1) {
//			uint32_t enables = Mgmt_Zone_Ctrl_Get_Enable_Override(&this->zonectrl.zone);
//			uint32_t drive = Mgmt_Zone_Ctrl_Get_Override_Drive(&this->zonectrl.zone);
//			uint32_t level = Mgmt_Zone_Ctrl_Get_Override_Level(&this->zonectrl.zone);
//
//			console->write(stdsprintf("Override Enables: 0x%08lx\n", enables));
//			console->write(stdsprintf("Override Drive:   0x%08lx\n", drive));
//			console->write(stdsprintf("Override Level:   0x%08lx\n", level));
//
//			return;
//		}
//
//		if (parameters.nargs() < 2) {
//			console->write("Invalid arguments, see help.\n");
//			return;
//		}
//
//		std::string option;
//		uint32_t pwren;
//
//		if (!parameters.parse_parameters(1, false, &option, &pwren)) {
//			console->write("Invalid arguments, see help.\n");
//			return;
//		}
//
//		if (pwren >= this->zonectrl.getPowerEnablesCount()) {
//			console->write("Power enable pin out-of-range, see help.\n");
//			return;
//		}
//
//		if (!option.compare("reset")) {
//			Mgmt_Zone_Ctrl_Set_Enable_Override(&this->zonectrl.zone, 0);
//			Mgmt_Zone_Ctrl_Set_Override_Drive(&this->zonectrl.zone, 0);
//			Mgmt_Zone_Ctrl_Set_Override_Level(&this->zonectrl.zone, 0);
//		} else if (!option.compare("disable")) {
//			uint32_t enables = Mgmt_Zone_Ctrl_Get_Enable_Override(&this->zonectrl.zone);
//			enables |= (1 << pwren);
//			Mgmt_Zone_Ctrl_Set_Enable_Override(&this->zonectrl.zone, enables);
//
//		} else if (!option.compare("disable")) {
//			uint32_t enables = Mgmt_Zone_Ctrl_Get_Enable_Override(&this->zonectrl.zone);
//			enables &= ~(1 << pwren);
//			Mgmt_Zone_Ctrl_Set_Enable_Override(&this->zonectrl.zone, enables);
//
//		} else if (!option.compare("drive")) {
//			uint32_t val = 0;
//			if (!parameters.parse_parameters(3, true, &val) || (val > 1)) {
//				console->write("Invalid arguments, see help.\n");
//				return;
//			}
//
//			uint32_t drive = Mgmt_Zone_Ctrl_Get_Override_Drive(&this->zonectrl.zone);
//
//			if (val == 0) {
//				drive &= ~(1 << pwren);
//			} else {
//				drive |= (1 << pwren);
//			}
//
//			Mgmt_Zone_Ctrl_Set_Override_Drive(&this->zonectrl.zone, drive);
//
//		} else if (!option.compare("level")) {
//			uint32_t val = 0;
//			if (!parameters.parse_parameters(3, true, &val) || (val > 1)) {
//				console->write("Invalid arguments, see help.\n");
//				return;
//			}
//
//			uint32_t level = Mgmt_Zone_Ctrl_Get_Override_Level(&this->zonectrl.zone);
//
//			if (val == 0) {
//				level &= ~(1 << pwren);
//			} else {
//				level |= (1 << pwren);
//			}
//
//			Mgmt_Zone_Ctrl_Set_Override_Level(&this->zonectrl.zone, level);
//		} else {
//			console->write("Unknown option, see help.\n");
//			return;
//		}
	}

private:
	ZoneController &zonectrl;
};

void ZoneController::registerConsoleCommands(CommandParser &parser, const std::string &prefix) {
	parser.registerCommand(prefix + "override", std::make_shared<ZoneController::Override>(*this));
}

void ZoneController::deregisterConsoleCommands(CommandParser &parser, const std::string &prefix) {
	parser.registerCommand(prefix + "override", NULL);
}

#endif

