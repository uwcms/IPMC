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

#ifndef SRC_COMMON_ZYNQIPMC_LIBS_ANSICODE_H_
#define SRC_COMMON_ZYNQIPMC_LIBS_ANSICODE_H_

#include <string>
#include <vector>
#include <map>

/**
 * A class representing and parsing ANSI Control Codes.
 */
class ANSICode {
public:
	std::string buffer; ///< The buffer for parsing.

	//! An enum representing the state of the current buffer.
	enum ParseState {
		PARSE_EMPTY, PARSE_INCOMPLETE, PARSE_COMPLETE, PARSE_INVALID,
	};

	/**
	 * Parse the current buffer as an ANSI control code.
	 *
	 * @return The status of the parsing.
	 */
	enum ParseState parse();
	/// \overload
	enum ParseState parse(std::string append) { this->buffer.append(append); return this->parse(); };
	/// \overload
	enum ParseState parse(char append) { this->buffer += append; return this->parse(); };

	std::string code; ///< The actual code of the last parsed code.
	std::string name; ///< The name of the last parsed code.
	std::vector<int> parameters; ///< The parameters of the last parsed code.

	static const std::map<std::string, std::string> codenames; ///< A mapping of ANSICode().code to ANSICode().name

	/**
	 * Convert 'A' to '^A'.
	 * @param key 'A'
	 * @return '^A'
	 */
	static constexpr char renderASCIIControlkey(char key) {
		// ref: http://jkorpela.fi/chars/c0.html
		return ( key >= 'A' && key <= '_' ? key-('A'-1) : 0);
	}

	/**
	 * Convert '^A' to 'A'.
	 * @param key '^A'
	 * @return 'A'
	 */
	static constexpr char parseASCIIControlkey(char key) {
		// ref: http://jkorpela.fi/chars/c0.html
		return ( key >= 1 && key <= 31 ? key+('A'-1) : 0 );
	}

	/// ASCII character constants
	///@{
	static const std::string ASCII_BELL;
	static const std::string ASCII_BACKSPACE;
	///@}

	/// ANSI escape code constants
	///@{
	static const std::string ANSI_ERASE_TO_END_OF_LINE;
	static const std::string ANSI_ERASE_TO_START_OF_LINE;
	static const std::string ANSI_ERASE_LINE;
	static const std::string ANSI_ERASE_DOWN;
	static const std::string ANSI_CURSOR_SAVE;
	static const std::string ANSI_CURSOR_RESTORE;
	static const std::string ANSI_CURSOR_FORWARD_ONE;
	static const std::string ANSI_CURSOR_FORWARD_INTFMT;
	static const std::string ANSI_CURSOR_BACK_ONE;
	static const std::string ANSI_CURSOR_BACK_INTFMT;
	static const std::string ANSI_CURSOR_ABSOLUTE_HORIZONTAL_POSITION_INTFMT;
	static const std::string ANSI_CURSOR_UP_ONE;
	static const std::string ANSI_CURSOR_UP_INTFMT;
	static const std::string ANSI_CURSOR_DOWN_ONE;
	static const std::string ANSI_CURSOR_DOWN_INTFMT;
	static const std::string ANSI_CURSOR_HOME;
	static const std::string ANSI_CURSOR_HOME_2INTFMT; // row, column
	static const std::string ANSI_CURSOR_QUERY_POSITION;
	static const std::string ANSI_INSERT_CHARACTER_POSITION;
	static const std::string ANSI_DELETE_CHARACTER_POSITION;
	static const std::string ANSI_INSERT_LINE;
	static const std::string ANSI_INSERT_LINE_INTFMT;
	static const std::string ANSI_DELETE_LINE;
	static const std::string ANSI_DELETE_LINE_INTFMT;
	///@}

	/// Terminal color choices.
	enum TermColor {
		BLACK      = 0,
		DARKGREY   = 60,
		RED        = 1,
		SALMON     = 61,
		GREEN      = 2,
		LIME       = 62,
		ORANGE     = 3,
		YELLOW     = 63,
		INDIGO     = 4,
		BLUE       = 4,
		LILAC      = 64,
		MAGENTA    = 5,
		PINK       = 65,
		TUROQUOISE = 6,
		CYAN       = 66,
		LIGHTGREY  = 7,
		WHITE      = 67,
		NOCOLOR    = 9,
	};

	/**
	 * Render an ANSI color code.
	 * @param fgcolor   The foreground color.
	 * @param bgcolor   The background color.
	 * @param bold      Bold font if true.
	 * @param underline Underlined font if true.
	 * @param inverse   Inverse display mode if true.
	 * @return An ANSI color code.
	 */
	static std::string color(enum TermColor fgcolor=NOCOLOR, enum TermColor bgcolor=NOCOLOR, bool bold=false, bool underline=false, bool inverse=false);
};

#endif /* SRC_COMMON_ZYNQIPMC_LIBS_ANSICODE_H_ */
