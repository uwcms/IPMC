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
#include <algorithm>

namespace {
/// The default 'help' console command.
class ConsoleCommand_help : public CommandParser::Command {
public:
	const CommandParser &parser; ///< The parser we're associated with
	/// Construct
	ConsoleCommand_help(CommandParser &parser) : parser(parser) { };

	/// Execute
	virtual void execute(std::function<void(std::string)> print, const CommandParser::CommandParameters &parameters) {
		std::string out;
		std::string command;

		if (parameters.parse_parameters(1, true, &command)) {
			out = parser.get_helptext(command);
			if (out.empty())
				out = "Unknown command.  Try help.\n";
			else if (out.back() != '\n')
				out += "\n";
		}
		else {
			std::vector<std::string> commands = parser.list_commands();
			std::sort(commands.begin(), commands.end());
			for (auto it = commands.begin(), eit = commands.end(); it != eit; ++it)
				if (it->substr(0,5) != "ANSI_") {
					out += *it + "\n";
				}
		}
		print(out);
	}

	virtual std::string get_helptext(const std::string &command) const {
		return stdsprintf("%s [command]\n\nGet help on a given command, or get a list of implemented commands.\n", command.c_str());
	}

	virtual std::vector<std::string> complete(const CommandParser::CommandParameters &parameters) const {
		std::string command;
		if (!parameters.parse_parameters(1, true, &command) || parameters.cursor_parameter != 1)
			return std::vector<std::string>(); // We're not completing a command.
		return this->parser.list_commands(); // We'll just pass this along.  The outer handler will do the sorting.
	};
};

} // anonymous namespace

CommandParser::CommandParser() {
	this->mutex = xSemaphoreCreateMutex();
	configASSERT(this->mutex);
	// Register a help command by default.
	this->register_command("help", std::make_shared<ConsoleCommand_help>(*this));
}

CommandParser::~CommandParser() {
	vSemaphoreDelete(this->mutex);
}

/**
 * Tokenize a command line possibly containing quoted strings.
 * @param commandline The string to tokenize
 * @param[out] cursor_parameter The parameter the cursor is at, or 0 if none.
 * @param[in,out] cursor_char IN: The character the cursor is at.  OUT: The parameter-relative character the cursor is at, or std::string::npos if it is not within a parameter.
 * @return A vector of tokens.
 */
std::vector<std::string> CommandParser::tokenize(const std::string &commandline, std::vector<std::string>::size_type *cursor_parameter, std::string::size_type *cursor_char) {
	const std::string::size_type input_cursor = (cursor_char ? *cursor_char : 0);
	if (cursor_parameter)
		*cursor_parameter = 0;
	if (cursor_char)
		*cursor_char = std::string::npos;

	std::vector<std::string> tokens;
	char inquote = 0;
	char prev_endquote = 0;
	std::string token;
	for (std::string::size_type i = 0; i < commandline.size(); ++i) {
		if (inquote && inquote != commandline[i]) {
			// Continuing quoted token.
			if (input_cursor == i) {
				if (cursor_parameter)
					*cursor_parameter = tokens.size();
				if (cursor_char)
					*cursor_char = token.size();
			}
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
				if (input_cursor == i) {
					if (cursor_parameter)
						*cursor_parameter = tokens.size();
					if (cursor_char)
						*cursor_char = token.size();
				}
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
			if (input_cursor == i) {
				if (cursor_parameter)
					*cursor_parameter = tokens.size();
				if (cursor_char)
					*cursor_char = token.size();
			}
			token += commandline[i];
		}
	}
	if (!token.empty())
		tokens.push_back(token);

	if (input_cursor >= commandline.size()) {
		if (cursor_parameter)
			*cursor_parameter = (tokens.empty() ? 0 : tokens.size()-1);
		if (cursor_char)
			*cursor_char = (tokens.empty() ? 0 : tokens.back().size());
	}
	return tokens;
}

/**
 * Parse a given commandline and execute the associated command.
 *
 * \note Whitespace-only lines are automatically ignored as successful parses.
 *
 * @param print A function for the command to use to return output to the user.
 * @param commandline The command line to be parsed.
 * @param cursor The current input cursor position.
 * @return false if unknown command, else true.
 */
