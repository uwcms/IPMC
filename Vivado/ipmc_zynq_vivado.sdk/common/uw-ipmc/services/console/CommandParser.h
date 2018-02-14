/*
 * CommandParser.h
 *
 *  Created on: Jan 12, 2018
 *      Author: jtikalsky
 */

#ifndef SRC_COMMON_UW_IPMC_LIBS_COMMANDPARSER_H_
#define SRC_COMMON_UW_IPMC_LIBS_COMMANDPARSER_H_

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
 * A commandline parser class, which handles registration of commands and
 * parsing and dispatch of command lines supplied as strings.
 */
class CommandParser {
public:
	CommandParser(CommandParser *chain=NULL);
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
		template <typename T, typename... Args> bool parse_parameters(int start, bool total_parse, T arg1, Args... subseq) const {
			if (start + 1 + sizeof...(subseq) > this->parameters.size())
				return false; // We want more than we have.
			if (total_parse && this->parameters.size() < start + 1 + sizeof...(subseq))
				return false; // We have more than we want, and that bothers us.
			if (arg1 && !CommandParameters::parse_one(this->parameters[start], arg1))
				return false; // The current arg failed to parse.
			if (sizeof...(subseq) > 0)
				return this->parse_parameters(start+1, total_parse, subseq...);
			else
				return true;
		}

		const std::vector<std::string> parameters; ///< The internal unparsed parameter list.
		const size_type cursor_parameter; ///< The parameter the cursor is in, or str::npos if it is in an inter-parameter space.
		const std::string::size_type cursor_char; ///< The character position within the parameter that the cursor is at.
	protected:
		/// \overload
		bool parse_parameters(int start, bool total_parse) const { configASSERT(0); return false; /* We should never get here, but typing requires it. */ };

		/// Parse one argument.
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
		static bool parse_one(const std::string &arg, bool *boolval);
		/// overload
		static bool parse_one(const std::string &arg, double *dblval);
		/// overload
		static bool parse_one(const std::string &arg, std::string *strval);
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
		 * \note Mandatory
		 *
		 * @param command The registered command name.
		 * @return Help text for the command.
		 */
		virtual std::string get_helptext(const std::string &command) const { configASSERT(0); return ""; };

		/**
		 * Execute the command.
		 *
		 * \note Mandatory
		 *
		 * @param console The calling console.  Use its safe_write() for stdout.
		 * @param parameters The parameters for this command execution.
		 */
		virtual void execute(ConsoleSvc &console, const CommandParameters &parameters) { configASSERT(0); };

		/**
		 * Provide completion options for the specified parameter (using .cursor_*).
		 *
		 * This function must return all valid parameter completions for the
		 * current cursor parameter at the current cursor position, as full
		 * parameter values.  It may return other values, and these will be
		 * filtered by the caller.
		 *
		 * \note Optional
		 *
		 * @param parameters The current parameters as they stand.
		 * @return
		 */
		virtual std::vector<std::string> complete(const CommandParameters &parameters) const { return std::vector<std::string>(); };
	};

	virtual bool parse(ConsoleSvc &console, const std::string &commandline, std::string::size_type cursor=0);
	virtual void register_command(const std::string &token, std::shared_ptr<Command> handler);
	virtual std::vector<std::string> list_commands(bool native_only=false) const;
	virtual std::string get_helptext(const std::string &command) const;

	/// The results of a completion.
	class CompletionResult {
	public:
		std::string::size_type cursor; ///< The offset into the completion result the cursor is at.
		std::string common_prefix; ///< The common prefix of all valid completions.
		std::vector<std::string> completions; ///< All valid completion options.
		CompletionResult() : cursor(0) { /* initialized blank */ };
		CompletionResult(std::string prefix, std::vector<std::string> completions);
	};
	CompletionResult complete(const std::string &commandline, std::string::size_type cursor) const;

	static std::vector<std::string> tokenize(const std::string &commandline, std::vector<std::string>::size_type *cursor_parameter=NULL, std::string::size_type *cursor_char=NULL);
protected:
	SemaphoreHandle_t mutex; ///< A mutex protecting the commandset info.
	std::map< std::string, std::shared_ptr<Command> > commandset; ///< The registered commands.
	std::shared_ptr<Command> get_command(const std::string &command) const;
	CommandParser(CommandParser const &) = delete;   ///< Class is not assignable.
	void operator=(CommandParser const &x) = delete; ///< Class is not copyable.
public:
	CommandParser *chain; ///< A chained command parser used for unknown commands if not NULL.
};

#endif /* SRC_COMMON_UW_IPMC_LIBS_COMMANDPARSER_H_ */
