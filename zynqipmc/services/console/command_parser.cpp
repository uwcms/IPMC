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

#include "FreeRTOS.h"
#include "task.h"
#include <IPMC.h>
#include <stdlib.h>
#include <errno.h>
#include <algorithm>
#include <libs/printf.h>
#include <libs/threading.h>
#include <services/console/command_parser.h>
#include <services/console/consolesvc.h>

/// The default 'help' console command.
class CommandParser::HelpCommand : public CommandParser::Command {
public:
	HelpCommand(CommandParser &parser) : parser(parser) { };

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		std::string out;
		std::string command;

		if (parameters.parseParameters(1, true, &command)) {
			out = parser.getHelpText(command);
			if (out.empty()) {
				out = "Unknown command.  Try help.\n";
			} else if (out.back() != '\n') {
				out += "\n";
			}
		} else {
			std::vector<std::string> commands = parser.listCommands();
			std::sort(commands.begin(), commands.end());
			for (auto it = commands.begin(), eit = commands.end(); it != eit; ++it) {
				if (it->substr(0,5) != "ANSI_") {
					out += *it + "\n";
				}
			}
		}
		console->write(out);
	}

	virtual std::string getHelpText(const std::string &command) const {
		return stdsprintf("%s [command]\n\nGet help on a given command, or get a list of implemented commands.\n", command.c_str());
	}

	virtual std::vector<std::string> complete(const CommandParser::CommandParameters &parameters) const {
		std::string command;
		if (!parameters.parseParameters(1, true, &command) || parameters.cursor_parameter != 1)
			return std::vector<std::string>(); // We're not completing a command.
		return this->parser.listCommands(); // We'll just pass this along.  The outer handler will do the sorting.
	};

private:
	const CommandParser &parser;
};

CommandParser::CommandParser(CommandParser *chain)
	: chain(chain) {
	this->mutex = xSemaphoreCreateMutex();
	configASSERT(this->mutex);
	// Register a help command by default.
	this->registerCommand("help", std::make_shared<CommandParser::HelpCommand>(*this));
}

CommandParser::~CommandParser() {
	vSemaphoreDelete(this->mutex);
}

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
		} else {
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

bool CommandParser::parse(std::shared_ptr<ConsoleSvc> console, const std::string &commandline, std::string::size_type cursor) {
	std::vector<std::string>::size_type cursor_param = 0;
	std::string::size_type cursor_char = cursor;
	std::vector<std::string> command = CommandParser::tokenize(commandline, &cursor_param, &cursor_char);
	if (command.size() < 1)
		return true; // We didn't fail, there was just nothing to do.

	std::shared_ptr<Command> handler = this->getCommand(command[0]);
	if (!handler)
		return false; // Unknown command.

	handler->execute(console, CommandParameters(command, cursor_param, cursor_char));
	return true;
}

std::shared_ptr<CommandParser::Command> CommandParser::getCommand(const std::string &command) const {
	std::shared_ptr<Command> handler = nullptr;
	MutexGuard<false> lock(this->mutex, true);
	if (this->commandset.count(command))
		handler = this->commandset.at(command);

	if (!handler && this->chain)
		handler = this->chain->getCommand(command);

	return handler;
}

void CommandParser::registerCommand(const std::string &token, std::shared_ptr<Command> handler) {
	MutexGuard<false> lock(this->mutex, true);
	if (!handler) {
		this->commandset.erase(token);
	} else {
		this->commandset[token] = handler;
	}
}

std::vector<std::string> CommandParser::listCommands(bool native_only) const {
	MutexGuard<false> lock(this->mutex, true);
	std::vector<std::string> commands;

	commands.reserve(this->commandset.size());
	for (auto it = this->commandset.begin(), eit = this->commandset.end(); it != eit; ++it)
		commands.push_back(it->first);

	if (!native_only && this->chain) {
		std::vector<std::string> chained_commands = this->chain->listCommands(false);
		commands.insert(commands.end(),chained_commands.begin(),chained_commands.end());
	}

	return std::move(commands);
}

std::string CommandParser::getHelpText(const std::string &command) const {
	std::shared_ptr<Command> handler = this->getCommand(command);
	if (handler) {
		return handler->getHelpText(command);
	} else {
		return "";
	}
}

static inline std::string::size_type first_character_difference(const std::string &a, const std::string &b) {
	std::string::size_type i = 0;
	while (i < a.size() && i < b.size() && a[i] == b[i])
		++i;

	return i;
}

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
		} else if (diffchar < maxmatchlen) {
			maxmatchlen = diffchar;
			this->common_prefix = it->substr(0, diffchar);
		}
		++it;
	}
}

