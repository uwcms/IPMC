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

#include <libs/printf.h>
#include <libs/threading.h>
#include <services/console/consolesvc.h>
#include <services/ipmi/m_state_machine.h>
#include <services/ipmi/sensor/hotswap_sensor.h>

MStateMachine::MStateMachine(std::shared_ptr<HotswapSensor> hotswap_sensor, IPMILED &blue_led, LogTree &log)
	: deactivate_payload(nullptr), mstate(1), hotswap_sensor(hotswap_sensor), blue_led(blue_led), log(log),
	  activation_locked(false), deactivation_locked(false), startup_locked(true), fault_locked(false),
	  physical_handle_state(HANDLE_OPEN), override_handle_state(HANDLE_NULL) {
	this->mutex = xSemaphoreCreateRecursiveMutex();
	configASSERT(this->mutex);
	this->log.log("Initialized in M1", LogTree::LOG_INFO);
}

MStateMachine::~MStateMachine() {
	vSemaphoreDelete(this->mutex);
}

void MStateMachine::setActivationLock(bool activation_locked, bool deactivation_locked) {
	MutexGuard<true> lock(this->mutex, true);
	this->activation_locked = activation_locked;
	this->deactivation_locked = deactivation_locked;
	this->reevaluate();
}

bool MStateMachine::activationLocked() {
	return this->activation_locked;
}

bool MStateMachine::deactivationLocked() {
	return this->activation_locked;
}

enum MStateMachine::HandleState MStateMachine::getEffectiveHandleState() {
	MutexGuard<true> lock(this->mutex, true);
	if (this->override_handle_state != HANDLE_NULL) {
		return this->override_handle_state;
	}
	return this->physical_handle_state;
}

enum MStateMachine::HandleState MStateMachine::getPhysicalHandleState() {
	return this->physical_handle_state;
}

void MStateMachine::setPhysicalHandleState(enum HandleState state) {
	MutexGuard<true> lock(this->mutex, true);
	if (state != HANDLE_OPEN && state != HANDLE_CLOSED) {
		throw std::domain_error("The physical handle cannot be in an unknown state.");
	}

	enum HandleState old_state = this->getEffectiveHandleState();
	this->physical_handle_state = state;
	if (this->startup_locked) {
		this->startup_locked = false;
		this->hotswap_sensor->rearm();
	}

	if (this->getEffectiveHandleState() == HANDLE_OPEN) {
		this->fault_locked = false;
	}

	this->reevaluate(ACTREQ_NONE, old_state);
}

enum MStateMachine::HandleState MStateMachine::getOverrideHandleState() {
	MutexGuard<true> lock(this->mutex, true);
	return this->override_handle_state;
}

void MStateMachine::setOverrideHandleState(enum HandleState state) {
	MutexGuard<true> lock(this->mutex, true);
	enum HandleState old_state = this->getEffectiveHandleState();
	this->override_handle_state = state;
	if (this->getEffectiveHandleState() == HANDLE_OPEN) {
		this->fault_locked = false;
	}
	this->reevaluate(ACTREQ_NONE, old_state);
}

void MStateMachine::payloadActivationComplete() {
	MutexGuard<true> lock(this->mutex, true);
	if (this->mstate == 3) {
		this->transition(4, HotswapSensor::TRANS_NORMAL);
	}
}

void MStateMachine::payloadDeactivationComplete() {
	MutexGuard<true> lock(this->mutex, true);
	if (this->mstate == 6) {
		this->transition(1, HotswapSensor::TRANS_NORMAL);
	}
}

void MStateMachine::setFaultLock(bool state) {
	MutexGuard<true> lock(this->mutex, true);
	this->fault_locked = state;
	this->reevaluate();
}

void MStateMachine::reevaluate(enum ActivationRequest activation_request, enum HandleState previous_handle_state) {
	MutexGuard<true> lock(this->mutex, true);
	if (!this->hotswap_sensor) {
		return; // We're not actually ready to operate yet.
	}

	enum HandleState handle_state = this->getEffectiveHandleState();
	bool reevaluate_again = false;

	// PICMG 3.0 §3.2.4.2.2 ¶139
	if (previous_handle_state == HANDLE_OPEN && handle_state == HANDLE_CLOSED) {
		this->activation_locked = false; // Cleared on handle close.
	}

	// PICMG 3.0 §3.2.4.2.2 ¶140
	if (previous_handle_state == HANDLE_CLOSED && handle_state == HANDLE_OPEN) {
		this->deactivation_locked = false; // Cleared on handle open.
	}

	switch (this->mstate) {
		case 0:
			throw std::logic_error("We should never be in M0");
			break;
		case 1:
			if (handle_state == HANDLE_CLOSED && !this->activation_locked && !this->startup_locked && !this->fault_locked)
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
			if (handle_state == HANDLE_OPEN && !this->deactivation_locked)
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
			break;
	}

	if (reevaluate_again) {
		this->reevaluate();
	}
}

