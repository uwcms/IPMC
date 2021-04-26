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

#include "infolink.h"
#include <exception>
#include <libs/mjson/mjson.h>
#include <libs/printf.h>
#include <libs/threading.h>
#include <services/console/consolesvc.h>
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

std::string InfoLink::MultiTypeValue::renderJSON() {
	if (this->value_type == TYPE_NULL)
		return "null";
	else if (this->value_type == TYPE_BOOL)
		return this->vsimple.vbool ? "true" : "false";
	else if (this->value_type == TYPE_INT)
		return stdsprintf("%d", this->vsimple.vint);
	else if (this->value_type == TYPE_UINT32)
		return stdsprintf("%lu", this->vsimple.vuint32);
	else if (this->value_type == TYPE_FLOAT)
		return stdsprintf("%f", this->vsimple.vfloat);
	else if (this->value_type == TYPE_STRING)
		return render_json_string(this->vstring);
	else if (this->value_type == TYPE_LAMBDA) {
		MultiTypeValue current = this->vlambda();
		if (current.value_type == TYPE_LAMBDA)
			throw std::range_error("MultiTypeValue-Lambda returned MultiTypeValue-Lambda");
		else
			return current.renderJSON();
	}
	else
		throw std::domain_error("MultiTypeValue has invalid type.");
}

InfoLink::InfoLink(ELM &elm)
    : ELM::Channel(elm, "Info") {
	this->mutex = xSemaphoreCreateRecursiveMutex();
}

InfoLink::~InfoLink() {
	xSemaphoreTakeRecursive(this->mutex, portMAX_DELAY);
	vSemaphoreDelete(this->mutex);
}

std::map<std::string, InfoLink::MultiTypeValue> *InfoLink::info = nullptr;
static SemaphoreHandle_t static_mutex = nullptr; ///< A mutex to guard static things.
void InfoLink::setInfo(std::string key, MultiTypeValue value) {
	safe_init_static_mutex(static_mutex, false);
	MutexGuard<false> lock(static_mutex);
	if (!info)
		info = new std::map<std::string, InfoLink::MultiTypeValue>();
	(*info)[key] = value;
};
void InfoLink::delInfo(std::string key) {
	safe_init_static_mutex(static_mutex, false);
	MutexGuard<false> lock(static_mutex);
	if (!info)
		info = new std::map<std::string, InfoLink::MultiTypeValue>();
	if (info->count(key))
		info->erase(key);
};
std::map<std::string, InfoLink::MultiTypeValue> InfoLink::getMyInfo() {
	safe_init_static_mutex(static_mutex, false);
	MutexGuard<false> lock(static_mutex);
	if (!info)
		info = new std::map<std::string, InfoLink::MultiTypeValue>();
	return *info;
};

std::string InfoLink::getELMInfo(TickType_t timeout) {
	if (timeout == 0) {
		MutexGuard<true> lock(this->mutex);
		return this->last_elm_info;
	}
	WaitList<true>::Subscription sub = this->waitlist.join();
	this->send("GET_INFO");
	if (sub.wait(timeout)) {
		MutexGuard<true> lock(this->mutex);
		return this->last_elm_info;
	}
	else {
		return "";
	}
}

void InfoLink::recv(const uint8_t *content, size_t size) {
	MutexGuard<true> lock(this->mutex);
	std::string message((const char *)content, size);
	if (message == "GET_INFO")
		this->sendInfo();
	else if (message.substr(0, 5) == "INFO ") {
		this->last_elm_info = message.substr(5);
		this->waitlist.wake();
	}
};
void InfoLink::sendInfo() {
	std::string packet = "{\n";
	for (auto item : this->getMyInfo())
		packet += stdsprintf("\t%s: %s,\n", render_json_string(item.first).c_str(), item.second.renderJSON().c_str());
	// We have either "{\n" or something ending in ",\n".
	if (packet == "{\n")
		this->send("INFO {}");
	else
		this->send(std::string("INFO ") + packet.substr(0, packet.size() - 2) + "\n}");
}

namespace
{
/// A "get_location" console command.
class GetELMInfoCommand : public CommandParser::Command {
public:
	GetELMInfoCommand(InfoLink &infolink) : infolink(infolink){};

	virtual std::string getHelpText(const std::string &command) const {
		return command + "\n\n"
		                 "Get the ELM's Info.\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		std::string info = this->infolink.getELMInfo(5000);
		if (info.size())
			console->write(info + "\n");
		else
			console->write("ELM info not available.\n");
	}

private:
	InfoLink &infolink;
};
} // anonymous namespace

void InfoLink::registerConsoleCommands(CommandParser &parser, const std::string &prefix) {
	parser.registerCommand(prefix + "get_elm_info", std::make_shared<GetELMInfoCommand>(*this));
}
void InfoLink::deregisterConsoleCommands(CommandParser &parser, const std::string &prefix) {
	parser.registerCommand(prefix + "get_elm_info", nullptr);
}