CommandParser::CompletionResult CommandParser::complete(const std::string &commandline, std::string::size_type cursor) const {
	std::vector<std::string>::size_type cursor_param = 0;
	std::string::size_type cursor_char = cursor;
	std::vector<std::string> command = CommandParser::tokenize(commandline, &cursor_param, &cursor_char);
	if (command.size() < 1)
		return CompletionResult("", this->listCommands());

	if (cursor_char == std::string::npos)
		return CompletionResult(); // Not on a completable location.

	std::shared_ptr<Command> handler = this->getCommand(command[0]);
	if (cursor_param == 0) {
		return CompletionResult(command[cursor_param].substr(0, cursor_char), this->listCommands());
	} else if (handler) {
		// Complete the command.
		std::vector<std::string> options = handler->complete(CommandParameters(command, cursor_param, cursor_char));
		return CompletionResult(command[cursor_param].substr(0, cursor_char), options);
	} else {
		return CompletionResult(); // Completing a parameter for an unknown command.
	}
}

bool CommandParser::CommandParameters::parse_one(const std::string &arg, xint32_t *x32val) {
	char *endptr = nullptr;
	safe_init_static_mutex(stdlib_mutex, false);
	MutexGuard<false> lock(stdlib_mutex, true);
	errno = 0;
    *x32val = strtoul(arg.c_str(), &endptr, 16);
	if (errno)
		endptr = nullptr;
    return (endptr && *endptr == '\0');
}

bool CommandParser::CommandParameters::parse_one(const std::string &arg, xint16_t *x16val) {
	xint32_t x32val;
	if (!CommandParameters::parse_one(arg, &x32val))
		return false;
	*x16val = x32val;
	return (x32val <= UINT16_MAX);
}

bool CommandParser::CommandParameters::parse_one(const std::string &arg, xint8_t *x8val) {
	xint32_t x32val;
	if (!CommandParameters::parse_one(arg, &x32val))
		return false;
	*x8val = x32val;
	return (x32val <= UINT8_MAX);
}

bool CommandParser::CommandParameters::parse_one(const std::string &arg, uint32_t *u32val) {
	char *endptr = nullptr;
	safe_init_static_mutex(stdlib_mutex, false);
	MutexGuard<false> lock(stdlib_mutex, true);
	errno = 0;
    *u32val = strtoul(arg.c_str(), &endptr, 0);
	if (errno)
		endptr = nullptr;
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
	char *endptr = nullptr;
	safe_init_static_mutex(stdlib_mutex, false);
	MutexGuard<false> lock(stdlib_mutex, true);
	errno = 0;
	*lintval = strtol(arg.c_str(), &endptr, 0);
	if (errno)
		endptr = nullptr;
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

bool CommandParser::CommandParameters::parse_one(const std::string &arg, unsigned int *intval) {
	// On this architecture, long unsigned int == unsigned int
	long unsigned int luint;
	if (!CommandParameters::parse_one(arg, &luint))
		return false;
	*intval = luint;
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
	char *endptr = nullptr;
	safe_init_static_mutex(stdlib_mutex, false);
	MutexGuard<false> lock(stdlib_mutex, true);
	errno = 0;
	*dblval = strtod(arg.c_str(), &endptr);
	if (errno)
		endptr = nullptr;

	return (endptr && *endptr == '\0');
}

bool CommandParser::CommandParameters::parse_one(const std::string &arg, std::string *strval) {
	*strval = arg;
	return true;
}
