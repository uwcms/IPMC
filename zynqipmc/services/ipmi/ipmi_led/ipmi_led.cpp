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

#include <libs/threading.h>
#include <services/ipmi/ipmi_led/ipmi_led.h>

#include <core.h>

IPMILED::IPMILED(LED &led) : led(led), local_min_duration(0ULL), lampTest_timeout(0ULL), timer(nullptr) {
	this->local_action.effect = OFF;
	this->local_action.min_duration = 0;
	this->override_action.effect = INACTIVE;
	this->mutex = xSemaphoreCreateRecursiveMutex();
}

IPMILED::~IPMILED() {
	MutexGuard<true> lock(this->mutex, true);
	if (this->timer)
		this->timer->cancel();
	lock.release();
	vSemaphoreDelete(this->mutex);
}

void IPMILED::submit(struct Action action) {
	MutexGuard<true> lock(this->mutex, true);
	this->future_actions.push_back(action);
	this->updateCurrentAction();
}

void IPMILED::override(struct Action action) {
	MutexGuard<true> lock(this->mutex, true);
	action.min_duration = UINT64_MAX; // This is only used to return the value in getCurrentPhysicalAction()
	this->override_action = action;
	this->applyPhysicalAction();
}

void IPMILED::lampTest(uint64_t duration) {
	MutexGuard<true> lock(this->mutex, true);
	this->lampTest_timeout.setTimeout(duration);
	this->applyPhysicalAction();
}

struct IPMILED::Action IPMILED::getCurrentLocalAction() {
	MutexGuard<true> lock(this->mutex, true);
	return this->local_action;
}
struct IPMILED::Action IPMILED::getCurrentOverrideAction() {
	MutexGuard<true> lock(this->mutex, true);
	return this->override_action;

}
AbsoluteTimeout IPMILED::getCurrentLampTestDuration() {
	MutexGuard<true> lock(this->mutex, true);
	return this->lampTest_timeout;
}

struct IPMILED::Action IPMILED::getCurrentPhysicalAction() {
	MutexGuard<true> lock(this->mutex, true);

	struct Action action;
	if (this->lampTest_timeout.getTimeout()) {
		action.effect = ON;
		action.min_duration = this->lampTest_timeout.getTimeout();
		action.control_level = LAMPTEST;
	} else if (this->override_action.effect != INACTIVE) {
		action = this->override_action;
		action.control_level = OVERRIDE;
	} else {
		action = this->local_action;
		action.min_duration = this->local_min_duration.getTimeout();
		action.control_level = LOCAL;
	}
	return action;
}

void IPMILED::resetLocal() {
	MutexGuard<true> lock(this->mutex, true);

	while(this->future_actions.size())
		this->future_actions.pop_front();

	this->local_min_duration.setTimeout(0ULL);

	if (this->timer)
		this->timer->cancel();

	this->timer = nullptr;
}

void IPMILED::updateCurrentAction() {
	MutexGuard<true> lock(this->mutex, true);

	while (!this->local_min_duration.getTimeout() && this->future_actions.size()) {
		this->local_action = this->future_actions.front();
		this->future_actions.pop_front();
		this->local_min_duration.setTimeout(this->local_action.min_duration);
		this->applyPhysicalAction();
	}

	if (this->timer) {
		this->timer->cancel();
		this->timer = nullptr;
	}

	if (this->local_min_duration.getTimeout()) {
		this->timer = std::make_shared<TimerService::Timer>([this]()->void{ this->updateCurrentAction(); }, this->local_min_duration);
		TimerService::globalTimer(TASK_PRIORITY_SERVICE).submit(this->timer);
	}
}

void IPMILED::applyPhysicalAction() {
	MutexGuard<true> lock(this->mutex, true);
	struct Action action = this->getCurrentPhysicalAction();

	switch (action.effect) {
	case OFF:   this->led.off(); break;
	case ON:    this->led.on(); break;
	case DIM:   this->led.dim(action.intensity); break;
	case BLINK: this->led.blink(action.period_ms, action.time_on_ms); break;
	case PULSE: this->led.pulse(action.period_ms); break;
	default:
		// Uh. Guess we'll just pass on this one.
		break;
	}
}
