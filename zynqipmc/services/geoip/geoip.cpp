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

#include <core.h>
#include <libs/mjson/mjson.h>
#include <libs/printf.h>
#include <libs/threading.h>
#include <services/console/consolesvc.h>
#include <services/geoip/geoip.h>
#include <services/ipmi/remote_fru_storage.h>
#include <stdint.h>

/**
 * A wrapper to cleanly render a JSON string using mjson.
 *
 * \note This isn't super efficient.
 *
 * \param str The string to render.
 * \return The final string.
 */
static std::string render_json_string(const std::string &str) {
	// mjson is not great at outputting json, lets just do this ourselves...
	static const std::map<char, const char *> escapes{
	    {'"', "\\\""},
	    {'\\', "\\\\"},
	    {'\b', "\\b"},
	    {'\f', "\\f"},
	    {'\n', "\\n"},
	    {'\r', "\\r"},
	    {'\t', "\\t"},
	};
	std::string out = "\"";
	for (auto c : str) {
		if (escapes.count(c))
			out += escapes.at(c);
		else if (c < ' ' || c > '~')
			out += stdsprintf("\\u%04hhX", (uint8_t)c);
		else
			out += c;
	}
	return out + "\"";
}

class GeoIP::ELMLinkService : public ELM::Channel {
public:
	ELMLinkService(ELM &elm, GeoIP &geoip) : ELM::Channel(elm, "GeoIP"), geoip(geoip){};

	void sendLocation() {
		const std::map<std::string, GeoIP::GeoLocation> results = this->geoip.getGeoLocation();
		std::string packet;
		for (auto result : results) {
			if (result.second.successful)
				packet += stdsprintf("{\"strategy\":%s,\"successful\":true,\"crate_id\":%s,\"slot_id\":%hhu},", render_json_string(result.first).c_str(), render_json_string(result.second.crate_id).c_str(), result.second.slot_id);
			else
				packet += stdsprintf("{\"strategy\":%s,\"successful\":false},", render_json_string(result.first).c_str());
		}
		// We now have a JSON list with trailing ',', or nothing.
		if (packet.size())
			packet = std::string("[") + packet.substr(0, packet.size() - 1) + "]";
		else
			packet = "[]";

		this->send(std::string("GET_LOCATION ") + packet);
	};

protected:
	GeoIP &geoip; ///< The GeoIP to use.
	virtual void recv(const uint8_t *content, size_t size) {
		std::string message((const char *)content, size);
		if (message == "RUN_STRATEGIES") {
			this->geoip.runStrategies();
		}
		else if (message == "RESET_FAILED") {
			this->geoip.resetFailedStrategies();
		}
		else if (message == "GET_LOCATION") {
			this->sendLocation();
		}
	}
};

GeoIP::GeoIP(LogTree &logtree, IPMBSvc *ipmb, ELM *elm)
    : logtree(logtree), ipmb(ipmb), elmlink(nullptr) {
	this->mutex = xSemaphoreCreateRecursiveMutex();
	if (elm)
		this->elmlink = new ELMLinkService(*elm, *this);
	this->runStrategies();
}

GeoIP::~GeoIP() {
	// Not supported.  We don't have a way to safely, synchronously kill our
	// runStrategies thread.
	xSemaphoreTakeRecursive(this->mutex, portMAX_DELAY);
	if (this->elmlink)
		delete this->elmlink;
	vSemaphoreDelete(this->mutex);
}

const std::map<std::string, GeoIP::GeoLocation> GeoIP::getGeoLocation() {
	MutexGuard<true> lock(this->mutex);
	return this->cache;
}

void GeoIP::setStrategyResult(const std::string &strategy, const GeoLocation &result) {
	MutexGuard<true> lock(this->mutex);
	this->cache[strategy] = result;
	if (result.successful)
		this->logtree.log(stdsprintf("Strategy \"%s\" found location \"%s\", slot %hhu\n", strategy.c_str(), result.crate_id.c_str(), result.slot_id), LogTree::LOG_NOTICE);
	else
		this->logtree.log(stdsprintf("Strategy \"%s\" failed.\n", strategy.c_str()), LogTree::LOG_NOTICE);
}

std::shared_ptr<WaitList<false>> GeoIP::runStrategies() {
	MutexGuard<true> lock(this->mutex);
	if (this->strategy_waitlist == nullptr) {
		this->strategy_waitlist = std::make_shared<WaitList<false>>();
		runTask("GeoIP", TASK_PRIORITY_SERVICE, [this]() -> void { this->runStrategyThread(); });
	}
	return this->strategy_waitlist;
}

void GeoIP::resetFailedStrategies() {
	MutexGuard<true> lock(this->mutex);
	for (auto it = this->cache.begin(); it != this->cache.end(); /* in-body */) {
		if (!it->second.successful)
			it = this->cache.erase(it);
		else
			++it;
	}
}