void MStateMachine::transition(uint8_t mstate, enum HotswapSensor::StateTransitionReason reason) {
	MutexGuard<true> lock(this->mutex, true);
	this->updateIpmiLed(this->mstate, mstate);
	this->mstate = mstate;
	this->hotswap_sensor->transition(mstate, reason);
	this->log.log(stdsprintf("Transitioned to M%hhu", mstate), LogTree::LOG_NOTICE);
}

void MStateMachine::updateIpmiLed(uint8_t prev_mstate, uint8_t mstate) {
	MutexGuard<true> lock(this->mutex, true);
	struct IPMILED::Action action;
	action.min_duration = 0;
	action.period_ms = 1000; // Ignored for non-blink, always accurate for blink.
	switch (mstate) {
		case 1:
			action.effect = IPMILED::ON; break;
		case 2:
			action.effect = IPMILED::BLINK;
			action.time_on_ms = 900;
			if (prev_mstate == 1)
				action.min_duration = 1000;
			break;
		case 3:
		case 4:
			action.effect = IPMILED::OFF; break;
		case 5:
		case 6:
			action.effect = IPMILED::BLINK;
			action.time_on_ms = 100;
			if (prev_mstate == 4)
				action.min_duration = 1000;
			break;
		case 7:
		default:
			return; // No change.
	}
	this->blue_led.submit(action);
}

/// Change the override handle status
class MStateMachine::HandleOverrideCommand : public CommandParser::Command {
public:
	HandleOverrideCommand(MStateMachine &mstatemachine) : mstatemachine(mstatemachine) { };

	virtual std::string getHelpText(const std::string &command) const {
		return command + " [in|out|release]\n\n"
				"Set the handle override state.\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		std::string arg;
		if (parameters.nargs() > 1) {
			if (!parameters.parseParameters(1, true, &arg)) {
				console->write("Invalid arguments, see help.\n");
				return;
			}
		}
		if (arg.size() == 0) {
			switch (this->mstatemachine.getOverrideHandleState()) {
				case MStateMachine::HANDLE_OPEN:
					console->write("Handle Electronically Open\n"); break;
				case MStateMachine::HANDLE_CLOSED:
					console->write("Handle Electronically Closed\n"); break;
				case MStateMachine::HANDLE_NULL:
					console->write("Handle Physically Controlled\n"); break;
				default:
					throw std::logic_error("The override handle state is neither Open, Closed, nor nullptr.");
			}

			switch (this->mstatemachine.getPhysicalHandleState()) {
				case MStateMachine::HANDLE_OPEN:
					console->write("Handle Physically Open\n"); break;
				case MStateMachine::HANDLE_CLOSED:
					console->write("Handle Physically Closed\n"); break;
				default:
					throw std::logic_error("The physical handle state is neither Open nor Closed.");
			}
		}
		else if (arg == "in") {
			this->mstatemachine.setOverrideHandleState(MStateMachine::HANDLE_CLOSED);
		}
		else if (arg == "out") {
			this->mstatemachine.setOverrideHandleState(MStateMachine::HANDLE_OPEN);
		}
		else if (arg == "release") {
			this->mstatemachine.setOverrideHandleState(MStateMachine::HANDLE_NULL);
		}
		else {
			console->write("Invalid arguments, see help.\n");
		}
	}

private:
	MStateMachine &mstatemachine;
};

void MStateMachine::registerConsoleCommands(CommandParser &parser, const std::string &prefix) {
	parser.registerCommand(prefix + "handle_override", std::make_shared<MStateMachine::HandleOverrideCommand>(*this));
}

void MStateMachine::deregisterConsoleCommands(CommandParser &parser, const std::string &prefix) {
	parser.registerCommand(prefix + "handle_override", nullptr);
}