bool CommandParser::parse(std::function<void(std::string)> print, const std::string &commandline, std::string::size_type cursor) {
	std::vector<std::string>::size_type cursor_param = 0;
	std::string::size_type cursor_char = cursor;
	std::vector<std::string> command = CommandParser::tokenize(commandline, &cursor_param, &cursor_char);
	if (command.size() < 1)
		return true; // We didn't fail, there was just nothing to do.

	std::shared_ptr<Command> handler = NULL;
	xSemaphoreTake(this->mutex, portMAX_DELAY);
	if (this->commandset.count(command[0]))
		handler = this->commandset.at(command[0]);
	xSemaphoreGive(this->mutex);
	if (!handler)
		return false; // Unknown command.
	handler->execute(print, CommandParameters(command, cursor_param, cursor_char));
	return true;
}

/**
 * Register a command with this helper (or unregister if handler==NULL).
 *
 * @param token The command to register.
 * @param handler The handler for the command.
 */
void CommandParser::register_command(const std::string &token, std::shared_ptr<Command> handler) {
	xSemaphoreTake(this->mutex, portMAX_DELAY);
	if (!handler)
		this->commandset.erase(token);
	else
		this->commandset[token] = handler;
	xSemaphoreGive(this->mutex);
}


/**
 * List installed commands.
 *
 * @return All installed commands.
 */
std::vector<std::string> CommandParser::list_commands() const {
	xSemaphoreTake(this->mutex, portMAX_DELAY);
	std::vector<std::string> commands;
	commands.reserve(this->commandset.size());
	for (auto it = this->commandset.begin(), eit = this->commandset.end(); it != eit; ++it)
		commands.push_back(it->first);
	xSemaphoreGive(this->mutex);
	return std::move(commands);
}

/**
 * Retrieve the help text for a specific command
 *
 * @param command The command to return help text for.
 * @return The help text or an empty string if empty or none present.
 */
std::string CommandParser::get_helptext(const std::string &command) const {
	xSemaphoreTake(this->mutex, portMAX_DELAY);
	std::string result;
	if (this->commandset.count(command))
		result = this->commandset.at(command)->get_helptext(command);
	xSemaphoreGive(this->mutex);
	return result;
}

static inline std::string::size_type first_character_difference(const std::string &a, const std::string &b) {
	std::string::size_type i = 0;
	while (i < a.size() && i < b.size() && a[i] == b[i])
		++i;
	return i;
}

/**
 * Instantiate a CompletionResult and handle all option processing.
 *
 * @param prefix The provided prefix to complete
 * @param completions The completion options.
 */
CommandParser::CompletionResult::CompletionResult(std::string prefix, std::vector<std::string> completions)
	: cursor(prefix.size()), completions(completions) {
	std::sort(this->completions.begin(), this->completions.end());
	this->common_prefix = prefix;
	std::string::size_type maxmatchlen = std::string::npos;
	for (auto it = this->completions.begin(); it != this->completions.end(); ) {
		if (it->substr(0, prefix.size()) != prefix) {
			// Doesn't match our provided prefix.
			it = this->completions.erase(it);
			continue;
		}
		std::string::size_type diffchar = first_character_difference(*it, this->common_prefix);
		if (maxmatchlen == std::string::npos) {
			// First matching value.
			maxmatchlen = it->size();
			this->common_prefix = *it;
		}
		else if (diffchar < maxmatchlen) {
			maxmatchlen = diffchar;
			this->common_prefix = it->substr(0, diffchar);
		}
		++it;
	}
}

/**
 * Complete a provided command line, from the given cursor position.
 *
 * @param commandline The command line to complete
 * @param cursor The cursor position to complete from
 * @return A CompletionResults object.
 */
CommandParser::CompletionResult CommandParser::complete(const std::string &commandline, std::string::size_type cursor) const {
	std::vector<std::string>::size_type cursor_param = 0;
	std::string::size_type cursor_char = cursor;
	std::vector<std::string> command = CommandParser::tokenize(commandline, &cursor_param, &cursor_char);
	if (command.size() < 1)
		return CompletionResult("", this->list_commands());

	if (cursor_char == std::string::npos)
		return CompletionResult(); // Not on a completable location.

	std::shared_ptr<Command> handler = NULL;
	xSemaphoreTake(this->mutex, portMAX_DELAY);
	if (this->commandset.count(command[0]))
		handler = this->commandset.at(command[0]);
	xSemaphoreGive(this->mutex);
	if (cursor_param == 0) {
		return CompletionResult(command[cursor_param].substr(0, cursor_char), this->list_commands());
	}
	else if (handler) {
		// Complete the command.
		std::vector<std::string> options = handler->complete(CommandParameters(command, cursor_param, cursor_char));
		return CompletionResult(command[cursor_param].substr(0, cursor_char), options);
	}
	else {
		return CompletionResult(); // Completing a parameter for an unknown command.
	}
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
