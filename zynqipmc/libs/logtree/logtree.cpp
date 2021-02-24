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

#include "logtree.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include <libs/printf.h>
#include <libs/except.h>
#include <libs/threading.h>
#include <services/console/command_parser.h>
#include <services/console/consolesvc.h>
#include <deque>
#include <algorithm>

LogTree::LogTree(const std::string& root_label) :
parent(nullptr), kLabel(root_label), kPath(root_label) {
	this->mutex = xSemaphoreCreateRecursiveMutex();
	configASSERT(this->mutex);
}

LogTree::LogTree(const std::string& subtree_label, LogTree &parent) :
parent(&parent), kLabel(subtree_label), kPath(parent.kPath + "." + subtree_label) {
	this->mutex = xSemaphoreCreateRecursiveMutex();
	configASSERT(this->mutex);

	xSemaphoreTakeRecursive(this->parent->mutex, portMAX_DELAY);
	this->filters = this->parent->filters; // Take a copy of the parent's filters.
	xSemaphoreGiveRecursive(this->parent->mutex);

	for (auto it = this->filters.begin(), eit = this->filters.end(); it != eit; ++it) {
		it->second.inheriting = true; // But all our configurations are inherited.
	}
}

LogTree::~LogTree() {
	if (this->parent) {
		// Detach from our parent, so no new operations can begin.
		xSemaphoreTakeRecursive(this->parent->mutex, portMAX_DELAY);
		this->parent->children.erase(this->kLabel);
		xSemaphoreGiveRecursive(this->parent->mutex);
	}

	// Ensure no operation can be in progress at this point, still, somehow.
	xSemaphoreTakeRecursive(this->mutex, portMAX_DELAY);

	// Clean up our children.
	while (!this->children.empty()) {
		delete this->children.begin()->second;
	}

	vSemaphoreDelete(this->mutex);
}

void LogTree::filterSubscribe(Filter *filter, enum LogLevel level, bool inheritance_update) {
	xSemaphoreTakeRecursive(this->mutex, portMAX_DELAY);

	// If this is an inheritance update, and I'm not inheriting this filter, discard it.
	if (inheritance_update && this->filters.count(filter) && !this->filters.at(filter).inheriting) {
		xSemaphoreGiveRecursive(this->mutex);
		return;
	}

	FilterAssociation assoc(level, inheritance_update);
	if (level == LOG_INHERIT) {
		/* If our parent exists, they will have a registration for this filter.
		 * If our parent does not exist, inheritance is disabled and we will
		 * default to LOG_SILENT.
		 */
		if (!this->parent) {
			// Can't inherit without a parent.
			assoc.level = LOG_SILENT;
			assoc.inheriting = false;
		} else {
			xSemaphoreTakeRecursive(this->parent->mutex, portMAX_DELAY);
			configASSERT(this->parent->filters.count(filter));
			// Inherit our parent's level and continue inheritance in the future.
			assoc.level = this->parent->filters.at(filter).level;
			assoc.inheriting = true;
			xSemaphoreGiveRecursive(this->parent->mutex);
		}
	}

	// Update myself.
	if (this->filters.count(filter)) {
		this->filters.at(filter) = assoc;
	} else {
		this->filters.insert(std::make_pair(filter, assoc));
	}

	/* Update inheriting children.
	 *
	 * This won't cause mutex troubles, as we never pass along our parameter AS LOG_INHERIT, so they needn't check their parent.
	 */
	for (auto it = this->children.begin(), eit = this->children.end(); it != eit; ++it) {
		it->second->filterSubscribe(filter, assoc.level, true);
	}

	xSemaphoreGiveRecursive(this->mutex);
}

