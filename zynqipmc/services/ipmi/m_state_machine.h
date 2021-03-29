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

#ifndef SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_M_STATE_MACHINE_H_
#define SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_M_STATE_MACHINE_H_

#include <FreeRTOS.h>
#include <libs/logtree/logtree.h>
#include <semphr.h>
#include <services/ipmi/ipmi_led/ipmi_led.h>
#include <services/ipmi/sensor/hotswap_sensor.h>
#include <services/persistentstorage/persistent_storage.h>
#include <stdint.h>
#include <memory>
#include <functional>

/**
 * A state machine handling IPMI M-State management and activation.
 */
class MStateMachine final : public ConsoleCommandSupport {
public:
	MStateMachine(std::shared_ptr<HotswapSensor> hotswap_sensor, IPMILED &blue_led, LogTree &log, PersistentStorage *persistent_storage=NULL);
	~MStateMachine();

	/// Activation lock controls ("Set FRU Activation Policy" Command)
	///@{
	void setActivationLock(bool activation_locked, bool deactivation_locked);
	bool activationLocked();
	bool deactivationLocked();
	///@}

	/// Handle state controls
	///@{
	enum HandleState {
		HANDLE_OPEN   = 0,
		HANDLE_CLOSED = 1,
		HANDLE_NULL   = 2,
	};
	enum HandleState getEffectiveHandleState();
	enum HandleState getPhysicalHandleState();
	void setPhysicalHandleState(enum HandleState state);
	enum HandleState getOverrideHandleState();
	void setOverrideHandleState(enum HandleState state);

	inline bool hasPersistentStorage() { return !!this->persistent_storage; }
	enum HandleState getPersistentOverrideHandleState();
	void setPersistentOverrideHandleState(enum HandleState state);
	///@}

	/// Signals from the Payload Manager indicating activation/deactivation progress.
	///@{
	///
	/**
	 * This function is to be called by the payload manager when it perceives that
	 * all required startup negotiation is completed (Power & E-Keying), and the
	 * payload should be officially considered active.  It is idempotent.
	 */
	void payloadActivationComplete();

	/**
	 * This function is to be called by the payload manager when it perceives that
	 * all required shutdown negotiation is completed (Power & E-Keying), and the
	 * payload should be officially considered inactive.  It is idempotent.
	 */
	void payloadDeactivationComplete();
	///@}

	/// Activation or deactivation command from shelf.
	///@{
	inline void activateFru() { this->reevaluate(ACTREQ_ACTIVATE_COMMANDED, HANDLE_NULL); };
	inline void deactivateFru() { this->reevaluate(ACTREQ_DEACTIVATE_COMMANDED, HANDLE_NULL); };
	///@}

	/**
	 * Change the state of the fault lock flag.  When this flag is true, we will not
	 * go from M1 to M2.  This flag is to be automatically cleared when the handle
	 * is out.
	 *
	 * @param state The new fault lock flag state.
	 */
	void setFaultLock(bool state);

	//! Return the current fault lock state.
	inline bool getFaultLock() const { return this->fault_locked; };

	/**
	 * Change the state of the update lock flag.  When this flag is true, we will not
	 * go from M1 to M2.  This flag is to be cleared only by a restart, and can be
	 * set only while in M1.
	 *
	 * @return The current state of the flag (showing whether it was set successfully).
	 */
	bool setUpdateLock();

	//! Return the current update lock state.
	inline bool getUpdateLock() const { return this->update_locked; };

	/// Retrieve current M-state.
	inline uint8_t getMState() { return this->mstate; };

	virtual void registerConsoleCommands(CommandParser &parser, const std::string &prefix);
	virtual void deregisterConsoleCommands(CommandParser &parser, const std::string &prefix);

	std::function<void(void)> deactivate_payload; ///< A function used to deactivate the payload.

private:
	bool payload_deactivating; ///< A flag to suppress repeated executions of deactivate_payload()
	class HandleOverrideCommand;

	SemaphoreHandle_t mutex;	///< Mutex protecting internal state.

	uint8_t mstate;			///< The current M-state
	std::shared_ptr<HotswapSensor> hotswap_sensor; ///< The hotswap sensor to use.
	IPMILED &blue_led;			///< The IPMI LED used to display hotswap state.
	LogTree &log;				///< The LogTree for our messages.
	PersistentStorage *persistent_storage;

	bool activation_locked:1;	///< Activation Locked bit
	bool deactivation_locked:1;	///< Deactivation Locked bit
	bool startup_locked:1;		///< An internal activation lock during boot.
	bool fault_locked:1;		///< A fault has occured and we will not enter M2 until the handle is opened.
	bool update_locked:1;       ///< An update related operation has locked the card in M1 until reboot.

	enum HandleState physical_handle_state:2;	///< The physical handle state.
	enum HandleState override_handle_state:2;	///< The handle override state.

	enum ActivationRequest {
		ACTREQ_NONE,                //!< ACTREQ_NONE
		ACTREQ_ACTIVATE_COMMANDED,  //!< ACTREQ_ACTIVATE_COMMANDED
		ACTREQ_DEACTIVATE_COMMANDED,//!< ACTREQ_DEACTIVATE_COMMANDED
		ACTREQ                      //!< ACTREQ
	};

	void transition(uint8_t mstate, enum HotswapSensor::StateTransitionReason reason);
	void reevaluate(enum ActivationRequest activation_request, enum HandleState previous_handle_state);

	//! Update the IPMI Blue LED for the newly entered mstate.
	void updateIpmiLed(uint8_t prev_mstate, uint8_t mstate);

public:
	void reevaluate() { this->reevaluate(ACTREQ_NONE, HANDLE_NULL); };
};

#endif /* SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_M_STATE_MACHINE_H_ */
