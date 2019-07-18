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

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_MGMT_ZONE_MGMTZONE_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_MGMT_ZONE_MGMTZONE_H_

// Only include driver if Management Zone IP is detected in the BSP.
#if XSDK_INDEXING || __has_include("mgmt_zone_ctrl.h")

#include "mgmt_zone_ctrl.h"
#include <vector>
#include <services/console/CommandParser.h>
#include <libs/ThreadingPrimitives.h>

/**
 * A single management zone controller driver for the ZYNQ-IPMC custom IP.
 *
 * The IP is used to control power pins and allows to build a virtual power tree
 * in software which is then used to react to external faults.
 *
 * Faults are generally wired from the SensorProcessor IP to this IP and the firmware
 * keeps track of faults in real-time. If a fault is detected the firmware will take care
 * of turning the corresponding power tree branch and its children off.
 *
 * The subclass Zone is what discriminates several power zones or branches.
 */
class ZoneController final : public ConsoleCommandSupport {
public:
	/**
	 * Initialize the Management Zone IP.
	 * @param device_id Hardware device ID, normally XPAR_MGMT_ZONE_CTRL_<>_DEVICE_ID. Check xparameters.h.
	 * @throw except::hardware_error if low level driver fails to configure.
	 */
	ZoneController(uint16_t device_id);
	~ZoneController();

	//! Get the number of power enable outputs available in this zone controller.
	uint32_t getPowerEnableCount() const { return this->zone.pwren_cnt; };

	//! Get the number of hardfault inputs available on this zone controller.
	uint32_t getHardfaultCount() const { return this->zone.hf_cnt; };

	//! Zone subclass to discriminate each zone individually.
	class Zone {
	public:
		/**
		 * Initialize a specific zone.
		 * @param zone_ctrl Associated zone controller.
		 * @param zone_number The number of the zone in firmware.
		 * @throw std::out_of_range is zone number is out of range.
		 */
		Zone(ZoneController &zone_ctrl, size_t zone_number);
		~Zone() {};

		/**
		 * Set the hardfault mask for this zone.
		 * @param mask A bitmask of hardfault inputs relevant to this zone only.
		 * @param holdoff A delay after startup during which hardfaults are ignored in clock ticks.
		 */
		void setHardfaultMask(uint64_t mask, uint32_t holdoff);

		/**
		 * Get the hardfault mask for this zone.
		 * @param mask A bitmask of hardfault inputs relevant to this zone. Can be set to nullptr.
		 * @param holdoff A delay after startup during which hardfaults are ignored. Can be set to nullptr.
		 */
		void getHardfaultMask(uint64_t *mask, uint32_t *holdoff);

		/**
		 * Get current hardfault status.
		 * @param apply_mask If true, mask out hardfaults not relevant to this zone.
		 * @return Hardfault mask.
		 */
		uint64_t getHardfaultStatus(bool apply_mask = true);

		//! Management zone power enable pin configuration.
		class OutputConfig {
		public:
			bool active_high   : 1;			///< True if the PEN output is active high.
			bool drive_enabled : 1;			///< True if the PEN output is controlled by this zone.
			uint16_t enable_delay   : 16;	///< The delay in MS from startup before enabling this PEN (shutdown in reverse order).
			OutputConfig(bool active_high = true, bool drive_enabled = false, uint16_t enable_delay = 0)
				: active_high(active_high), drive_enabled(drive_enabled), enable_delay(enable_delay) { };
		};

		/**
		 * Set Power Enable configuration.
		 * @param pen_config An array of the correct number of OutputConfig entries.
		 */
		void setPowerEnableConfig(const std::vector<OutputConfig> &pen_config);

		/**
		 * Get Power Enable configuration.
		 * @param[out] pen_config An array of OutputConfig entries.
		 */
		void getPowerEnableConfig(std::vector<OutputConfig> &pen_config);

		/**
		 * Get current power enable status.
		 * @param apply_mask If true, mask out power enables not relevant to this zone.
		 * @return Power enable status.
		 */
		uint32_t getPowerEnableStatus(bool apply_mask = true);

		//! Management zone power state targets.
		typedef enum {
			ON,   ///< Transition to power on.
			OFF,  ///< Transition to power off.
			KILL, ///< Immediately cut off power.
		} PowerAction;

		/**
		 * Set the power state for this zone.
		 * @param action The new state to transition to.
		 */
		void setPowerState(PowerAction action);

		/**
		 * Get the power state for this zone.
		 * @param in_transition Optional output indicating whether the power is in transition to the returned state.
		 * @return The current desired (if in transition) or active (if stable) power state.
		 */
		bool getPowerState(bool *in_transition = nullptr);

		/**
		 * Retrieve the desired power state (whether the zone is enabled or enabling
		 * according to the last state specifically set).
		 *
		 * @note This value is not affected by faults, either hard OR soft faults.
		 *       This means that set_power_state(KILL) will not change the desired
		 *       state, and OFF must also be subsequently set to acknowledge it.
		 *
		 * @return true if on, else false
		 */
		bool getDesiredPowerState() { return this->desired_power_state; };

		/**
		 * Retrieve the tick64 timestamp that the desired power state was last changed.
		 *
		 * @note This value is not affected by faults, either hard OR soft faults.
		 *       This means that set_power_state(KILL) will not update this timestamp
		 *       and OFF must also be subsequently used if this is desired.
		 *
		 * @return tick64 timestamp of last change.
		 */
		inline uint64_t getLastTransitionStart() { return this->last_transition_start_ts; };
		inline void resetLastTransitionStart() { this->last_transition_start_ts = get_tick64(); };

		//! Get the management zone name, if set.
		inline void setName(const std::string &name) { this->name = name; };

		//! Set the management zone name, not required but helpful.
		inline const std::string& getName() const { return this->name; };

	private:
		ZoneController &zonectrl;	///< The controller associated to this zone.
		const size_t kZoneNumber;	///< Zone number.
		std::string name;			///< Name of this zone.
		bool desired_power_state;	///< Desired power state, ON or OFF.
		uint64_t last_transition_start_ts; ///< The timestamp of the start of the last transition.
	};

	// From base class ConsoleCommandSupport:
	virtual void registerConsoleCommands(CommandParser &parser, const std::string &prefix="");
	virtual void deregisterConsoleCommands(CommandParser &parser, const std::string &prefix="");

private:
	// Supported console commands:
	class Override;	///< Command to override power enable pins.

	Mgmt_Zone_Ctrl zone;	///< Controller low-level driver data.
};

#endif

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_MGMT_ZONE_MGMTZONE_H_ */