void LogTree::filterUnsubscribe(Filter *filter) {
	/* There's a deadlock condition here, since this takes the same locks, in
	 * the opposite order (when recursion is factored) as filterSubscribe when
	 * filterSubscribe, however as this is only ever run during the filter
	 * destructor, and you obviously are not subscribing through a filter object
	 * that you are actively deleting, this case is not relevant.
	 *
	 * This could be avoided by way of a tree-global lock, which must be taken
	 * (with no local locks held), before any tree spanning functions.  This is
	 * not worth the time, given the above reasoning.
	 */
	xSemaphoreTakeRecursive(this->mutex, portMAX_DELAY);

	this->filters.erase(filter);
	for (auto it = this->children.begin(), eit = this->children.end(); it != eit; ++it) {
		it->second->filterUnsubscribe(filter);
	}

	xSemaphoreGiveRecursive(this->mutex);
}

LogTree& LogTree::operator[](const std::string &label) {
	xSemaphoreTakeRecursive(this->mutex, portMAX_DELAY);
	if (!this->children.count(label)) {
		this->children.insert(std::make_pair(label, new LogTree(label, *this)));
	}
	LogTree *ret = this->children.at(label);
	xSemaphoreGiveRecursive(this->mutex);
	return *ret;
}

LogTree::size_type LogTree::getChildCount() {
	xSemaphoreTakeRecursive(this->mutex, portMAX_DELAY);
	size_type count = this->children.size();
	xSemaphoreGiveRecursive(this->mutex);
	return count;
}

LogTree::size_type LogTree::getChildCount(const std::string &label) {
	xSemaphoreTakeRecursive(this->mutex, portMAX_DELAY);
	size_type count = this->children.count(label);
	xSemaphoreGiveRecursive(this->mutex);
	return count;
}

std::vector<std::string> LogTree::listChildren() {
	xSemaphoreTakeRecursive(this->mutex, portMAX_DELAY);

	std::vector<std::string> children;
	children.reserve(this->children.size());
	for (auto it = this->children.begin(), eit = this->children.end(); it != eit; ++it) {
		children.push_back(it->first);
	}

	xSemaphoreGiveRecursive(this->mutex);
	return std::move(children);
}

void LogTree::log(const std::string &message, enum LogLevel level) {
	if (level == LOG_SILENT) {
		throw std::domain_error("Messages may not be logged at level LOG_SILENT.");
	}

	xSemaphoreTakeRecursive(this->mutex, portMAX_DELAY);
	for (auto it = this->filters.begin(), eit = this->filters.end(); it != eit; ++it) {
		if (it->second.level >= level && it->first->handler) {
			it->first->handler(*this, message, level);
		}
	}
	xSemaphoreGiveRecursive(this->mutex);
}

/// A "log" console command.
class LogTree::LogCommand : public CommandParser::Command {
public:
	LogCommand(LogTree &logtree) : logtree(logtree) { };

	virtual std::string getHelpText(const std::string &command) const {
		return command + " LOGLEVEL \"text\" [...]\n"
				"\n"
				"LOGLEVEL is any prefix of:\n"
				"  CRITICAL\n"
				"  ERROR\n"
				"  WARNING\n"
				"  NOTICE\n"
				"  INFO\n"
				"  DIAGNOSTIC\n"
				"  TRACE\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		std::string out;
		std::string levelstr;
		if (!parameters.parseParameters(1, false, &levelstr)) {
			console->write("Invalid parameters. See help.\n");
			return;
		}
		LogTree::LogLevel level = LogTree::LOG_SILENT;
		if (levelstr == std::string("CRITICAL").substr(0,levelstr.size())) {
			level = LogTree::LOG_CRITICAL;
		} else if (levelstr == std::string("ERROR").substr(0,levelstr.size())) {
			level = LogTree::LOG_ERROR;
		} else if (levelstr == std::string("WARNING").substr(0,levelstr.size())) {
			level = LogTree::LOG_WARNING;
		} else if (levelstr == std::string("NOTICE").substr(0,levelstr.size())) {
			level = LogTree::LOG_NOTICE;
		} else if (levelstr == std::string("INFO").substr(0,levelstr.size())) {
			level = LogTree::LOG_INFO;
		} else if (levelstr == std::string("DIAGNOSTIC").substr(0,levelstr.size())) {
			level = LogTree::LOG_DIAGNOSTIC;
		} else if (levelstr == std::string("TRACE").substr(0,levelstr.size())) {
			level = LogTree::LOG_TRACE;
		}

		if (level == LogTree::LOG_SILENT) {
			console->write("Unknown loglevel.\n");
			return;
		}

		for (CommandParser::CommandParameters::size_type i = 2; i < parameters.nargs(); ++i) {
			std::string arg;
			parameters.parseParameters(i, false, &arg);
			if (out.size()) out += " ";
			out += arg;
		}
		this->logtree.log(out, level);
	}

private:
	LogTree &logtree; ///< The logtree to log to.
};

