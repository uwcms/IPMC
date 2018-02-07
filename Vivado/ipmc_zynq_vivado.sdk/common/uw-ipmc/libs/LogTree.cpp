/*
 * Logger.cpp
 *
 *  Created on: Dec 13, 2017
 *      Author: jtikalsky
 */

#include "FreeRTOS.h"
#include "semphr.h"
#include <IPMC.h>
#include <libs/LogTree.h>
#include <libs/CommandParser.h>
#include <deque>
#include <algorithm>

/**
 * Instantiate a new LogTree root node.
 *
 * \note You should call this only for the root node.  For all other nodes,
 *       you should call parent["subnode_label"] to auto-instantiate a node.
 *
 * @param root_label The label for the root node of this LogTree
 */
LogTree::LogTree(const std::string root_label)
	: label(root_label), path(root_label), parent(NULL) {
	this->mutex = xSemaphoreCreateRecursiveMutex();
	configASSERT(this->mutex);
}

/**
 * Instantiate a new LogTree child node.
 *
 * @param subtree_label  The label for this LogTree node (not including parent path)
 * @param parent         The parent of this LogTree
 */
LogTree::LogTree(const std::string subtree_label, LogTree &parent)
	: label(subtree_label), path(parent.path + "." + subtree_label), parent(&parent) {
	this->mutex = xSemaphoreCreateRecursiveMutex();
	configASSERT(this->mutex);

	xSemaphoreTakeRecursive(this->parent->mutex, portMAX_DELAY);
	this->filters = this->parent->filters; // Take a copy of the parent's filters.
	xSemaphoreGiveRecursive(this->parent->mutex);
	for (auto it = this->filters.begin(), eit = this->filters.end(); it != eit; ++it)
		it->second.inheriting = true; // But all our configurations are inherited.
}

LogTree::~LogTree() {
	xSemaphoreTakeRecursive(this->mutex, portMAX_DELAY);
	configASSERT(this->children.empty());
	configASSERT(this->filters.empty());
	if (this->parent) {
		xSemaphoreTakeRecursive(this->parent->mutex, portMAX_DELAY);
		this->parent->children.erase(this->label);
		xSemaphoreGiveRecursive(this->parent->mutex);
	}
	vSemaphoreDelete(this->mutex);
}

/**
 * Subscribe a filter to this LogTree at a given level.
 *
 * @param filter  The filter for which the subscription should be modified
 * @param level   The new level of the subscription, or LOG_INHERIT to (re)enable inheritance
 * @param inheritance_update  true if this should be treated as an inheritance update
 */
void LogTree::filter_subscribe(Filter *filter, enum LogLevel level, bool inheritance_update) {
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
		}
		else {
			xSemaphoreTakeRecursive(this->parent->mutex, portMAX_DELAY);
			configASSERT(this->parent->filters.count(filter));
			// Inherit our parent's level and continue inheritance in the future.
			assoc.level = this->parent->filters.at(filter).level;
			assoc.inheriting = true;
			xSemaphoreGiveRecursive(this->parent->mutex);
		}
	}

	// Update myself.
	if (this->filters.count(filter))
		this->filters.at(filter) = assoc;
	else
		this->filters.insert(std::make_pair(filter, assoc));

	/* Update inheriting children.
	 *
	 * This won't cause mutex troubles, as we never pass along our parameter AS LOG_INHERIT, so they needn't check their parent.
	 */
	for (auto it = this->children.begin(), eit = this->children.end(); it != eit; ++it)
		it->second->filter_subscribe(filter, assoc.level, true);

	xSemaphoreGiveRecursive(this->mutex);
}

/**
 * Unsubscribe a filter from this LogTree.
 *
 * @param filter The filter that should be unsubscribed.
 */
