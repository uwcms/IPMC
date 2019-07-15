/*
 * IPMILED.h
 *
 *  Created on: Jan 16, 2019
 *      Author: jtikalsky
 */

#ifndef SRC_COMMON_UW_IPMC_SERVICES_IPMI_IPMI_LED_IPMILED_H_
#define SRC_COMMON_UW_IPMC_SERVICES_IPMI_IPMI_LED_IPMILED_H_

#include <drivers/generics/led.h>
#include <libs/ThreadingPrimitives.h>
#include <services/timer/TimerService.h>

class IPMI_LED {
public:
	IPMI_LED(LED &led);
	virtual ~IPMI_LED();

	enum Effect {
		INACTIVE,
		OFF,
		ON,
		DIM,
		BLINK,
		PULSE,
	};
	enum ControlLevel {
		LOCAL,
		OVERRIDE,
		LAMPTEST,
	};
	struct Action {
		Effect effect;
		uint64_t min_duration;
		float intensity;
		uint32_t periodMs;
		uint32_t timeOnMs;
		enum ControlLevel control_level; // Return only, not setable
	};
	void submit(struct Action action);
	void override(struct Action action);
	void lamp_test(uint64_t duration);
	struct Action get_current_local_action();
	struct Action get_current_override_action();
	AbsoluteTimeout get_current_lamp_test_duration();
	struct Action get_current_physical_action();
	void reset_local();
protected:
	SemaphoreHandle_t mutex; ///< A mutex protecting the internal mechanisms
	LED &led; ///< The LED controlled by this mechanism.

	AbsoluteTimeout local_min_duration; ///< The remaining minimum duration of the current action.
	struct Action local_action; ///< The current action.
	struct Action override_action; ///< The non-local (override) action.
	AbsoluteTimeout lamp_test_timeout; ///< A timeout for the latest lamp test.
	std::list<struct Action> future_actions; ///< A queue of future actions.
	std::shared_ptr<TimerService::Timer> timer; ///< The current 'next stage' timer.

	void update_current_action();
	void apply_physical_action();
};

#endif /* SRC_COMMON_UW_IPMC_SERVICES_IPMI_IPMI_LED_IPMILED_H_ */
