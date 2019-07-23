/*
 * IPMILED.cpp
 *
 *  Created on: Jan 16, 2019
 *      Author: jtikalsky
 */

#include <libs/threading.h>
#include <services/ipmi/ipmi_led/IPMILED.h>
#include <IPMC.h>

IPMI_LED::IPMI_LED(LED &led) : led(led), local_min_duration(0ULL), lamp_test_timeout(0ULL), timer(NULL) {
	this->local_action.effect = OFF;
	this->local_action.min_duration = 0;
	this->override_action.effect = INACTIVE;
	this->mutex = xSemaphoreCreateRecursiveMutex();
}

IPMI_LED::~IPMI_LED() {
	MutexGuard<true> lock(this->mutex, true);
	if (this->timer)
		this->timer->cancel();
	lock.release();
	vSemaphoreDelete(this->mutex);
}

void IPMI_LED::submit(struct Action action) {
	MutexGuard<true> lock(this->mutex, true);
	this->future_actions.push_back(action);
	this->update_current_action();
}

void IPMI_LED::override(struct Action action) {
	MutexGuard<true> lock(this->mutex, true);
	action.min_duration = UINT64_MAX; // This is only used to return the value in get_current_physical_action()
	this->override_action = action;
	this->apply_physical_action();
}

void IPMI_LED::lamp_test(uint64_t duration) {
	MutexGuard<true> lock(this->mutex, true);
	this->lamp_test_timeout.setTimeout(duration);
	this->apply_physical_action();
}

struct IPMI_LED::Action IPMI_LED::get_current_local_action() {
	MutexGuard<true> lock(this->mutex, true);
	return this->local_action;
}
struct IPMI_LED::Action IPMI_LED::get_current_override_action() {
	MutexGuard<true> lock(this->mutex, true);
	return this->override_action;

}
AbsoluteTimeout IPMI_LED::get_current_lamp_test_duration() {
	MutexGuard<true> lock(this->mutex, true);
	return this->lamp_test_timeout;
}

struct IPMI_LED::Action IPMI_LED::get_current_physical_action() {
	MutexGuard<true> lock(this->mutex, true);

	struct Action action;
	if (this->lamp_test_timeout.getTimeout()) {
		action.effect = ON;
		action.min_duration = this->lamp_test_timeout.getTimeout();
		action.control_level = LAMPTEST;
	}
	else if (this->override_action.effect != INACTIVE) {
		action = this->override_action;
		action.control_level = OVERRIDE;
	}
	else {
		action = this->local_action;
		action.min_duration = this->local_min_duration.getTimeout();
		action.control_level = LOCAL;
	}
	return action;
}

void IPMI_LED::reset_local() {
	MutexGuard<true> lock(this->mutex, true);
	while(this->future_actions.size())
		this->future_actions.pop_front();
	this->local_min_duration.setTimeout(0ULL);
	if (this->timer)
		this->timer->cancel();
	this->timer = NULL;
}

void IPMI_LED::update_current_action() {
	MutexGuard<true> lock(this->mutex, true);
	while (!this->local_min_duration.getTimeout() && this->future_actions.size()) {
		this->local_action = this->future_actions.front();
		this->future_actions.pop_front();
		this->local_min_duration.setTimeout(this->local_action.min_duration);
		this->apply_physical_action();
	}
	if (this->timer) {
		this->timer->cancel();
		this->timer = NULL;
	}
	if (this->local_min_duration.getTimeout()) {
		this->timer = std::make_shared<TimerService::Timer>([this]()->void{ this->update_current_action(); }, this->local_min_duration);
		TimerService::global_timer(TASK_PRIORITY_SERVICE).submit(this->timer);
	}
}

void IPMI_LED::apply_physical_action() {
	MutexGuard<true> lock(this->mutex, true);
	struct Action action = this->get_current_physical_action();

	switch (action.effect) {
	case OFF:   this->led.off(); break;
	case ON:    this->led.on(); break;
	case DIM:   this->led.dim(action.intensity); break;
	case BLINK: this->led.blink(action.periodMs, action.timeOnMs); break;
	case PULSE: this->led.pulse(action.periodMs); break;
	default:
		// Uh. Guess we'll just pass on this one.
		break;
	}
}