void LogTree::filter_unsubscribe(Filter *filter) {
	/* There's a deadlock condition here, since this takes the same locks, in
	 * the opposite order (when recursion is factored) as filter_subscribe when
	 * filter_subscribe, however as this is only ever run during the filter
	 * destructor, and you obviously are not subscribing through a filter object
	 * that you are actively deleting, this case is not relevant.
	 *
	 * This could be avoided by way of a tree-global lock, which must be taken
	 * (with no local locks held), before any tree spanning functions.  This is
	 * not worth the time, given the above reasoning.
	 */
	xSemaphoreTakeRecursive(this->mutex, portMAX_DELAY);
	this->filters.erase(filter);
	for (auto it = this->children.begin(), eit = this->children.end(); it != eit; ++it)
		it->second->filter_unsubscribe(filter);
	xSemaphoreGiveRecursive(this->mutex);
}

/**
 * This function will retrieve a pointer to the child LogTree with the
 * appropriate label, and create it if necessary.
 *
 * @param label
 * @return The requested child LogTree
 */
LogTree& LogTree::operator[](const std::string &label) {
	xSemaphoreTakeRecursive(this->mutex, portMAX_DELAY);
	if (!this->children.count(label))
		this->children.insert(std::make_pair(label, new LogTree(label, *this)));
	LogTree *ret = this->children.at(label);
	xSemaphoreGiveRecursive(this->mutex);
	return *ret;
}

/**
 * Return the number of children (i.e. 0 or 1) with a given label.
 * @param label The label
 * @return 0 if no children, else 1.
 */
LogTree::size_type LogTree::count(const std::string &label) {
	xSemaphoreTakeRecursive(this->mutex, portMAX_DELAY);
	size_type count = this->children.count(label);
	xSemaphoreGiveRecursive(this->mutex);
	return count;
}

/**
 * Return a list of this node's children.
 * @return A list of this node's children
 */
std::vector<std::string> LogTree::list_children() {
	xSemaphoreTakeRecursive(this->mutex, portMAX_DELAY);

	std::vector<std::string> children;
	children.reserve(this->children.size());
	for (auto it = this->children.begin(), eit = this->children.end(); it != eit; ++it)
		children.push_back(it->first);

	xSemaphoreGiveRecursive(this->mutex);
	return std::move(children);
}

/**
 * Log a message to the given LogTree node, and dispatch it to all relevant
 * handlers.
 *
 * @param message The message to log
 * @param level   The LogLevel for this message
 *
 * \see LogLevel
 */
void LogTree::log(const std::string &message, enum LogLevel level) {
	configASSERT(level != LOG_SILENT);
	xSemaphoreTakeRecursive(this->mutex, portMAX_DELAY);
	for (auto it = this->filters.begin(), eit = this->filters.end(); it != eit; ++it)
		if (it->second.level >= level && it->first->handler)
			it->first->handler(*this, message, level);
	xSemaphoreGiveRecursive(this->mutex);
}

namespace {
/// A "log" console command.
class ConsoleCommand_log : public CommandParser::Command {
public:
	LogTree &logtree; ///< The logtree to log to.

	/// Instantiate
	ConsoleCommand_log(LogTree &logtree) : logtree(logtree) { };

	virtual std::string get_helptext(const std::string &command) const {
		return stdsprintf(
				"%s LOGLEVEL \"text\" [...]\n"
				"\n"
				"LOGLEVEL is any prefix of:\n"
				"  CRITICAL\n"
				"  ERROR\n"
				"  WARNING\n"
				"  NOTICE\n"
				"  INFO\n"
				"  DIAGNOSTIC\n"
				"  TRACE\n", command.c_str());
	}

