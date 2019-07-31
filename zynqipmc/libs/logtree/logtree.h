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

#ifndef SRC_COMMON_ZYNQIPMC_LIBS_LOGTREE_H_
#define SRC_COMMON_ZYNQIPMC_LIBS_LOGTREE_H_

#include <services/console/command_parser.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include <string>
#include <list>
#include <vector>
#include <map>
#include <functional>

/**
 * The LogTree class provides as tree of logging facilities which can be
 * subscribed to in a configurable manner, allowing log levels to be
 * independently dynamically configured for any subtree by any of multiple
 * independent log subscribers.  It is designed to be as efficient as possible
 * for logging at the cost of lower efficiency during reconfiguration.
 */
class LogTree final : public ConsoleCommandSupport {
public:

	/**
	 * Instantiate a new LogTree root node.
	 *
	 * @note You should call this only for the root node.  For all other nodes,
	 *       you should call parent["subnode_label"] to auto-instantiate a node.
	 *
	 * @param root_label The label for the root node of this LogTree.
	 */
	LogTree(const std::string& root_label);
	virtual ~LogTree();

	//! Returns the parent LogTree or nullptr if this is the root.
	inline LogTree* getParent() { return this->parent; };

	//! Returns the LogTree label.
	inline const std::string& getLabel() const { return this->kLabel; };

	//! Returns the LogTree path.
	inline const std::string& getPath() const { return this->kPath; };

	// Not copyable or assignable.
	LogTree(const LogTree&) = delete;
	LogTree& operator= (const LogTree&) = delete;

	//! Log Levels
	enum LogLevel {
		LOG_SILENT     = 0, ///< No messages.  Ever. (filters only)
		LOG_CRITICAL   = 1, ///< Critical errors with significant impact
		LOG_ERROR      = 2, ///< Errors
		LOG_WARNING    = 3, ///< Warnings
		LOG_NOTICE     = 4, ///< Unusual informational messages (unusual but not errors)
		LOG_INFO       = 5, ///< Common informational messages (normal operation)
		LOG_DIAGNOSTIC = 6, ///< Diagnostic messages
		LOG_TRACE      = 7, ///< Detailed trace messages, beyond normal human reality.  May contain binary data.
		LOG_ALL        = 8, ///< All messages (filters only)
		LOG_INHERIT    = 9, ///< Inherit parent loglevel (filters only)
	};

	//! Converts LogLevel to string.
	inline static const char* getLogLevelString(enum LogLevel level) { return kLogLevelStrings[level]; };

	/**
	 * A Filter represents a view of a LogTree, and has an associated handler
	 * that is run when matching messages are logged.
	 */
	class Filter : public ConsoleCommandSupport {
	public:
		/**
		 * A callback representing the handler to run for matching log entries
		 * (i.e. write to console).
		 *
		 * @warning This function MAY NOT delete or create new filters, however
		 *          it MAY change subscription levels and log additional
		 *          messages.  If you are logging messages from your log handler,
		 *          you should beware of infinite recursion and ensure that your
		 *          handler is reentrant.
		 *
		 * @param logtree The LogTree emitting the message.
		 * @param message The log message being emitted.
		 * @param level   The loglevel of the emitted message.
		 */
		typedef std::function<void(LogTree &logtree, const std::string &message, enum LogLevel level)> handler_t;

		handler_t handler; ///< The handler associated with this filter.

		/**
		 * Instantiate a Filter and prepare the initial subscription.
		 *
		 * @param logtree  The logtree this filter is to be associated with.
		 * @param handler  The handler to run when messages are received.
		 * @param level    The default level for this filter's subscription to its LogTree.
		 */
		Filter(LogTree &logtree, handler_t handler, enum LogLevel level);
		virtual ~Filter();

		// Not copyable or assignable.
		Filter(const Filter&) = delete;
		Filter& operator= (const Filter&) = delete;

		/**
		 * Reconfigure this Filter's loglevel for a LogTree subtree.
		 *
		 * @note The LogTree node to reconfigure must be in the same tree as this Filter
		 *       was originally created in.
		 *
		 * @param logtree The LogTree subtree to reconfigure.
		 * @param level   The new loglevel, or LOG_INHERIT to (re)enable inheritance.
		 */
		void reconfigure(LogTree &logtree, enum LogLevel level);

		/**
		 * Retrieve this Filter's loglevel for a LogTree subtree.
		 *
		 * @note The LogTree node to reconfigure must be in the same tree as this Filter
		 *       was originally created in.
		 *
		 * @param logtree The LogTree subtree to query the configuration for.
		 * @return        The configured loglevel, or LOG_INHERIT if inheriting.
		 */
		enum LogLevel getConfiguration(LogTree &logtree);

