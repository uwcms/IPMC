/*
 * MGMTZone.cpp
 *
 *  Created on: Mar 20, 2018
 *      Author: jtikalsky
 */

#include "MGMTZone.h"

#if __has_include(<mgmt_zone_ctrl.h>)

#include "FreeRTOS.h"
#include <libs/printf.h>
#include <libs/except.h>

/**
 * Instantiate a MZ.
 *
 * @param DeviceId The controller hosting this zone.
 * @param MZNo The number of the MZ within its controller.
 */
MGMT_Zone::MGMT_Zone(u16 DeviceId, u32 MZNo)
	: DeviceId(DeviceId), MZNo(MZNo) {
	if (XST_SUCCESS != Mgmt_Zone_Ctrl_Initialize(&this->zone, DeviceId))
		throw except::hardware_error(stdsprintf("Unable to initialize MGMT_Zone(%hu, %lu)", DeviceId, MZNo));
}

MGMT_Zone::~MGMT_Zone() {
}

/**
 * Set the hardfault mask for this MZ
 *
 * @param mask A bitmask of hardfault inputs relevant to this MZ.
 * @param holdoff A delay after startup during which hardfaults are ignored.
 */
void MGMT_Zone::set_hardfault_mask(u64 mask, u32 holdoff) {
	MZ_config config;
	Mgmt_Zone_Ctrl_Get_MZ_Cfg(&this->zone, this->MZNo, &config);
	config.hardfault_mask = mask;
	config.fault_holdoff = holdoff;
	Mgmt_Zone_Ctrl_Set_MZ_Cfg(&this->zone, this->MZNo, &config);
}

/**
 * Get the hardfault mask for this MZ
 *
 * @param[out] mask A bitmask of hardfault inputs relevant to this MZ. (u64* or NULL)
 * @param[out] holdoff A delay after startup during which hardfaults are ignored. (u32* or NULL)
 */
void MGMT_Zone::get_hardfault_mask(u64 *mask, u32 *holdoff) {
	MZ_config config;
	Mgmt_Zone_Ctrl_Get_MZ_Cfg(&this->zone, this->MZNo, &config);
	if (mask)
		*mask = config.hardfault_mask;
	if (holdoff)
		*holdoff = config.fault_holdoff;
}

/**
 * Get current hardfault status.
 *
 * @param apply_mask If true, mask out hardfaults not relevant to this MZ.
 * @return
 */
u64 MGMT_Zone::get_hardfault_status(bool apply_mask) {
	u64 mask = UINT64_MAX;
	if (apply_mask)
		this->get_hardfault_mask(&mask, NULL);
	return mask & Mgmt_Zone_Ctrl_Get_Hard_Fault_Status(&this->zone);
}

/**
 * Set Power Enable configuration
 *
 * @param pen_config An array of the correct number of OutputConfig entries.
 */
void MGMT_Zone::set_pen_config(const std::vector<OutputConfig> &pen_config) {
	if (pen_config.size() != this->get_pen_count())
		throw std::logic_error(stdsprintf("Supplied PEN config vector specifies an incorrect number of PENs (%u/%lu) for MZ %lu", pen_config.size(), this->get_pen_count(), this->MZNo));
	MZ_config config;
	Mgmt_Zone_Ctrl_Get_MZ_Cfg(&this->zone, this->MZNo, &config);
	for (uint32_t i = 0; i < this->get_pen_count(); ++i) {
		config.pwren_cfg[i] =
				((pen_config[i].drive_enabled)<<17) |
				((pen_config[i].active_high  )<<16) |
				((pen_config[i].enable_delay )<< 0) ;
	}
	Mgmt_Zone_Ctrl_Set_MZ_Cfg(&this->zone, this->MZNo, &config);
}

/**
 * Get Power Enable configuration
 *
 * @param[out] pen_config An array of OutputConfig entries.
 */
void MGMT_Zone::get_pen_config(std::vector<OutputConfig> &pen_config) {
	pen_config.clear();
	MZ_config config;
	Mgmt_Zone_Ctrl_Get_MZ_Cfg(&this->zone, this->MZNo, &config);
	for (uint32_t i = 0; i < this->get_pen_count(); ++i)
		pen_config.emplace_back(config.pwren_cfg[i] & (1<<16), config.pwren_cfg[i] & (1<<17), config.pwren_cfg[i]&0xffff);
}

/**
 * Get current power enable status.
 *
 * @param apply_mask If true, mask out power enables not relevant to this MZ.
 * @return
 */
u32 MGMT_Zone::get_pen_status(bool apply_mask) {
	u32 mask = UINT32_MAX;
	if (apply_mask) {
		std::vector<OutputConfig> pencfg;
		this->get_pen_config(pencfg);
		mask = 0;
		for (uint32_t i = 0; i < this->get_pen_count(); ++i)
			if (pencfg[i].drive_enabled)
				mask |= (1 << i);
	}
	return mask & Mgmt_Zone_Ctrl_Get_Pwr_En_Status(&this->zone);
}

/**
 * Set the power state for this Zone
 *
 * @param action The new state to transition to.
 */
void MGMT_Zone::set_power_state(PowerAction action) {
	switch (action) {
	case ON:   Mgmt_Zone_Ctrl_Pwr_ON_Seq(&this->zone, this->MZNo); break;
	case OFF:  Mgmt_Zone_Ctrl_Pwr_OFF_Seq(&this->zone, this->MZNo); break;
	case KILL: Mgmt_Zone_Ctrl_Dispatch_Soft_Fault(&this->zone, this->MZNo); break;
	default:   throw std::domain_error(stdsprintf("Invalid PowerAction %u supplied to set_power_state() for MZ %lu", static_cast<unsigned int>(action), this->MZNo));
	}
}

/**
 * Get the power state for this Zone
 *
 * @param[out] in_transition Optional output indicating whether the power is in transition to the returned state.
 * @return The current desired (if in transition) or active (if stable) power state.
 */
bool MGMT_Zone::get_power_state(bool *in_transition) {
	bool active;
	bool transitioning;

	switch (Mgmt_Zone_Ctrl_Get_MZ_Status(&this->zone, this->MZNo)) {
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

#endif