	virtual void execute(std::function<void(std::string)> print, const CommandParser::CommandParameters &parameters) {
		std::string out;
		std::string levelstr;
		if (!parameters.parse_parameters(1, false, &levelstr)) {
			print("Invalid parameters. See help.\n");
			return;
		}
		LogTree::LogLevel level = LogTree::LOG_SILENT;
		if (levelstr == std::string("CRITICAL").substr(0,levelstr.size()))
			level = LogTree::LOG_CRITICAL;
		else if (levelstr == std::string("ERROR").substr(0,levelstr.size()))
			level = LogTree::LOG_ERROR;
		else if (levelstr == std::string("WARNING").substr(0,levelstr.size()))
			level = LogTree::LOG_WARNING;
		else if (levelstr == std::string("NOTICE").substr(0,levelstr.size()))
			level = LogTree::LOG_NOTICE;
		else if (levelstr == std::string("INFO").substr(0,levelstr.size()))
			level = LogTree::LOG_INFO;
		else if (levelstr == std::string("DIAGNOSTIC").substr(0,levelstr.size()))
			level = LogTree::LOG_DIAGNOSTIC;
		else if (levelstr == std::string("TRACE").substr(0,levelstr.size()))
			level = LogTree::LOG_TRACE;

		if (level == LogTree::LOG_SILENT) {
			print("Unknown loglevel.\n");
			return;
		}

		for (CommandParser::CommandParameters::size_type i = 2; i < parameters.nargs(); ++i) {
			std::string arg;
			parameters.parse_parameters(i, false, &arg);
			if (out.size())
				out += " ";
			out += arg;
		}
		this->logtree.log(out, level);
	}
};
} // anonymous namespace

/**
 * Register console commands related to this logtree.
 * @param parser The command parser to register to.
 * @param prefix A prefix for the registered commands.
 */
void LogTree::register_console_commands(CommandParser &parser, const std::string &prefix) {
	parser.register_command(prefix + "log", std::make_shared<ConsoleCommand_log>(*this));
}

/**
 * Unregister console commands related to this logtree.
 * @param parser The command parser to unregister from.
 * @param prefix A prefix for the registered commands.
 */
void LogTree::deregister_console_commands(CommandParser &parser, const std::string &prefix) {
	parser.register_command(prefix + "log", NULL);
}

/**
 * Reconfigure this Filter's loglevel for a LogTree subtree.
 *
 * \note The LogTree node to reconfigure must be in the same tree as this Filter
 *       was originally created in.
 *
 * @param logtree The LogTree subtree to reconfigure
 * @param level   The new loglevel, or LOG_INHERIT to (re)enable inheritance.
 */
void LogTree::Filter::reconfigure(LogTree &logtree, enum LogLevel level) {
	LogTree *root = &logtree;
	while (root->parent)
		root = root->parent;
	configASSERT(root == this->logtree);
	logtree.filter_subscribe(this, level, false);
}

/**
 * Retrieve this Filter's loglevel for a LogTree subtree.
 *
 * \note The LogTree node to reconfigure must be in the same tree as this Filter
 *       was originally created in.
 *
 * @param logtree The LogTree subtree to query the configuration for
 * @return        The configured loglevel, or LOG_INHERIT if inheriting
 */
enum LogTree::LogLevel LogTree::Filter::get_configuration(LogTree &logtree) {
	LogTree *root = &logtree;
	while (root->parent)
		root = root->parent;
	configASSERT(root == this->logtree);

	xSemaphoreTakeRecursive(logtree.mutex, portMAX_DELAY);
	configASSERT(logtree.filters.count(this));
	enum LogLevel level = logtree.filters.at(this).level;
	if (logtree.filters.at(this).inheriting)
		level = LOG_INHERIT;
	xSemaphoreGiveRecursive(logtree.mutex);
	return level;
}

/**
 * Instantiate a Filter and prepare the initial subscription.
 *
 * @param logtree  The logtree this filter is to be associated with.
 * @param handler  The handler to run when messages are received.
 * @param level    The default level for this filter's subscription to its LogTree.
 */
LogTree::Filter::Filter(LogTree &logtree, handler_t handler, enum LogLevel level)
	: handler(handler) {

	LogTree *root = &logtree;
	while (root->parent)
		root = root->parent;
	this->logtree = root;
	if (root != &logtree)
		root->filter_subscribe(this, LOG_SILENT, false); // Subscribe to the full tree itself, first.
	logtree.filter_subscribe(this, level, false);
}

