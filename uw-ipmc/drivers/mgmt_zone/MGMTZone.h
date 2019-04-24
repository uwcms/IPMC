/*
 * MGMTZone.h
 *
 *  Created on: Mar 20, 2018
 *      Author: jtikalsky
 */

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_MGMT_ZONE_MGMTZONE_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_MGMT_ZONE_MGMTZONE_H_

#if XSDK_INDEXING || __has_include(<mgmt_zone_ctrl.h>)
#define MGMTZONE_DRIVER_INCLUDED

#include <xil_types.h>
#include <vector>
#include <mgmt_zone_ctrl.h>
#include <libs/ThreadingPrimitives.h>

/**
 * A single management zone.
 */
class MGMT_Zone {
public:
	const u16 DeviceId; ///< The DeviceId of the controller hosting this zone.
	const u32 MZNo; ///< The MZ number within the MZ Controller
	MGMT_Zone(u16 DeviceId, u32 MZNo);
	virtual ~MGMT_Zone();

	virtual void set_hardfault_mask(u64 mask, u32 holdoff);
	virtual void get_hardfault_mask(u64 *mask, u32 *holdoff);
	virtual u64 get_hardfault_status(bool apply_mask=true);

	/// Management Zone Power Enable Pin Configuration
	class OutputConfig {
	public:
		bool active_high   : 1;  ///< True if the PEN output is active high.
		bool drive_enabled : 1;  ///< True if the PEN output is controlled by this MZ.
		u16 enable_delay   : 16; ///< The delay in MS from startup before enabling this PEN (shutdown in reverse order).
		/// Instantiate
		OutputConfig(bool active_high=true, bool drive_enabled=false, u16 enable_delay=0)
			: active_high(active_high), drive_enabled(drive_enabled), enable_delay(enable_delay) { };
	};

	virtual void set_pen_config(const std::vector<OutputConfig> &pen_config);
	virtual void get_pen_config(std::vector<OutputConfig> &pen_config);
	virtual u32 get_pen_status(bool apply_mask=true);

	/// Management Zone Power State Targets
	typedef enum {
		ON,   ///< Transition to power on.
		OFF,  ///< Transition to power off.
		KILL, ///< Immediately cut off power.
	} PowerAction;
	virtual void set_power_state(PowerAction action);
	virtual bool get_power_state(bool *in_transition=NULL);

	/**
	 * Retrieve the desired power state (whether the zone is enabled or enabling
	 * according to the last state specifically set).
	 *
	 * \note This value is not affected by faults, either hard OR soft faults.
	 *       This means that set_power_state(KILL) will not change the desired
	 *       state, and OFF must also be subsequently set to acknowledge it.
	 *
	 * @return true if on, else false
	 */
	virtual bool get_desired_power_state() { return this->_desired_power_state; };

	/**
	 * Retrieve the tick64 timestamp that the desired power state was last changed.
	 *
	 * \note This value is not affected by faults, either hard OR soft faults.
	 *       This means that set_power_state(KILL) will not update this timestamp
	 *       and OFF must also be subsequently used if this is desired.
	 *
	 * @return tick64 timestamp of last change
	 */
	virtual uint64_t last_transition_start_ts() { return this->_last_transition_start_ts; };
	virtual void reset_last_transition_start_ts() { this->_last_transition_start_ts = get_tick64(); };

	/// Get the number of power enables on this MZ controller.
	virtual u32 get_pen_count() const { return this->zone.pwren_cnt; };
	/// Get the number of hardfault inputs on this MZ controller.
	virtual u32 get_hardfault_count() const { return this->zone.hf_cnt; };

protected:
	Mgmt_Zone_Ctrl zone; ///< The Mgmt_Zone_Ctrl instance.
	bool _desired_power_state;
	uint64_t _last_transition_start_ts; ///< The timestamp of the start of the last transition.
};

#endif

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_MGMT_ZONE_MGMTZONE_H_ */
