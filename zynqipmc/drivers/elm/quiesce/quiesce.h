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

#ifndef SRC_COMMON_ZYNQIPMC_DRIVERS_ELM_QUIESCE_H_
#define SRC_COMMON_ZYNQIPMC_DRIVERS_ELM_QUIESCE_H_

#include <FreeRTOS.h>
#include <string>
#include <functional>
#include <memory>
#include <libs/logtree/logtree.h>
#include <libs/threading.h>
#include <services/timer/timer.h>
#include <drivers/elm/elm.h>
#include <list>

/**
 * A quiesce driver, using ELM-Link communication to request a clean shutdown
 * and wait for it.
 */
class ELMQuiesce final : public ELM::Channel {
public:
	/**
	 * Initializes and links the channel to target ELM interface.
	 * @param elm ELM interface where channel exists.
	 * @param startup_allowance The maximum time after power-on your ELM may take
	 *                          to begin responding to quiesce requests.
	 * @param panic_window The length of time after power-on during which no quiesce
	 *                     is required.  ("Oops, I didn't mean to put that handle in!")
	 * @param acknowledgement_timeout The length of time to wait for acknowledgement
	 *                                of a quiesce request.
	 * @param quiesce_timeout The maximum length of time to wait for quiescence
	 *                        after a request is acknowledged.
	 * @param logtree The logtree node to log to.
	 */
	ELMQuiesce(ELM &elm, TickType_t startup_allowance, TickType_t panic_window,
			TickType_t acknowledgement_timeout, TickType_t quiesce_timeout,
			LogTree *logtree = NULL) :
			ELM::Channel(elm, "quiesce"), logtree(logtree), startup_allowance(
					startup_allowance), panic_window(panic_window), acknowledgement_timeout(
					acknowledgement_timeout), quiesce_timeout(quiesce_timeout), startup_timestamp(
					0), quiesce_request_timestamp(0), quiesce_acknowledgement_timestamp(
					0), fsm_timer(nullptr), send_request_timer(nullptr) {
		this->mutex = xSemaphoreCreateRecursiveMutex();
		configASSERT(this->mutex);
	};
	virtual ~ELMQuiesce();

	/**
	 * A function handle to be called when the quiesce has completed, either
	 * successfuly, or by timeout.
	 *
	 * @param successful true if the quiesce was confirmed, false otherwise
	 */
	typedef std::function<void(bool)> QuiesceCompleteCallback;

	/**
	 * A function to notify the quiesce manager when the ELM power is enabled
	 * for use in timing calculations.
	 */
	void elm_powered_on();
	/**
	 * A function to notify the quiesce manager when the ELM power is removed
	 * for use in timing calculations.
	 */
	void elm_powered_off();

	/**
	 * Send a quiesce request to the ELM.
	 *
	 * @param callback Callback to be called when the operation is complete
	 *
	 * \note callback may be called immediately in the current thread or from
	 *       another thread at a later time.
	 *
	 * \note If you call this with a quiesce already in progress, the timeouts
	 *       will not be updated, but your callback will still be enqueued.
	 */
	void quiesce(QuiesceCompleteCallback callback);

	/**
	 * Indicate that a quiesce has been completed, in case you have some other
	 * out of band signalling method.
	 * @param successful true if the quiesce succeeded, else false
	 */
	void quiesceComplete(bool successful=true);

	/**
	 * Determine if a quiesce is in progress.
	 * @return true if a quiesce is in progress, else false
	 */
	bool quiesceInProgress();

	// From base class ConsoleCommandSupport:
	virtual void registerConsoleCommands(CommandParser &parser, const std::string &prefix="");
	virtual void deregisterConsoleCommands(CommandParser &parser, const std::string &prefix="");

protected:
	virtual void recv(const uint8_t *content, size_t size);

	SemaphoreHandle_t mutex; ///< A mutex to guard internal state.
	LogTree *logtree; ///< The logtree to log to.

	TickType_t startup_allowance;
	TickType_t panic_window;
	TickType_t acknowledgement_timeout;
	TickType_t quiesce_timeout;

	uint64_t startup_timestamp; ///< A timestamp of power-on, to base various windows on.
	uint64_t quiesce_request_timestamp; ///< A timestamp of when quiesce was requested, to base timeouts on.
	uint64_t quiesce_acknowledgement_timestamp; ///< A timestamp of when quiesce was acknowledged, to base timeouts on.
	std::shared_ptr<TimerService::Timer> fsm_timer; ///< The timer for the next FSM update.
	std::shared_ptr<TimerService::Timer> send_request_timer; ///< The timer for sending "QUIESCE_NOW".

	std::list<QuiesceCompleteCallback> callbacks; ///< The callbacks to trigger when a quiesce is complete.

	void quiesce_fsm();
};

#endif /* SRC_COMMON_ZYNQIPMC_DRIVERS_ELM_QUIESCE_H_ */