LogTree::Filter::~Filter() {
	this->logtree->filter_unsubscribe(this);
}

namespace {
/// A "loglevel" console command.
class ConsoleCommand_loglevel : public CommandParser::Command {
public:
	LogTree::Filter &filter; ///< The filter to configure.
	LogTree &root; ///< The root logtree to configure the filter for.

	/// Instantiate
	ConsoleCommand_loglevel(LogTree::Filter &filter, LogTree &root) : filter(filter), root(root) { };

	virtual std::string get_helptext(const std::string &command) const {
		return stdsprintf(
				"%s logtree [LOGLEVEL]\n"
				"\n"
				"With a loglevel parameter, change the current loglevel of the target.\n"
				"Without a loglevel parameter, print the current loglevel of the target.\n"
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
				"  PARENT (restore inheritance)\n", command.c_str());
	}

	virtual void execute(std::function<void(std::string)> print, const CommandParser::CommandParameters &parameters) {
		std::string out;
		std::string facilitystr;
		std::string levelstr;

		if (!parameters.parse_parameters(1, true, &facilitystr)) {
			print("Invalid parameters. See help.\n");
			return;
		}
		parameters.parse_parameters(2, true, &levelstr); // Optionally parse levelstr

		LogTree *facility = &this->root;
		if (facilitystr == "*") {
			// MASS Listings!
			std::map<std::string, LogTree*> facilities;
			std::deque<LogTree*> open_nodes;
			open_nodes.push_back(&this->root);
			while (!open_nodes.empty()) {
				facilities.emplace(open_nodes.front()->path, open_nodes.front());
				std::vector<std::string> children = open_nodes.front()->list_children();
				for (auto it = children.begin(), eit = children.end(); it != eit; ++it)
					open_nodes.push_back(&((*open_nodes.front())[*it]));
				open_nodes.pop_front();
			}
			for (auto it = facilities.begin(), eit = facilities.end(); it != eit; ++it) {
				LogTree::LogLevel curlevel = this->filter.get_configuration(*it->second);
				configASSERT(curlevel <= LogTree::LOG_INHERIT);
				print(stdsprintf("%-10s %s\n", LogTree::LogLevel_strings[curlevel], it->second->path.c_str()));
			}
			return;
		}
		else if (facilitystr != this->root.path) {
			if (facilitystr.substr(0, this->root.path.size()+1) != this->root.path + ".") {
				print("Unknown log facility.\n");
				return;
			}
			facilitystr = facilitystr.substr(this->root.path.size()+1);
			while (facilitystr.size()) {
				std::string::size_type nextdot = facilitystr.find_first_of(".");
				std::string part = facilitystr.substr(0, nextdot);

				if (part == "*" && nextdot == std::string::npos && levelstr.empty()) {
					// Listings!
					std::vector<std::string> children = facility->list_children();
					std::sort(children.begin(), children.end());
					for (auto it = children.begin(), eit = children.end(); it != eit; ++it) {
						LogTree::LogLevel curlevel = this->filter.get_configuration((*facility)[*it]);
						configASSERT(curlevel <= LogTree::LOG_INHERIT);
						print(stdsprintf("%-10s %s\n", LogTree::LogLevel_strings[curlevel], (*facility)[*it].path.c_str()));
					}
					return;
				}
				else if (!facility->count(part)) {
					print("Unknown log facility.\n");
					return;
				}
				facility = &((*facility)[part]);

				if (nextdot == std::string::npos)
					break; // No further dots after the one we just processed.
				facilitystr = facilitystr.substr(nextdot+1);
			}
		}

		if (levelstr.empty()) {
			LogTree::LogLevel curlevel = this->filter.get_configuration(*facility);
			configASSERT(curlevel <= LogTree::LOG_INHERIT);
			print(std::string(LogTree::LogLevel_strings[curlevel]) + "\n");
			return;
		}

		LogTree::LogLevel level = LogTree::LOG_SILENT;
		if (levelstr == std::string("SILENT").substr(0,levelstr.size()))
			level = LogTree::LOG_SILENT;
		else if (levelstr == std::string("CRITICAL").substr(0,levelstr.size()))
			level = LogTree::LOG_CRITICAL;
		else if (levelstr == std::string("ERROR").substr(0,levelstr.size()))
			level = LogTree::LOG_ERROR;
		else if (levelstr == std::string("WARNING").substr(0,levelstr.size()))
			level = LogTree::LOG_WARNING;
		else if (levelstr == std::string("NOTICE").substr(0,levelstr.size()))
			level = LogTree::LOG_NOTICE;
		else if (levelstr == std::string("INFO").substr(0,levelstr.size()))
			level = LogTree::LOG_INFO;
		else if (levelstr == std::string("DIAGNOSTIC").substr(0,levelstr.size()))
			level = LogTree::LOG_DIAGNOSTIC;
		else if (levelstr == std::string("TRACE").substr(0,levelstr.size()))
			level = LogTree::LOG_TRACE;
		else if (levelstr == std::string("ALL").substr(0,levelstr.size()))
			level = LogTree::LOG_ALL;
		else if (levelstr == std::string("PARENT").substr(0,levelstr.size()))
			level = LogTree::LOG_INHERIT;
		else {
			print("Unknown loglevel. See help.\n");
			return;
		}

		this->filter.reconfigure(*facility, level);
	}

