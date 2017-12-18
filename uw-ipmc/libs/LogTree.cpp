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
LogTree::LogTree(const std::string subtree_label, LogTree *parent)
	: label(subtree_label), path(parent->path + "." + subtree_label), parent(parent) {
	this->mutex = xSemaphoreCreateMutex();
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
	if (parent) {
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
		this->children.insert(std::make_pair(label, new LogTree(label, this)));
	LogTree *ret = this->children.at(label);
	xSemaphoreGiveRecursive(this->mutex);
	return *ret;
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
			it->first->handler(this, message, level);
	xSemaphoreGiveRecursive(this->mutex);
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
void LogTree::Filter::reconfigure(LogTree *logtree, enum LogLevel level) {
	LogTree *root = logtree;
	while (root->parent)
		root = root->parent;
	configASSERT(root == this->logtree);
	logtree->filter_subscribe(this, level, false);
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
enum LogTree::LogLevel LogTree::Filter::get_configuration(LogTree *logtree) {
	LogTree *root = logtree;
	while (root->parent)
		root = root->parent;
	configASSERT(root == this->logtree);

	xSemaphoreTakeRecursive(logtree->mutex, portMAX_DELAY);
	configASSERT(logtree->filters.count(this));
	enum LogLevel level = logtree->filters.at(this).level;
	if (logtree->filters.at(this).inheriting)
		level = LOG_INHERIT;
	xSemaphoreGiveRecursive(logtree->mutex);
	return level;
}

/**
 * Instantiate a Filter and prepare the initial subscription.
 *
 * @param logtree  The logtree this filter is to be associated with.
 * @param handler  The handler to run when messages are received.
 * @param level    The default level for this filter's subscription to its LogTree.
 */
LogTree::Filter::Filter(LogTree *logtree, handler_t handler, enum LogLevel level)
	: handler(handler) {

	LogTree *root = logtree;
	while (root->parent)
		root = root->parent;
	this->logtree = root;
	if (root != logtree)
		root->filter_subscribe(this, LOG_SILENT, false); // Subscribe to the full tree itself, first.
	logtree->filter_subscribe(this, level, false);
}

LogTree::Filter::~Filter() {
	logtree->filter_unsubscribe(this);
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
