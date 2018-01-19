#ifndef SRC_COMMON_UW_IPMC_LIBS_ANSICODE_H_
#define SRC_COMMON_UW_IPMC_LIBS_ANSICODE_H_

#include <string>
#include <vector>
#include <map>

/**
 * A class representing and parsing ANSI Control Codes.
 */
class ANSICode {
public:
	std::string buffer; ///< The buffer for parsing.

	/// An enum representing the state of the current buffer.
	enum ParseState {
		PARSE_EMPTY, PARSE_INCOMPLETE, PARSE_COMPLETE, PARSE_INVALID,
	};

	enum ParseState parse();
	/// \overload
	enum ParseState parse(std::string append) { this->buffer.append(append); return this->parse(); };
	/// \overload
	enum ParseState parse(char append) { this->buffer += append; return this->parse(); };

	std::string code; ///< The actual code of the last parsed code.
	std::string name; ///< The name of the last parsed code.
	std::vector<int> parameters; ///< The parameters of the last parsed code.

	static const std::map<std::string, std::string> codenames; ///< A mapping of ANSICode().code to ANSICode().name

	/// ANSI escape code constants
	///@{
	static const std::string ANSI_ERASE_TO_END_OF_LINE;
	static const std::string ANSI_ERASE_TO_START_OF_LINE;
	static const std::string ANSI_ERASE_LINE;
	static const std::string ANSI_CURSOR_FORWARD_ONE;
	static const std::string ANSI_CURSOR_FORWARD_INTFMT;
	static const std::string ANSI_CURSOR_BACK_ONE;
	static const std::string ANSI_CURSOR_BACK_INTFMT;
	static const std::string ANSI_ABSOLUTE_HORIZONTAL_POSITION_INTFMT;
	static const std::string ANSI_CURSOR_UP_ONE;
	static const std::string ANSI_CURSOR_UP_INTFMT;
	static const std::string VT102_INSERT_CHARACTER_POSITION;
	static const std::string VT102_DELETE_CHARACTER_POSITION;
	static const std::string VT102_INSERT_LINE;
	static const std::string VT102_INSERT_LINE_INTFMT;
	///@}
};

#endif /* SRC_COMMON_UW_IPMC_LIBS_ANSICODE_H_ */
