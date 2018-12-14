/*
 * MStateMachine.h
 *
 *  Created on: Nov 19, 2018
 *      Author: jtikalsky
 */

#ifndef SRC_COMMON_UW_IPMC_SERVICES_IPMI_MSTATEMACHINE_H_
#define SRC_COMMON_UW_IPMC_SERVICES_IPMI_MSTATEMACHINE_H_

#include <FreeRTOS.h>
#include <semphr.h>
#include <stdint.h>
#include <services/ipmi/sensor/HotswapSensor.h>
#include <libs/LogTree.h>
#include <memory>
#include <functional>

/**
 * A state machine handling IPMI M-State management and activation.
 */
class MStateMachine {
public:
	MStateMachine(std::shared_ptr<HotswapSensor> hotswap_sensor, LogTree &log);
	virtual ~MStateMachine();

	/// Activation lock controls ("Set FRU Activation Policy" Command)
	///@{
	virtual void set_activation_lock(bool activation_locked, bool deactivation_locked);
	virtual bool activation_locked();
	virtual bool deactivation_locked();
	///@}

	/// Handle state controls
	///@{
	enum HandleState {
		HANDLE_OPEN   = 0,
		HANDLE_CLOSED = 1,
		HANDLE_NULL   = 2,
	};
	virtual enum HandleState effective_handle_state();
	virtual enum HandleState physical_handle_state();
	virtual void physical_handle_state(enum HandleState state);
	virtual enum HandleState override_handle_state();
	virtual void override_handle_state(enum HandleState state);
	///@}

	/// Signals from the Payload Manager indicating activation/deactivation progress.
	///@{
	virtual void payload_activation_complete();
	virtual void payload_deactivation_complete();
	///@}

	/// Activation or deactivation command from shelf.
	///@{
	virtual void activate_fru() { this->reevaluate(ACTREQ_ACTIVATE_COMMANDED, HANDLE_NULL); };
	virtual void deactivate_fru() { this->reevaluate(ACTREQ_DEACTIVATE_COMMANDED, HANDLE_NULL); };
	///@}

	/// Retrieve current M-state
	virtual uint8_t mstate() { return this->_mstate; };

	virtual void register_console_commands(CommandParser &parser, const std::string &prefix);
	virtual void deregister_console_commands(CommandParser &parser, const std::string &prefix);

	std::function<void(void)> deactivate_payload; ///< A function used to deactivate the payload.

protected:
	SemaphoreHandle_t mutex; ///< Mutex protecting internal state.

	uint8_t _mstate; ///< The current M-state
	std::shared_ptr<HotswapSensor> hotswap_sensor; ///< The hotswap sensor to use.
	LogTree &log; ///< The LogTree for our messages.

	bool _activation_locked:1; ///< Activation Locked bit
	bool _deactivation_locked:1; ///< Deactivation Locked bit
	bool _startup_locked:1; ///< An internal activation lock during boot.

	enum HandleState _physical_handle_state:2; ///< The physical handle state.
	enum HandleState _override_handle_state:2; ///< The handle override state.

	enum ActivationRequest {
		ACTREQ_NONE,
		ACTREQ_ACTIVATE_COMMANDED,
		ACTREQ_DEACTIVATE_COMMANDED,
		ACTREQ
	};
	void transition(uint8_t mstate, enum HotswapSensor::StateTransitionReason reason);
	void reevaluate(enum ActivationRequest activation_request, enum HandleState previous_handle_state);

public:
	void reevaluate() { this->reevaluate(ACTREQ_NONE, HANDLE_NULL); };
};

#endif /* SRC_COMMON_UW_IPMC_SERVICES_IPMI_MSTATEMACHINE_H_ */
