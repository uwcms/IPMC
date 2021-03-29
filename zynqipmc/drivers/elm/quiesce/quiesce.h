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
	 * @param subscribe_validity The duration after which a "QUIESCE_SUBSCRIBE"
	 *                           request will be forgotten.
	 * @param logtree The logtree node to log to.
	 */
	ELMQuiesce(ELM &elm, TickType_t subscribe_validity,
			TickType_t quiesce_timeout, LogTree *logtree=NULL) :
			ELM::Channel(elm, "quiesce"), logtree(logtree),
			subscribe_validity(subscribe_validity), subscribe_timeout(0ULL),
			quiesce_timeout(quiesce_timeout), quiesce_timer(nullptr) {
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
	 * Send a quiesce request to the ELM.
	 *
	 * @param callback Callback to be called when the operation is complete
	 *
	 * \note callback may be called immediately in the current thread if the
	 *       ELM does not currently support quiescence.  It may be called from
	 *       another thread if an attempt at quiescence is made.
	 *
	 * \note If you call this with a quiesce already in progress, the timeout
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

	/**
	 * Determine if the ELM currently supports quiescence.
	 * @return true if the ELM is currently accepting quiesce commands, else false
	 */
	bool quiesceSubscribed();

	// From base class ConsoleCommandSupport:
	virtual void registerConsoleCommands(CommandParser &parser, const std::string &prefix="");
	virtual void deregisterConsoleCommands(CommandParser &parser, const std::string &prefix="");

protected:
	virtual void recv(const uint8_t *content, size_t size);

	SemaphoreHandle_t mutex; ///< A mutex to guard internal state.
	LogTree *logtree; ///< The logtree to log to.
	TickType_t subscribe_validity; ///< The duration a subscribe is valid for.
	AbsoluteTimeout subscribe_timeout; ///< A timeout for the current subscription.
	TickType_t quiesce_timeout; ///< The timeout duration for a quiesce.
	std::shared_ptr<TimerService::Timer> quiesce_timer; ///< The timer handle for timing out a failed quiesce.
	std::list<QuiesceCompleteCallback> callbacks; ///< The callbacks to trigger when a quiesce is complete.
};

#endif /* SRC_COMMON_ZYNQIPMC_DRIVERS_ELM_QUIESCE_H_ */
