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

#ifndef SRC_COMMON_ZYNQIPMC_SERVICES_GEOIP_GEOIP_H_
#define SRC_COMMON_ZYNQIPMC_SERVICES_GEOIP_GEOIP_H_

#include <drivers/elm/elm.h>
#include <libs/logtree/logtree.h>
#include <libs/threading.h>
#include <map>
#include <semphr.h>
#include <services/console/command_parser.h>

/**
 * A class providing Geo-Location probing facilities.
 */
class GeoIP : public ConsoleCommandSupport {
public:
	/**
	 * Instantiate a GeoIP module.
	 *
	 * \param logtree The LogTree to output messages to.
	 * \param ipmb The IPMB to use for built-in probing strategies (if available).
	 * \param elm The ELM interface to provide ELMLink service to.
	 */
	GeoIP(LogTree &logtree, IPMBSvc *ipmb, ELM *elm);
	virtual ~GeoIP();

	class GeoLocation {
	public:
		std::string crate_id;
		uint8_t slot_id; // Value "0" reserved for unknown.
		bool successful;
		GeoLocation(const std::string &crate_id, uint8_t slot_id)
		    : crate_id(crate_id), slot_id(slot_id), successful(true){};
		GeoLocation()
		    : crate_id(""), slot_id(0), successful(false){};
	};

	/**
	 * This function returns a mapping of all available/executed strategy
	 * results.
	 *
	 * \return A mapping of strategy names to available strategy results.
	 */
	const std::map<std::string, GeoLocation> getGeoLocation();

	/**
	 * Run all strategies that have not yet registered results.  If a run is in
	 * progress, a new run will not be started.
	 *
	 * \note This will only run strategies that are a part of the core module.
	 *       Externally-implemented board-specific strategies must be rerun in
	 *       the relevant board-specific manner.
	 *
	 * \return A WaitList to wait for completion of the current strategy run.
	 */
	std::shared_ptr<WaitList<false>> runStrategies();

	/**
	 * Reset all strategies that have stored a failure result, so that they will
	 * be executed again at next run.
	 */
	void resetFailedStrategies();

	/**
     * This function supports board-specific strategies not implemented in the
	 * core module. These strategies may be run externally and submit their
	 * results when complete.
	 *
	 * \param strategy The name of the strategy.
	 * \param result The GeoLocation deduced by this strategy.
	 */
	void setStrategyResult(const std::string &strategy, const GeoLocation &result);

	// From base class ConsoleCommandSupport:
	virtual void registerConsoleCommands(CommandParser &parser, const std::string &prefix = "");
	virtual void deregisterConsoleCommands(CommandParser &parser, const std::string &prefix = "");

protected:
	LogTree &logtree;        ///< The LogTree for status logging.
	IPMBSvc *ipmb;           ///< The IPMB to use for probing our crate & slot identifiers.
	SemaphoreHandle_t mutex; ///< The mutex.

	class ELMLinkService;
	ELMLinkService *elmlink; ///< Our ELMLink Service, if present.

	std::map<std::string, GeoLocation> cache; ///< A cache for identified GeoLocation data.

	std::shared_ptr<WaitList<false>> strategy_waitlist; ///< A waitlist for the strategy runner.
	void runStrategyThread();
};

#endif /* SRC_COMMON_ZYNQIPMC_SERVICES_FAULTLOG_FAULTLOG_H_ */
