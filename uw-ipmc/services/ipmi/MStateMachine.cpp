/*
 * MStateMachine.cpp
 *
 *  Created on: Nov 19, 2018
 *      Author: jtikalsky
 */

#include <services/ipmi/MStateMachine.h>
#include <services/ipmi/sensor/HotswapSensor.h>
#include <services/console/ConsoleSvc.h>
#include <libs/ThreadingPrimitives.h>
#include <libs/printf.h>

MStateMachine::MStateMachine(std::shared_ptr<HotswapSensor> hotswap_sensor, IPMI_LED &blue_led, LogTree &log)
	: deactivate_payload(NULL), _mstate(1), hotswap_sensor(hotswap_sensor), blue_led(blue_led), log(log),
	  _activation_locked(false), _deactivation_locked(false), _startup_locked(true), _fault_locked(false),
	  _physical_handle_state(HANDLE_OPEN), _override_handle_state(HANDLE_NULL) {
	this->mutex = xSemaphoreCreateRecursiveMutex();
	configASSERT(this->mutex);
	this->log.log("Initialized in M1", LogTree::LOG_INFO);
}

MStateMachine::~MStateMachine() {
	vSemaphoreDelete(this->mutex);
}

void MStateMachine::set_activation_lock(bool activation_locked, bool deactivation_locked) {
	MutexGuard<true> lock(this->mutex, true);
	this->_activation_locked = activation_locked;
	this->_deactivation_locked = deactivation_locked;
	this->reevaluate();
}

bool MStateMachine::activation_locked() {
	return this->_activation_locked;
}

bool MStateMachine::deactivation_locked() {
	return this->_activation_locked;
}

enum MStateMachine::HandleState MStateMachine::effective_handle_state() {
	MutexGuard<true> lock(this->mutex, true);
	if (this->_override_handle_state != HANDLE_NULL)
		return this->_override_handle_state;
	return this->_physical_handle_state;
}

enum MStateMachine::HandleState MStateMachine::physical_handle_state() {
	return this->_physical_handle_state;
}

void MStateMachine::physical_handle_state(enum HandleState state) {
	MutexGuard<true> lock(this->mutex, true);
	if (state != HANDLE_OPEN && state != HANDLE_CLOSED)
		throw std::domain_error("The physical handle cannot be in an unknown state.");

	if (this->_physical_handle_state != state && state == HANDLE_OPEN) {     // XXX
		void payload_manager_apd_bringup_poweroff_hack();                    // XXX
		payload_manager_apd_bringup_poweroff_hack();                         // XXX
	}                                                                        // XXX
	enum HandleState old_state = this->effective_handle_state();
	this->_physical_handle_state = state;
	if (this->_startup_locked) {
		this->_startup_locked = false;
		this->hotswap_sensor->rearm();
	}
	if (this->effective_handle_state() == HANDLE_OPEN)
		this->_fault_locked = false;
	this->reevaluate(ACTREQ_NONE, old_state);
}

enum MStateMachine::HandleState MStateMachine::override_handle_state() {
	MutexGuard<true> lock(this->mutex, true);
	return this->_override_handle_state;
}

void MStateMachine::override_handle_state(enum HandleState state) {
	MutexGuard<true> lock(this->mutex, true);
	enum HandleState old_state = this->effective_handle_state();
	this->_override_handle_state = state;
	if (this->effective_handle_state() == HANDLE_OPEN)
		this->_fault_locked = false;
	this->reevaluate(ACTREQ_NONE, old_state);
}

/**
 * This function is to be called by the payload manager when it perceives that
 * all required startup negotiation is completed (Power & E-Keying), and the
 * payload should be officially considered active.  It is idempotent.
 */
void MStateMachine::payload_activation_complete() {
	MutexGuard<true> lock(this->mutex, true);
	if (this->_mstate == 3)
		this->transition(4, HotswapSensor::TRANS_NORMAL);
}

/**
 * This function is to be called by the payload manager when it perceives that
 * all required shutdown negotiation is completed (Power & E-Keying), and the
 * payload should be officially considered inactive.  It is idempotent.
 */
void MStateMachine::payload_deactivation_complete() {
	MutexGuard<true> lock(this->mutex, true);
	if (this->_mstate == 6)
		this->transition(1, HotswapSensor::TRANS_NORMAL);
}

