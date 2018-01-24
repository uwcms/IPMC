/*
 * Logger.h
 *
 *  Created on: Dec 13, 2017
 *      Author: jtikalsky
 */

#ifndef SRC_COMMON_UW_IPMC_LIBS_LOGTREE_H_
#define SRC_COMMON_UW_IPMC_LIBS_LOGTREE_H_

#include "FreeRTOS.h"
#include "semphr.h"
#include <string>
#include <list>
#include <vector>
#include <map>
#include <functional>

class CommandParser;

/**
 * The LogTree class provides as tree of logging facilities which can be
 * subscribed to in a configurable manner, allowing log levels to be
 * independently dynamically configured for any subtree by any of multiple
 * independent log subscribers.  It is designed to be as efficient as possible
 * for logging at the cost of lower efficiency during reconfiguration.
 */
class LogTree {
public:
	const std::string label; ///< The name of this LogTree node
	const std::string path;  ///< The full path name of this LogTree node

	LogTree(std::string root_label);
protected:
	LogTree(std::string subtree_label, LogTree &parent);
public:
	virtual ~LogTree();

	// Not copyable or assignable.
	LogTree(const LogTree&) = delete;
	LogTree& operator= (const LogTree&) = delete;

	/// Log Levels
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

	/// An array of string names for the LogLevels enum.
	static const char *LogLevel_strings[10];

	/**
	 * A Filter represents a view of a LogTree, and has an associated handler
	 * that is run when matching messages are logged.
	 */
	class Filter {
	public:
		/**
		 * A callback representing the handler to run for matching log entries
		 * (i.e. write to console).
		 *
		 * \warning This function MAY NOT delete or create new filters, however
		 *          it MAY change subscription levels and log additional
		 *          messages.  If you are logging messages from your log handler,
		 *          you should beware of infinite recursion and ensure that your
		 *          handler is reentrant.
		 *
		 * \param logtree The LogTree emitting the message
		 * \param message The log message being emitted
		 * \param level   The loglevel of the emitted message
		 */
		typedef std::function<void(LogTree &logtree, const std::string &message, enum LogLevel level)> handler_t;

		handler_t handler; ///< The handler associated with this filter.
		Filter(LogTree &logtree, handler_t handler, enum LogLevel level);
		virtual ~Filter();
		void reconfigure(LogTree &logtree, enum LogLevel level);
		enum LogLevel get_configuration(LogTree &logtree);

		void register_console_commands(CommandParser &parser, const std::string &prefix="");
		void deregister_console_commands(CommandParser &parser, const std::string &prefix="");
	protected:
		LogTree *logtree; ///< A reference to the (root) LogTree this Filter is associated with.

		// Not copyable or assignable.
		Filter(const Filter&) = delete;
		Filter& operator= (const Filter&) = delete;
	};
	friend class Filter;

	/// Inherit the underlying container's size_type.
	typedef std::map<const std::string, LogTree*>::size_type size_type;
	LogTree& operator[](const std::string &label);
	size_type count(const std::string &label);
	std::vector<std::string> list_children();

	void log(const std::string &message, enum LogLevel level);
	void register_console_commands(CommandParser &parser, const std::string &prefix="");
	void deregister_console_commands(CommandParser &parser, const std::string &prefix="");

protected:
	LogTree * const parent;                         ///< The parent of this LogTree node.
	std::map<const std::string, LogTree*> children; ///< All children of this LogTree node.

	void filter_subscribe(Filter *filter, enum LogLevel level, bool inheritance_update);
	void filter_unsubscribe(Filter *filter);

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
	std::map<Filter*, FilterAssociation> filters; ///< Filters associated with this LogTree node
	SemaphoreHandle_t mutex; ///< A mutex protecting the filter and child lists.
};

#endif /* SRC_COMMON_UW_IPMC_LIBS_LOGTREE_H_ */