		// From base class ConsoleCommandSupport:
		virtual void registerConsoleCommands(CommandParser &parser, const std::string &prefix="");
		virtual void deregisterConsoleCommands(CommandParser &parser, const std::string &prefix="");

	private:
		class LogLevelCommand;	///< Console command to change the log level of the filter.

		LogTree *logtree; ///< A reference to the (root) LogTree this Filter is associated with.
	};

	//! Inherit the underlying container's size_type.
	typedef std::map<const std::string, LogTree*>::size_type size_type;

	/**
	 * This function will retrieve a pointer to the child LogTree with the
	 * appropriate label, and create it if necessary.
	 *
	 * @param label Log label.
	 * @return The requested child LogTree.
	 */
	LogTree& operator[](const std::string &label);

	/**
	 * Return the number of children this facility has.
	 * @return The number of child facilities.
	 */
	size_type getChildCount();

	/**
	 * Return the number of children (i.e. 0 or 1) with a given label.
	 * @param label The label.
	 * @return 0 if no children, else 1.
	 */
	size_type getChildCount(const std::string &label);

	//! Return a list of this node's children.
	std::vector<std::string> listChildren();

	/**
	 * Log a message to the given LogTree node, and dispatch it to all relevant
	 * handlers.
	 *
	 * @param message The message to log.
	 * @param level   The LogLevel for this message.
	 * @see LogLevel
	 */
	void log(const std::string &message, enum LogLevel level);

	// From base class ConsoleCommandSupport:
	virtual void registerConsoleCommands(CommandParser &parser, const std::string &prefix="");
	virtual void deregisterConsoleCommands(CommandParser &parser, const std::string &prefix="");

private:
	/**
	 * Instantiate a new LogTree child node.
	 *
	 * @param subtree_label  The label for this LogTree node (not including parent path).
	 * @param parent         The parent of this LogTree.
	 */
	LogTree(const std::string& subtree_label, LogTree &parent);

	LogTree* parent;			///< The parent of this LogTree node.
	const std::string kLabel;	///< The name of this LogTree node.
	const std::string kPath;	///< The full path name of this LogTree node.

	/// An array of string names for the LogLevels enum.
	static const char *kLogLevelStrings[10];

	class LogCommand; ///< Console command to log a message.

	std::map<const std::string, LogTree*> children; ///< All children of this LogTree node.

	/**
	 * Subscribe a filter to this LogTree at a given level.
	 *
	 * @param filter  The filter for which the subscription should be modified.
	 * @param level   The new level of the subscription, or LOG_INHERIT to (re)enable inheritance.
	 * @param inheritance_update  true if this should be treated as an inheritance update.
	 */
	void filterSubscribe(Filter *filter, enum LogLevel level, bool inheritance_update);

	/**
	 * Unsubscribe a filter from this LogTree.
	 *
	 * @param filter The filter that should be unsubscribed.
	 */
	void filterUnsubscribe(Filter *filter);

	/**
	 * This internal bookkeeping class represents filter associations.
	 */
	class FilterAssociation {
	public:
		enum LogLevel level; ///< The level of messages to deliver to this filter
		bool inheriting;     ///< Determines whether this loglevel should be overwritten by inheritance updates.
		/// Convenience constructor
		FilterAssociation(enum LogLevel level, bool inheriting=true) : level(level), inheriting(inheriting) { };
	};
	std::map<Filter*, FilterAssociation> filters; ///< Filters associated with this LogTree node.
	SemaphoreHandle_t mutex; ///< A mutex protecting the filter and child lists.
};

/**
 * A log message repeat checker.  It records log messages last sent within
 * `timeout` and allows you to avoid repeating them unnecessarily.
 */
class LogRepeatSuppressor final {
public:
	/**
	 * Initialize a LogRepeatSuppressor.
	 *
	 * @param tree The LogTree to pass messages to.
	 * @param timeout The minimum time before repeating a message.
	 */
	LogRepeatSuppressor(LogTree &tree, uint64_t timeout = pdMS_TO_TICKS(10000));
	~LogRepeatSuppressor();

	/**
	 * Check if a log message is unique within a period of `this->timeout`, and if
	 * it is, log it.
	 *
	 * @param message The log message to check.
	 * @param level The LogLevel for output if unique.
	 * @return true if the message was logged, else false.
	 */
	bool logUnique(const std::string &message, enum LogTree::LogLevel level);

	//! Clean old log messages from the tracking index.
	void clean();

private:
	SemaphoreHandle_t mutex;					///< A mutex protecting the map.
	LogTree &tree;								///< The LogTree to dispatch unique messages to.
	std::map<std::string, uint64_t> lastlog;	///< A map of recently sent messages.
	const uint64_t kTimeout;					///< The minimum time between successive identical messages.
};

#endif /* SRC_COMMON_ZYNQIPMC_LIBS_LOGTREE_H_ */