/**
 * Change the state of the fault lock flag.  When this flag is true, we will not
 * go from M1 to M2.  This flag is to be automatically cleared when the handle
 * is out.
 *
 * @param state The new fault lock flag state
 */
void MStateMachine::fault_lock(bool state) {
	MutexGuard<true> lock(this->mutex, true);
	this->_fault_locked = state;
	this->reevaluate();
}

void MStateMachine::reevaluate(enum ActivationRequest activation_request, enum HandleState previous_handle_state) {
	MutexGuard<true> lock(this->mutex, true);
	if (!this->hotswap_sensor)
		return; // We're not actually ready to operate yet.

	enum HandleState handle_state = this->effective_handle_state();
	bool reevaluate_again = false;

	// PICMG 3.0 §3.2.4.2.2 ¶139
	if (previous_handle_state == HANDLE_OPEN && handle_state == HANDLE_CLOSED)
		this->_activation_locked = false; // Cleared on handle close.
	// PICMG 3.0 §3.2.4.2.2 ¶140
	if (previous_handle_state == HANDLE_CLOSED && handle_state == HANDLE_OPEN)
		this->_deactivation_locked = false; // Cleared on handle open.

	switch (this->_mstate) {
	case 0:
		throw std::logic_error("We should never be in M0");
		break;
	case 1:
		if (handle_state == HANDLE_CLOSED && !this->_activation_locked && !this->_startup_locked && !this->_fault_locked)
			this->transition(2, (previous_handle_state != handle_state ? HotswapSensor::TRANS_OPERATOR_SWITCH : HotswapSensor::TRANS_NORMAL));
		break;
	case 2:
		if (handle_state == HANDLE_OPEN)
			this->transition(1, (previous_handle_state != handle_state ? HotswapSensor::TRANS_OPERATOR_SWITCH : HotswapSensor::TRANS_NORMAL));
		else if (activation_request == ACTREQ_ACTIVATE_COMMANDED) {
			this->transition(3, HotswapSensor::TRANS_COMMANDED_BY_SHELF);
			reevaluate_again = true;
		}
		break;
	case 3:
		if (handle_state == HANDLE_OPEN) {
			enum HotswapSensor::StateTransitionReason reason;
			if (previous_handle_state != handle_state)
				reason = HotswapSensor::TRANS_OPERATOR_SWITCH;
			else if (activation_request == ACTREQ_DEACTIVATE_COMMANDED)
				/* This reason isn't legal according to PICMG 3.0 Table 3-23 but
				 * is necessary to be honest in the case of Set FRU Activation
				 * in M3 which is shown as possible in PICMG 3.0 Figure 3-5.
				 */
				reason = HotswapSensor::TRANS_COMMANDED_BY_SHELF;
			else
				reason = HotswapSensor::TRANS_FRU_PROGRAMATIC;
			this->transition(6, reason);
			reevaluate_again = true;
			break;
		}
		// We must now wait for the (Shelf-driven) negotiation of power and E-Keying.
		// this->transition(4, HotswapSensor::TRANS_NORMAL);
		break;
	case 4:
		if (handle_state == HANDLE_OPEN && !this->_deactivation_locked)
			this->transition(5, (previous_handle_state != handle_state ? HotswapSensor::TRANS_OPERATOR_SWITCH : HotswapSensor::TRANS_NORMAL));
		else if (activation_request == ACTREQ_DEACTIVATE_COMMANDED) {
			this->transition(6, HotswapSensor::TRANS_COMMANDED_BY_SHELF);
			reevaluate_again = true;
		}
		break;
	case 5:
		if (activation_request == ACTREQ_ACTIVATE_COMMANDED)
			this->transition(4, HotswapSensor::TRANS_COMMANDED_BY_SHELF);
		else if (activation_request == ACTREQ_DEACTIVATE_COMMANDED) {
			this->transition(6, HotswapSensor::TRANS_COMMANDED_BY_SHELF);
			reevaluate_again = true;
		}
		break;
	case 6:
		// Deactivation of backend power & discard of E-Keyed interfaces is managed by us.
		if (this->deactivate_payload)
			this->deactivate_payload();
		/* The transition to M1 will be triggered by the call to
		 * payload_deactivation_complete() by the payload manager.
		 */
		//this->transition(1, HotswapSensor::TRANS_NORMAL);
		break;
	case 7:
		throw std::logic_error("We can't exactly be claiming to be M7 on our own.");
		break;
	default:
		throw std::logic_error("We are not in any state [M0, M7]");
	}
	if (reevaluate_again)
		this->reevaluate();
}