void LogTree::registerConsoleCommands(CommandParser &parser, const std::string &prefix) {
	parser.registerCommand(prefix + "log", std::make_shared<LogTree::LogCommand>(*this));
}

void LogTree::deregisterConsoleCommands(CommandParser &parser, const std::string &prefix) {
	parser.registerCommand(prefix + "log", nullptr);
}

void LogTree::Filter::reconfigure(LogTree &logtree, enum LogLevel level) {
	LogTree *root = &logtree;
	while (root->parent) root = root->parent;
	configASSERT(root == this->logtree);
	logtree.filterSubscribe(this, level, false);
}

enum LogTree::LogLevel LogTree::Filter::getConfiguration(LogTree &logtree) {
	LogTree *root = &logtree;
	while (root->parent) root = root->parent;
	configASSERT(root == this->logtree);

	xSemaphoreTakeRecursive(logtree.mutex, portMAX_DELAY);
	configASSERT(logtree.filters.count(this));
	enum LogLevel level = logtree.filters.at(this).level;
	if (logtree.filters.at(this).inheriting) {
		level = LOG_INHERIT;
	}
	xSemaphoreGiveRecursive(logtree.mutex);
	return level;
}

LogTree::Filter::Filter(LogTree &logtree, handler_t handler, enum LogLevel level)
	: handler(handler) {
	LogTree *root = &logtree;
	while (root->parent) root = root->parent;
	this->logtree = root;
	if (root != &logtree) {
		root->filterSubscribe(this, LOG_SILENT, false); // Subscribe to the full tree itself, first.
	}
	logtree.filterSubscribe(this, level, false);
}

LogTree::Filter::~Filter() {
	this->logtree->filterUnsubscribe(this);
}

class LogTree::Filter::LogLevelCommand : public CommandParser::Command {
public:
	LogLevelCommand(LogTree::Filter &filter, LogTree &root) : filter(filter), root(root) { };

