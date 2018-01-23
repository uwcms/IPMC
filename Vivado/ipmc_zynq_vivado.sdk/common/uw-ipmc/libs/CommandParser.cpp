/*
 * CommandParser.cpp
 *
 *  Created on: Jan 12, 2018
 *      Author: jtikalsky
 */

#include <libs/CommandParser.h>
#include "FreeRTOS.h"
#include "task.h"
#include <IPMC.h>
#include <stdlib.h>
#include <errno.h>

CommandParser::CommandParser() {
	this->mutex = xSemaphoreCreateMutex();
	configASSERT(this->mutex);
}

CommandParser::~CommandParser() {
	vSemaphoreDelete(this->mutex);
}

/**
 * Tokenize a command line possibly containing quoted strings.
 * @param commandline The string to tokenize
 * @return A vector of tokens.
 */
std::vector<std::string> CommandParser::tokenize(const std::string &commandline) {
	std::vector<std::string> tokens;
	char inquote = 0;
	char prev_endquote = 0;
	std::string token;
	for (std::string::size_type i = 0; i < commandline.size(); ++i) {
		if (inquote && inquote != commandline[i]) {
			// Continuing quoted token.
			token += commandline[i];
			continue;
		}
		if (inquote && inquote == commandline[i]) {
			// Ending quoted token.
			prev_endquote = inquote;
			inquote = 0;
			continue;
		}
		if (commandline[i] == '"' || commandline[i] == '\'') {
			// Starting new quote.
			if (prev_endquote == commandline[i]) {
				// Reopening the one we just closed, this is an escape.
				token += commandline[i];
			}
			inquote = commandline[i];
			continue;
		}
		// Else, This is not quoted, nor a quote change.
		prev_endquote = 0;
		if (commandline[i] == ' ') {
			if (!token.empty())
				tokens.push_back(token);
			token = "";
		}
		else {
			token += commandline[i];
		}
	}
	if (!token.empty())
		tokens.push_back(token);
	return tokens;
}

/**
 * Parse a given commandline and execute the associated command.
 *
 * \note Whitespace-only lines are automatically ignored as successful parses.
 *
 * @param print A function for the command to use to return output to the user.
 * @param commandline The command line to be parsed.
 * @return false if unknown command, else true.
 */
bool CommandParser::parse(std::function<void(std::string)> print, const std::string &commandline) {
	std::vector<std::string> command = CommandParser::tokenize(commandline);
	if (command.size() < 1)
		return true; // We didn't fail, there was just nothing to do.

	handler_t handler = NULL;
	xSemaphoreTake(this->mutex, portMAX_DELAY);
	if (this->commandset.count(command[0]))
		handler = this->commandset.at(command[0]);
	xSemaphoreGive(this->mutex);
	if (!handler)
		return false; // Unknown command.
	handler(print, CommandParameters(command));
	return true;
}

/**
 * Register a command with this helper (or unregister if handler==NULL).
 *
 * @param command The command to register.
 * @param handler The handler for the command.
 * @param helptext The helptext for the command.
 */
void CommandParser::register_command(const std::string &command, handler_t handler, const std::string &helptext) {
	xSemaphoreTake(this->mutex, portMAX_DELAY);
	if (!handler) {
		this->commandset.erase(command);
		this->commandhelp.erase(command);
	}
	else {
		this->commandset[command] = handler;
		this->commandhelp[command] = helptext;
	}
	xSemaphoreGive(this->mutex);
}

/**
 * Retrieve the help text for a specific command
 *
 * @param command The command to return help text for.
 * @return The help text or an empty string if empty or none present.
 */
std::string CommandParser::get_helptext(const std::string &command) {
	xSemaphoreTake(this->mutex, portMAX_DELAY);
	std::string result;
	if (this->commandhelp.count(command))
		result = this->commandhelp.at(command);
	xSemaphoreGive(this->mutex);
	return result;
}

bool CommandParser::CommandParameters::parse_one(const std::string &arg, uint32_t *u32val) {
	char *endptr = NULL;
	init_stdlib_mutex();
	xSemaphoreTake(stdlib_mutex, portMAX_DELAY);
	errno = 0;
    *u32val = strtoul(arg.c_str(), &endptr, 0);
	if (errno)
		endptr = NULL;
	xSemaphoreGive(stdlib_mutex);
    return (endptr && *endptr == '\0');
}

bool CommandParser::CommandParameters::parse_one(const std::string &arg, uint16_t *u16val) {
	uint32_t u32val;
	if (!CommandParameters::parse_one(arg, &u32val))
		return false;
	*u16val = u32val;
	return (u32val <= UINT16_MAX);
}

bool CommandParser::CommandParameters::parse_one(const std::string &arg, uint8_t *u8val) {
	uint32_t u32val;
	if (!CommandParameters::parse_one(arg, &u32val))
		return false;
	*u8val = u32val;
	return (u32val <= UINT8_MAX);
}

bool CommandParser::CommandParameters::parse_one(const std::string &arg, long int *lintval) {
	char *endptr = NULL;
	init_stdlib_mutex();
	xSemaphoreTake(stdlib_mutex, portMAX_DELAY);
	errno = 0;
	*lintval = strtol(arg.c_str(), &endptr, 0);
	if (errno)
		endptr = NULL;
	xSemaphoreGive(stdlib_mutex);
	return (endptr && *endptr == '\0');
}

bool CommandParser::CommandParameters::parse_one(const std::string &arg, int *intval) {
	// On this architecture, long int == int
	long int lint;
	if (!CommandParameters::parse_one(arg, &lint))
		return false;
	*intval = lint;
	return true;
}

bool CommandParser::CommandParameters::parse_one(const std::string &arg, bool *boolval) {
	static const std::vector<std::string> truevals = {"1","true","True","TRUE","yes","Yes","YES"};
	static const std::vector<std::string> falsevals = {"0","false","False","FALSE","no","No","NO"};
	if (arg.empty())
		return false;
	for (auto it = truevals.begin(), eit = truevals.end(); it != eit; ++it) {
		if (arg == it->substr(0, arg.size())) {
			*boolval = true;
			return true;
		}
	}
	for (auto it = falsevals.begin(), eit = falsevals.end(); it != eit; ++it) {
		if (arg == it->substr(0, arg.size())) {
			*boolval = false;
			return true;
		}
	}
	return false;
}

bool CommandParser::CommandParameters::parse_one(const std::string &arg, double *dblval) {
	char *endptr = NULL;
	init_stdlib_mutex();
	xSemaphoreTake(stdlib_mutex, portMAX_DELAY);
	errno = 0;
	*dblval = strtod(arg.c_str(), &endptr);
	if (errno)
		endptr = NULL;
	xSemaphoreGive(stdlib_mutex);
	return (endptr && *endptr == '\0');
}

bool CommandParser::CommandParameters::parse_one(const std::string &arg, std::string *strval) {
	*strval = arg;
	return true;
}
