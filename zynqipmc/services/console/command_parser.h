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

#ifndef SRC_COMMON_ZYNQIPMC_LIBS_COMMANDPARSER_H_
#define SRC_COMMON_ZYNQIPMC_LIBS_COMMANDPARSER_H_

#include "FreeRTOS.h"
#include "semphr.h"
#include "xil_types.h"
#include <string>
#include <vector>
#include <type_traits>
#include <functional>
#include <map>
#include <memory>

class ConsoleSvc;

/**
 * A command line parser class, which handles registration of commands and
 * parsing and dispatch of command lines supplied as strings.
 */
class CommandParser final {
public:
	/**
	 * Initialize a CommandParser
	 *
	 * @param chain The next chained command parser, if applicable.
	 */
	CommandParser(CommandParser *chain = nullptr);
	virtual ~CommandParser();

	/**
	 * A parameter set for a parsed command.
	 */
	class CommandParameters {
	public:
		typedef std::vector<std::string>::size_type size_type; ///< Inherit size_type from our underlying storage.

		/**
		 * Construct an intelligent parameterset from an unparsed parameter list.
		 * @param parameters The list of string parameters.
		 * @param cursor_parameter The parameter under the cursor.
		 * @param cursor_character The parameter-relative cursor position.
		 */
		CommandParameters(const std::vector<std::string> &parameters, size_type cursor_parameter, size_type cursor_character)
			: parameters(parameters), cursor_parameter(cursor_parameter), cursor_char(cursor_character) { };
		virtual ~CommandParameters() { };

		/**
		 * Get the number of parameters supplied.
		 * @return The number of parameters.
		 */
		size_type nargs() const { return this->parameters.size(); };

		/**
		 * Parse the parameters stored in this object into their appropriate types.
		 *
		 * This will automatically perform type conversions on the parsed parameters.
		 *
		 * @param start        The index of the first parameter to parse.
		 * @param total_parse  If true, parse will fail if there are excess parameters.
		 * @param arg1         A pointer to store the first parsed parameter in, or (int*)0 to skip.
		 * @param subseq       A variable length list of pointers to store subsequent parsed parameters in.
		 * @return true if success, else false
		 */
		template <typename T, typename... Args> bool parseParameters(int start, bool total_parse, T arg1, Args... subseq) const {
			if (start + 1 + sizeof...(subseq) > this->parameters.size())
				return false; // We want more than we have.
			if (total_parse && this->parameters.size() < start + 1 + sizeof...(subseq))
				return false; // We have more than we want, and that bothers us.
			if (arg1 && !CommandParameters::parse_one(this->parameters[start], arg1))
				return false; // The current arg failed to parse.
			if (sizeof...(subseq) > 0)
				return this->parseParameters(start+1, total_parse, subseq...);
			else
				return true;
		}

		const std::vector<std::string> parameters; ///< The internal unparsed parameter list.
		const size_type cursor_parameter; ///< The parameter the cursor is in, or str::npos if it is in an inter-parameter space.
		const std::string::size_type cursor_char; ///< The character position within the parameter that the cursor is at.

		/**
		 * A proxy class for a specified integer type, allowing it to be handled
		 * with the base 16 specifier.  You can perform bidirectional assignment
		 * and implicit conversion between T and HexInt<T>
		 */
		template <typename T> class HexInt {
		public:
			T value; ///< The value of this HexInt
			/// Zero-Initialization
			HexInt() : value(0) { };
			/// Implicit conversion to T
			operator T() { return this->value; };
			/// Implicit conversion from T
			HexInt(T value) : value(value) { };
			/// Assignment to/from T
			T operator=(const T &value) { this->value = value; return *this; };
		};
		typedef HexInt<uint32_t> xint32_t; ///< Base16 converted uint32_t
		typedef HexInt<uint16_t> xint16_t; ///< Base16 converted uint16_t
		typedef HexInt<uint8_t>  xint8_t;  ///< Base16 converted uint8_t

		/// Parse one argument.
		static bool parse_one(const std::string &arg, xint32_t *x32val);
		/// overload
		static bool parse_one(const std::string &arg, xint16_t *x16val);
		/// overload
		static bool parse_one(const std::string &arg, xint8_t *x8val);
		/// overload
		static bool parse_one(const std::string &arg, uint32_t *u32val);
		/// overload
		static bool parse_one(const std::string &arg, uint16_t *u16val);
		/// overload
		static bool parse_one(const std::string &arg, uint8_t *u8val);
		/// overload
		static bool parse_one(const std::string &arg, long int *lintval);
		/// overload
		static bool parse_one(const std::string &arg, int *intval);
		/// overload
		static bool parse_one(const std::string &arg, unsigned int *intval);
		/// overload
		static bool parse_one(const std::string &arg, bool *boolval);
		/// overload
		static bool parse_one(const std::string &arg, double *dblval);
		/// overload
		static bool parse_one(const std::string &arg, std::string *strval);

	protected:
		/// \overload
		bool parseParameters(int start, bool total_parse) const { configASSERT(0); return false; /* We should never get here, but typing requires it. */ };
	};