	virtual std::string getHelpText(const std::string &command) const {
		return command +
				"%s logtree [LOGLEVEL]\n"
				"\n"
				"With a loglevel parameter, change the current loglevel of the target.\n"
				"Without a loglevel parameter, console->write the current loglevel of the target.\n"
				"  You may specify target.* to list immediate children's loglevels.\n"
				"  You may specify the special target * to list ALL log facilities.\n"
				"\n"
				"LOGLEVEL is any prefix of:\n"
				"  SILENT\n"
				"  CRITICAL\n"
				"  ERROR\n"
				"  WARNING\n"
				"  NOTICE\n"
				"  INFO\n"
				"  DIAGNOSTIC\n"
				"  TRACE\n"
				"  ALL\n"
				"  PARENT (restore inheritance)\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		std::string out;
		std::string facilitystr;
		std::string levelstr;

		if (!parameters.parseParameters(1, true, &facilitystr)) {
			console->write("Invalid parameters. See help.\n");
			return;
		}

		parameters.parseParameters(2, true, &levelstr); // Optionally parse levelstr

		LogTree *facility = &this->root;
		if (facilitystr == "*") {
			// MASS Listings!
			std::map<std::string, LogTree*> facilities;
			std::deque<LogTree*> open_nodes;
			open_nodes.push_back(&this->root);

			while (!open_nodes.empty()) {
				facilities.emplace(open_nodes.front()->kPath, open_nodes.front());
				std::vector<std::string> children = open_nodes.front()->listChildren();
				for (auto it = children.begin(), eit = children.end(); it != eit; ++it) {
					open_nodes.push_back(&((*open_nodes.front())[*it]));
				}
				open_nodes.pop_front();
			}

			for (auto it = facilities.begin(), eit = facilities.end(); it != eit; ++it) {
				LogTree::LogLevel curlevel = this->filter.getConfiguration(*it->second);
				configASSERT(curlevel <= LogTree::LOG_INHERIT);
				console->write(stdsprintf("%-10s %s\n", LogTree::kLogLevelStrings[curlevel], it->second->kPath.c_str()));
			}

			return;
		}
		else if (facilitystr != this->root.kPath) {
			if (facilitystr.substr(0, this->root.kPath.size()+1) != this->root.kPath + ".") {
				console->write("Unknown log facility.\n");
				return;
			}

			facilitystr = facilitystr.substr(this->root.kPath.size()+1);
			while (facilitystr.size()) {
				std::string::size_type nextdot = facilitystr.find_first_of(".");
				std::string part = facilitystr.substr(0, nextdot);

				if (part == "*" && nextdot == std::string::npos && levelstr.empty()) {
					// Listings!
					std::vector<std::string> children = facility->listChildren();
					std::sort(children.begin(), children.end());
					for (auto it = children.begin(), eit = children.end(); it != eit; ++it) {
						LogTree::LogLevel curlevel = this->filter.getConfiguration((*facility)[*it]);
						configASSERT(curlevel <= LogTree::LOG_INHERIT);
						console->write(stdsprintf("%-10s %s\n", LogTree::kLogLevelStrings[curlevel], (*facility)[*it].kPath.c_str()));
					}

					return;
				} else if (!facility->getChildCount(part)) {
					console->write("Unknown log facility.\n");
					return;
				}
				facility = &((*facility)[part]);

				if (nextdot == std::string::npos) {
					break; // No further dots after the one we just processed.
				}
				facilitystr = facilitystr.substr(nextdot+1);
			}
		}

		if (levelstr.empty()) {
			LogTree::LogLevel curlevel = this->filter.getConfiguration(*facility);
			configASSERT(curlevel <= LogTree::LOG_INHERIT);
			console->write(std::string(LogTree::kLogLevelStrings[curlevel]) + "\n");
			return;
		}

		LogTree::LogLevel level = LogTree::LOG_SILENT;
		if (levelstr == std::string("SILENT").substr(0,levelstr.size())) {
			level = LogTree::LOG_SILENT;
		} else if (levelstr == std::string("CRITICAL").substr(0,levelstr.size())) {
			level = LogTree::LOG_CRITICAL;
		} else if (levelstr == std::string("ERROR").substr(0,levelstr.size())) {
			level = LogTree::LOG_ERROR;
		} else if (levelstr == std::string("WARNING").substr(0,levelstr.size())) {
			level = LogTree::LOG_WARNING;
		} else if (levelstr == std::string("NOTICE").substr(0,levelstr.size())) {
			level = LogTree::LOG_NOTICE;
		} else if (levelstr == std::string("INFO").substr(0,levelstr.size())) {
			level = LogTree::LOG_INFO;
		} else if (levelstr == std::string("DIAGNOSTIC").substr(0,levelstr.size())) {
			level = LogTree::LOG_DIAGNOSTIC;
		} else if (levelstr == std::string("TRACE").substr(0,levelstr.size())) {
			level = LogTree::LOG_TRACE;
		} else if (levelstr == std::string("ALL").substr(0,levelstr.size())) {
			level = LogTree::LOG_ALL;
		} else if (levelstr == std::string("PARENT").substr(0,levelstr.size())) {
			level = LogTree::LOG_INHERIT;
		} else {
			console->write("Unknown loglevel. See help.\n");
			return;
		}

		this->filter.reconfigure(*facility, level);
	}

