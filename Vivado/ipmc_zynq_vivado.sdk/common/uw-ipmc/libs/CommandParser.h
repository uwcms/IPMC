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

/**
 * A commandline parser class, which handles registration of commands and
 * parsing and dispatch of command lines supplied as strings.
 */
class CommandParser {
public:
	CommandParser();
	virtual ~CommandParser();

	/**
	 * A parameter set for a parsed command.
	 */
	class CommandParameters {
	public:
		/**
		 * Construct an intelligent parameterset from an unparsed parameter list.
		 * @param parameters The list of string parameters.
		 */
		CommandParameters(const std::vector<std::string> &parameters) : parameters(parameters) { };
		virtual ~CommandParameters() { };

		/**
		 * Get the number of parameters supplied.
		 * @return The number of parameters.
		 */
		size_t nargs() { return this->parameters.size(); };

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
	protected:
		/// \overload
		bool parse_parameters(int start, bool total_parse) const { configASSERT(0); return false; /* We should never get here, but typing requires it. */ };

		const std::vector<std::string> parameters; ///< The internal unparsed parameter list.

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
		static bool parse_one(const std::string &arg, double *dblval);
		/// overload
		static bool parse_one(const std::string &arg, std::string *strval);
	};

	/**
	 * A function called when a specific command is run.
	 *
	 * @param stdout     The location to print any output to.
	 * @param parameters The command's parameters.
	 */
	typedef std::function<void(std::function<void(std::string)> print, const CommandParameters &parameters)> handler_t;

	virtual bool parse(std::function<void(std::string)> print, const std::string &commandline);
	virtual void register_command(const std::string &command, handler_t handler, const std::string &helptext);
	virtual std::string get_helptext(const std::string &command);

	static std::vector<std::string> tokenize(const std::string &commandline);
protected:
	SemaphoreHandle_t mutex; ///< A mutex protecting the commandset info.
	std::map<std::string, handler_t> commandset; ///< The registered commands.
	std::map<std::string, std::string> commandhelp; ///< The registered commands' help texts.
};

#endif /* SRC_COMMON_UW_IPMC_LIBS_COMMANDPARSER_H_ */