void GeoIP::runStrategyThread() {
	/* We will not hold the mutex the whole time, so that cached data can still
	 * be easily retrieved, and other related requests can go through properly.
	 */

	class Strategy {
	public:
		GeoLocation value;
		bool pending;
		bool updated;
		bool requires_ipmb;
		Strategy(bool requires_ipmb) : pending(true), updated(false), requires_ipmb(requires_ipmb){};
		void update(const GeoLocation &value) {
			this->value = value;
			this->pending = false;
			this->updated = true;
		};
	};

	std::map<std::string, Strategy> strategies{
	    {"first_rackmount_chassis_serial", Strategy(true)},
	};

	{
		// Load cached results, to avoid doing extra work.
		MutexGuard<true> lock(this->mutex);
		for (auto rec : this->cache)
			if (strategies.count(rec.first))
				strategies.at(rec.first).pending = false;
	}

	if (this->ipmb) {
		uint8_t ipmb_slot_id = 0;
		uint8_t ipmb_address = this->ipmb->getIPMBAddress();
		if (ipmb_address >= 0x82 && ipmb_address <= 0xA0) {
			// PICMG 3.0 Table 3-4 "Assigned Hardware Addresses and IPMB addresses"
			ipmb_slot_id = (ipmb_address - 0x80) / 2;
		}

		// Our only current strategy enumerates FRU Data.  Others might, in the
		// future.
		for (int i = 0; i < 127; ++i) {
			// Currently this only checks one, and you could break above, but we may
			// implement others later which would need to continue.
			bool fru_done = true;
			for (auto strategy : strategies)
				if (strategy.second.requires_ipmb && strategy.second.pending)
					fru_done = false;
			if (fru_done)
				break; // Don't need more FRU Data.

			// Read out the next FRU Storage.
			std::shared_ptr<RemoteFRUStorage> storage = RemoteFRUStorage::probe(this->ipmb, 0x20, i);
			if (!storage)
				break; // Guess we hit the end.
			std::shared_ptr<RemoteFRUStorage::ChassisInfo> chassis = storage->readChassisInfoArea();
			if (!chassis)
				continue; // We got a record but it didn't have Chassis Info.

			if (strategies.at("first_rackmount_chassis_serial").pending && chassis->type == 0x17 /* "Rack Mount Chassis" */) {
				// Cool, this is what our "first_rackmount_chassis_serial" strategy needs.
				strategies.at("first_rackmount_chassis_serial").update(GeoLocation(trim_string(chassis->serial_number), ipmb_slot_id));
			}
		}
	}
	else {
		for (auto strategy : strategies)
			if (strategy.second.pending && strategy.second.requires_ipmb)
				strategy.second.update(GeoLocation()); // failed.
	}

	// Record results or failures from our FRU Data probe.
	for (auto strategy : strategies)
		if (strategy.second.updated)
			this->setStrategyResult(strategy.first, strategy.second.value);

	{
		MutexGuard<true> lock(this->mutex);
		this->strategy_waitlist->wake();
		this->strategy_waitlist = nullptr;
	}

	if (this->elmlink)
		this->elmlink->sendLocation();
}

namespace
{
/// A "get_location" console command.
class GetLocationCommand : public CommandParser::Command {
public:
	GetLocationCommand(GeoIP &geoip) : geoip(geoip){};

	virtual std::string getHelpText(const std::string &command) const {
		return command + "\n\n"
		                 "Run all incomplete GeoIP location probing strategies and return their results.\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		this->geoip.runStrategies()->join().wait();
		const std::map<std::string, GeoIP::GeoLocation> results = this->geoip.getGeoLocation();
		for (auto result : results) {
			if (result.second.successful)
				console->write(stdsprintf("Strategy \"%s\": Location \"%s\", Slot %hhu\n", result.first.c_str(), result.second.crate_id.c_str(), result.second.slot_id));
			else
				console->write(stdsprintf("Strategy \"%s\" Failed.\n", result.first.c_str()));
		}
	}

private:
	GeoIP &geoip;
};

/// A "reset_failed" console command.
class ResetFailedCommand : public CommandParser::Command {
public:
	ResetFailedCommand(GeoIP &geoip) : geoip(geoip){};

	virtual std::string getHelpText(const std::string &command) const {
		return command + "\n\n"
		                 "Reset the status of all failed GeoIP location probing strategies.\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		this->geoip.resetFailedStrategies();
	}

private:
	GeoIP &geoip;
};
} // anonymous namespace

void GeoIP::registerConsoleCommands(CommandParser &parser, const std::string &prefix) {
	parser.registerCommand(prefix + "get_location", std::make_shared<GetLocationCommand>(*this));
	parser.registerCommand(prefix + "reset_failed", std::make_shared<ResetFailedCommand>(*this));
}
void GeoIP::deregisterConsoleCommands(CommandParser &parser, const std::string &prefix) {
	parser.registerCommand(prefix + "get_location", nullptr);
	parser.registerCommand(prefix + "reset_failed", nullptr);
}