	virtual std::vector<std::string> complete(const CommandParser::CommandParameters &parameters) const {
		if (parameters.cursor_parameter != 1) {
			return std::vector<std::string>(); // Can't help you.
		}

		std::string facilitystr = parameters.parameters[1].substr(0, parameters.cursor_char);
		LogTree *facility = &this->root;
		if (facilitystr.size() <= facility->kPath.size() || facilitystr.substr(0, facility->kPath.size()+1) != facility->kPath+".") {
			return std::vector<std::string>{facility->kPath+"."}; // The only valid value would be the root facility name.
		}

		facilitystr.erase(0, facility->kPath.size()+1); // Remove the root facility.

		std::string::size_type next_dot;
		while ( (next_dot = facilitystr.find_first_of(".")) != std::string::npos ) {
			std::string next_hop = facilitystr.substr(0, next_dot);

			if (facility->getChildCount(next_hop)) {
				facility = &((*facility)[next_hop]);
			} else {
				break;
			}

			facilitystr.erase(0, next_dot+1);
		}

		// And in case we have a fully typed out facility...
		if (facility->getChildCount(facilitystr)) {
			// We have an exact facility name, not followed by a dot.
			facility = &((*facility)[facilitystr]);

			// We're only going to add a dot to indicate it has children, until they tab again.
			return std::vector<std::string>{facility->kPath+"."};
		}
		else {
			// We have an incomplete facility name remaining.
			// Send all children of the last known facility and let the filter sort it out.

			std::vector<std::string> children = facility->listChildren();
			std::vector<std::string> ret;
			for (auto it = children.begin(), eit = children.end(); it != eit; ++it) {
				if ((*facility)[*it].getChildCount()) {
					ret.push_back(facility->kPath + "." + *it + ".");
				} else {
					ret.push_back(facility->kPath + "." + *it);
				}
			}
			return std::move(ret);
		}
	};

private:
	LogTree::Filter &filter; ///< The filter to configure.
	LogTree &root; ///< The root logtree to configure the filter for.
};

void LogTree::Filter::registerConsoleCommands(CommandParser &parser, const std::string &prefix) {
	parser.registerCommand(prefix + "loglevel", std::make_shared<LogTree::Filter::LogLevelCommand>(*this, *this->logtree));
}

void LogTree::Filter::deregisterConsoleCommands(CommandParser &parser, const std::string &prefix) {
	parser.registerCommand(prefix + "loglevel", nullptr);
}

const char *LogTree::kLogLevelStrings[10] = {
	"SILENT",
	"CRITICAL",
	"ERROR",
	"WARNING",
	"NOTICE",
	"INFO",
	"DIAGNOSTIC",
	"TRACE",
	"ALL",
	"INHERIT"
};

LogRepeatSuppressor::LogRepeatSuppressor(LogTree &tree, uint64_t timeout) :
tree(tree), kTimeout(timeout) {
	this->mutex = xSemaphoreCreateRecursiveMutex();
	configASSERT(this->mutex);
};

LogRepeatSuppressor::~LogRepeatSuppressor() {
	vSemaphoreDelete(this->mutex);
};

bool LogRepeatSuppressor::logUnique(const std::string &message, enum LogTree::LogLevel level) {
	xSemaphoreTakeRecursive(this->mutex, portMAX_DELAY);
	uint64_t now = get_tick64();

	this->clean();

	bool result = true;
	if (this->lastlog.count(message)) {
		result = false;
	} else {
		this->lastlog[message] = now;
	}

	xSemaphoreGiveRecursive(this->mutex);

	if (result) this->tree.log(message, level);

	return result;
}

void LogRepeatSuppressor::clean() {
	xSemaphoreTakeRecursive(this->mutex, portMAX_DELAY);
	uint64_t now = get_tick64();

	for (auto it = this->lastlog.begin(), eit = this->lastlog.end(); it != eit; /* below */) {
		if (it->second + this->kTimeout < now) {
			it = this->lastlog.erase(it);
		} else {
			++it;
		}
	}

	xSemaphoreGiveRecursive(this->mutex);
}
