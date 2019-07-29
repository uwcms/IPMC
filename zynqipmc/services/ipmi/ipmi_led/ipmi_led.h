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

#ifndef SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_IPMI_LED_IPMI_LED_H_
#define SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_IPMI_LED_IPMI_LED_H_

#include <drivers/generics/led.h>
#include <libs/threading.h>
#include <services/timer/timer.h>

/**
 * Allows LEDs to have complex sequences which is required per the PICMG spec.
 */
class IPMILED {
public:
	/**
	 * Associates a specific LED to this IPMI LED controller.
	 * @param led The LED reference to associate.
	 */
	IPMILED(LED &led);
	virtual ~IPMILED();

	//! The different types of supported modes.
	enum Effect {
		INACTIVE,
		OFF,
		ON,
		DIM,
		BLINK,
		PULSE,
	};

	//! The type of control we can have on the LED.
	enum ControlLevel {
		LOCAL,
		OVERRIDE,
		LAMPTEST,
	};

	/**
	 * Each action is represented by this structure which covers
	 * everything the LED can do.
	 */
	struct Action {
		Effect effect;			///< Desired effect.
		uint64_t min_duration;	///< Effect duration.
		float intensity;		///< Dim intensity.
		uint32_t period_ms;		///< Pulse intensity in milliseconds.
		uint32_t time_on_ms;	///< Active time during period interval in milliseconds.
		enum ControlLevel control_level; // Return only, not setable
	};

	//! Add a new action to the action to the queue.
	void submit(struct Action action);

	//! Override and set the LED with a new action immediately.
	void override(struct Action action);

	//! Do a lamp test, as part of the IPMI spec. Duration in FreeRTOS ticks.
	void lampTest(uint64_t duration);

	//! Get current LED action.
	struct Action getCurrentLocalAction();

	//! Get current LED action if in override state.
	struct Action getCurrentOverrideAction();

	//! Get the current lamp test duration.
	AbsoluteTimeout getCurrentLampTestDuration();

	//! Get the current physical action.
	struct Action getCurrentPhysicalAction();

	//! Reset the LED behavior back to default.
	void resetLocal();

protected:
	SemaphoreHandle_t mutex;	///< A mutex protecting the internal mechanisms.
	LED &led;					///< The LED controlled by this mechanism.

	AbsoluteTimeout local_min_duration;			///< The remaining minimum duration of the current action.
	struct Action local_action;					///< The current action.
	struct Action override_action;				///< The non-local (override) action.
	AbsoluteTimeout lampTest_timeout;			///< A timeout for the latest lamp test.
	std::list<struct Action> future_actions;	///< A queue of future actions.
	std::shared_ptr<TimerService::Timer> timer;	///< The current 'next stage' timer.

	void updateCurrentAction();
	void applyPhysicalAction();
};

#endif /* SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_IPMI_LED_IPMI_LED_H_ */