	/**
	 * Prefix a vector with a string, as a helper.
	 *
	 * @param prefix The prefix
	 * @param vec The vector
	 * @return The vector with all items prefixed by the prefix.
	 */
	static std::vector<std::string> prefix_vector(const std::string &prefix, const std::vector<std::string> &vec) {
		std::vector<std::string> ret;
		for (auto it = vec.begin(), eit = vec.end(); it != eit; ++it)
			ret.push_back(prefix + *it);
		return std::move(ret);
	}

	virtual std::vector<std::string> complete(const CommandParser::CommandParameters &parameters) const {
		if (parameters.cursor_parameter != 1)
			return std::vector<std::string>(); // Can't help you.

		std::string facilitystr = parameters.parameters[1].substr(0, parameters.cursor_char);
		LogTree *facility = &this->root;
		if (facilitystr.size() <= facility->path.size() || facilitystr.substr(0, facility->path.size()+1) != facility->path+".")
			return std::vector<std::string>{facility->path}; // The only valid value would be the root facility name.

		facilitystr.erase(0, facility->path.size()+1); // Remove the root facility.

		std::string::size_type next_dot;
		while ( (next_dot = facilitystr.find_first_of(".")) != std::string::npos ) {
			std::string next_hop = facilitystr.substr(0, next_dot);

			if (facility->count(next_hop))
				facility = &((*facility)[next_hop]);
			else
				break;

			facilitystr.erase(0, next_dot+1);
		}

		// And in case we have a fully typed out facility...
		if (facility->count(facilitystr))
			facility = &((*facility)[facilitystr]);

		/* Ok we should now be at the deepest facility that we know of.
		 *
		 * We'll return its children and let the completion filter sort out the rest.
		 */
		return ConsoleCommand_loglevel::prefix_vector(facility->path+".", facility->list_children());
	};
};
} // anonymous namespace

/**
 * Register console commands related to this filter.
 * @param parser The command parser to register to.
 * @param prefix A prefix for the registered commands.
 */
void LogTree::Filter::register_console_commands(CommandParser &parser, const std::string &prefix) {
	parser.register_command(prefix + "loglevel", std::make_shared<ConsoleCommand_loglevel>(*this, *this->logtree));
}

/**
 * Unregister console commands related to this filter.
 * @param parser The command parser to unregister from.
 * @param prefix A prefix for the registered commands.
 */
void LogTree::Filter::deregister_console_commands(CommandParser &parser, const std::string &prefix) {
	parser.register_command(prefix + "loglevel", NULL);
}

const char *LogTree::LogLevel_strings[10] = {
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