void MStateMachine::transition(uint8_t mstate, enum HotswapSensor::StateTransitionReason reason) {
	MutexGuard<true> lock(this->mutex, true);
	this->update_ipmi_led(this->_mstate, mstate);
	this->_mstate = mstate;
	this->hotswap_sensor->transition(mstate, reason);
	this->log.log(stdsprintf("Transitioned to M%hhu", mstate), LogTree::LOG_NOTICE);
}

/**
 * Update the IPMI Blue LED for the newly entered mstate.
 */
void MStateMachine::update_ipmi_led(uint8_t prev_mstate, uint8_t mstate) {
	MutexGuard<true> lock(this->mutex, true);
	struct IPMI_LED::Action action;
	action.min_duration = 0;
	action.periodMs = 1000; // Ignored for non-blink, always accurate for blink.
	switch (mstate) {
	case 1:
		action.effect = IPMI_LED::ON; break;
	case 2:
		action.effect = IPMI_LED::BLINK;
		action.timeOnMs = 900;
		if (prev_mstate == 1)
			action.min_duration = 1000;
		break;
	case 3:
	case 4:
		action.effect = IPMI_LED::OFF; break;
	case 5:
	case 6:
		action.effect = IPMI_LED::BLINK;
		action.timeOnMs = 100;
		if (prev_mstate == 4)
			action.min_duration = 1000;
		break;
	case 7:
	default:
		return; // No change.
	}
	this->blue_led.submit(action);
}

namespace {

/// Change the override handle status
class ConsoleCommand_handle_override : public CommandParser::Command {
public:
	MStateMachine &mstatemachine;
	ConsoleCommand_handle_override(MStateMachine &mstatemachine) : mstatemachine(mstatemachine) { };

	virtual std::string get_helptext(const std::string &command) const {
		return stdsprintf(
				"%s [in|out|release]\n\n"
				"Set the handle override state.\n", command.c_str());
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		std::string arg;
		if (parameters.nargs() > 1) {
			if (!parameters.parse_parameters(1, true, &arg)) {
				console->write("Invalid arguments, see help.\n");
				return;
			}
		}
		if (arg.size() == 0) {
			switch (this->mstatemachine.override_handle_state()) {
			case MStateMachine::HANDLE_OPEN:
				console->write("Handle Electronically Open\n"); break;
			case MStateMachine::HANDLE_CLOSED:
				console->write("Handle Electronically Closed\n"); break;
			case MStateMachine::HANDLE_NULL:
				console->write("Handle Physically Controlled\n"); break;
			default:
				throw std::logic_error("The override handle state is neither Open, Closed, nor NULL.");
			}
			switch (this->mstatemachine.physical_handle_state()) {
			case MStateMachine::HANDLE_OPEN:
				console->write("Handle Physically Open\n"); break;
			case MStateMachine::HANDLE_CLOSED:
				console->write("Handle Physically Closed\n"); break;
			default:
				throw std::logic_error("The physical handle state is neither Open nor Closed.");
			}
		}
		else if (arg == "in") {
			this->mstatemachine.override_handle_state(MStateMachine::HANDLE_CLOSED);
		}
		else if (arg == "out") {
			this->mstatemachine.override_handle_state(MStateMachine::HANDLE_OPEN);
		}
		else if (arg == "release") {
			this->mstatemachine.override_handle_state(MStateMachine::HANDLE_NULL);
		}
		else {
			console->write("Invalid arguments, see help.\n");
		}
	}
};

}

/**
 * Register console commands related to the MStateMachine.
 * @param parser The command parser to register to.
 * @param prefix A prefix for the registered commands.
 */
void MStateMachine::register_console_commands(CommandParser &parser, const std::string &prefix) {
	parser.register_command(prefix + "handle_override", std::make_shared<ConsoleCommand_handle_override>(*this));
}

/**
 * Unregister console commands related to the MStateMachine.
 * @param parser The command parser to unregister from.
 * @param prefix A prefix for the registered commands.
 */
void MStateMachine::deregister_console_commands(CommandParser &parser, const std::string &prefix) {
	parser.register_command(prefix + "handle_override", NULL);
}