	/**
	 * A class representing a console command.
	 */
	class Command {
	public:
		Command() { };
		virtual ~Command() { };

		/**
		 * Retrieve the helptext for this command.
		 *
		 * @note Mandatory
		 *
		 * @param command The registered command name.
		 * @return Help text for the command.
		 */
		virtual std::string getHelpText(const std::string &command) const { configASSERT(0); return ""; };

		/**
		 * Execute the command.
		 *
		 * @note Mandatory
		 *
		 * @param console The calling console.  Use its safe_write() for stdout.
		 * @param parameters The parameters for this command execution.
		 */
		virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParameters &parameters) = 0;//{ configASSERT(0); };

		/**
		 * Provide completion options for the specified parameter (using .cursor_*).
		 *
		 * This function must return all valid parameter completions for the
		 * current cursor parameter at the current cursor position, as full
		 * parameter values.  It may return other values, and these will be
		 * filtered by the caller.
		 *
		 * @note Optional
		 *
		 * @param parameters The current parameters as they stand.
		 * @return
		 */
		virtual std::vector<std::string> complete(const CommandParameters &parameters) const { return std::vector<std::string>(); };
	};

	/**
	 * Parse a given command line and execute the associated command.
	 *
	 * @note Whitespace-only lines are automatically ignored as successful parses.
	 *
	 * @param console The calling console.  Use its safe_write() for stdout.
	 * @param commandline The command line to be parsed.
	 * @param cursor The current input cursor position.
	 * @return false if unknown command, else true.
	 */
	virtual bool parse(std::shared_ptr<ConsoleSvc> console, const std::string &commandline, std::string::size_type cursor=0);

	/**
	 * Register a command with this helper (or unregister if handler==nullptr).
	 *
	 * @param token The command to register.
	 * @param handler The handler for the command.
	 */
	virtual void registerCommand(const std::string &token, std::shared_ptr<Command> handler);

	//! List installed commands.
	virtual std::vector<std::string> listCommands(bool native_only=false) const;

	/**
	 * Retrieve the help text for a specific command
	 *
	 * @param command The command to return help text for.
	 * @return The help text or an empty string if empty or none present.
	 */
	virtual std::string getHelpText(const std::string &command) const;

	/// The results of a completion.
	class CompletionResult {
	public:
		std::string::size_type cursor; ///< The offset into the completion result the cursor is at.
		std::string common_prefix; ///< The common prefix of all valid completions.
		std::vector<std::string> completions; ///< All valid completion options.
		CompletionResult() : cursor(0) { /* initialized blank */ };

		/**
		 * Instantiate a CompletionResult and handle all option processing.
		 *
		 * @param prefix The provided prefix to complete
		 * @param completions The completion options.
		 */
		CompletionResult(std::string prefix, std::vector<std::string> completions);
	};

	/**
	 * Complete a provided command line, from the given cursor position.
	 *
	 * @param commandline The command line to complete
	 * @param cursor The cursor position to complete from
	 * @return A CompletionResults object.
	 */
	CompletionResult complete(const std::string &commandline, std::string::size_type cursor) const;

	/**
	 * Tokenize a command line possibly containing quoted strings.
	 * @param commandline The string to tokenize
	 * @param[out] cursor_parameter The parameter the cursor is at, or 0 if none.
	 * @param[in,out] cursor_char IN: The character the cursor is at.  OUT: The parameter-relative character the cursor is at, or std::string::npos if it is not within a parameter.
	 * @return A vector of tokens.
	 */
	static std::vector<std::string> tokenize(const std::string &commandline, std::vector<std::string>::size_type *cursor_parameter=nullptr, std::string::size_type *cursor_char=nullptr);

private:
	// Default commands:
	class HelpCommand;

	SemaphoreHandle_t mutex; ///< A mutex protecting the commandset info.
	std::map< std::string, std::shared_ptr<Command> > commandset; ///< The registered commands.

	/**
	 * Retrieve a command specified by the given string.
	 * @param command The command to return.
	 * @return The matching command object or a 'null' pointer.
	 */
	std::shared_ptr<Command> getCommand(const std::string &command) const;
	CommandParser(CommandParser const &) = delete;   ///< Class is not assignable.
	void operator=(CommandParser const &x) = delete; ///< Class is not copyable.

	CommandParser *chain; ///< A chained command parser used for unknown commands if not nullptr.
};

/**
 * Interface abstract class that other classes that support commands should implement.
 */
class ConsoleCommandSupport {
public:
	/**
	 * Register console commands related to this storage.
	 * @param parser The command parser to register to.
	 * @param prefix A prefix for the registered commands.
	 */
	virtual void registerConsoleCommands(CommandParser &parser, const std::string &prefix="") = 0;

	/**
	 * Unregister console commands related to this storage.
	 * @param parser The command parser to unregister from.
	 * @param prefix A prefix for the registered commands.
	 */
	virtual void deregisterConsoleCommands(CommandParser &parser, const std::string &prefix="") = 0;
};

#endif /* SRC_COMMON_ZYNQIPMC_LIBS_COMMANDPARSER_H_ */
