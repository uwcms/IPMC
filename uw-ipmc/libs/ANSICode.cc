/*
 * ANSICode.cc
 *
 *  Created on: Jan 19, 2018
 *      Author: jtikalsky
 */

#include <libs/ANSICode.h>
#include <IPMC.h>

/**
 * Parse the current buffer as an ANSI control code.
 *
 * @return The status of the parsing.
 */
enum ANSICode::ParseState ANSICode::parse() {
	this->code.clear();
	this->name.clear();
	this->parameters.clear();

	if (this->buffer.empty())
		return PARSE_EMPTY;

	if (this->buffer[0] != '\x1B')
		return PARSE_INVALID; // Not a control sequence.
	else if (this->buffer.size() == 1)
		return PARSE_INCOMPLETE; // We have an escape code, but nothing else yet.
	else if (this->buffer.size() == 2) {
		if (this->buffer[1] == 'O')
			return PARSE_INCOMPLETE; // O is apparently an annoying special case, starting a two letter code.
		else if (
				('A' <= this->buffer[1] && this->buffer[1] <= 'Z') ||
				('a' <= this->buffer[1] && this->buffer[1] <= 'z')
				) {
			this->code = this->buffer[1];
			this->name = this->code;
			if (this->codenames.count(this->name))
				this->name = ANSICode::codenames.at(this->name);
			return PARSE_COMPLETE;
		}
		else if (this->buffer[1] == '[')
			return PARSE_INCOMPLETE; // Continuation.
		else {
			return PARSE_INVALID;
		}
	}
	else if (this->buffer.size() == 3 && this->buffer[1] == 'O') {
		// Handle that annoying special case, of the two letter O_ code.
		if (
				('A' <= this->buffer[2] && this->buffer[2] <= 'Z') ||
				('a' <= this->buffer[2] && this->buffer[2] <= 'z')
				) {
			this->code = this->buffer.substr(1,2);
			this->name = this->code;
			if (this->codenames.count(this->name))
				this->name = ANSICode::codenames.at(this->name);
			return PARSE_COMPLETE;
		}
		else
			return PARSE_INVALID; // Turns out it wasn't valid after all.
	}

	std::string param_buf;
	for (auto it = this->buffer.begin()+2, eit = this->buffer.end(); it != eit; ++it) {
		if ('0' <= *it && *it <= '9') {
			param_buf += *it;
		}
		else if (*it == ';') {
			if (param_buf.empty())
				this->parameters.push_back(0);
			else {
				init_stdlib_mutex();
				xSemaphoreTake(stdlib_mutex, portMAX_DELAY);
				this->parameters.push_back(atoi(param_buf.c_str()));
				xSemaphoreGive(stdlib_mutex);
				param_buf.clear();
			}
		}
		else if ( ('A' <= *it && *it <= 'Z') || ('a' <= *it && *it <= 'z') || (*it == '~') ) {
			if (!param_buf.empty()) {
				init_stdlib_mutex();
				xSemaphoreTake(stdlib_mutex, portMAX_DELAY);
				this->parameters.push_back(atoi(param_buf.c_str()));
				xSemaphoreGive(stdlib_mutex);
				param_buf.clear();
			}

			if (*it == '~') {
				/* These are things like INS, DEL, HOME, END, PGUP, PGDN.
				 *
				 * They vary by the parameter number, so, for tilde, the last
				 * parameter is part of the command, and it's invalid without
				 * one.
				 */
				if (this->parameters.size() != 1)
					return PARSE_INVALID;
				this->code = stdsprintf("[%d~", this->parameters[0]);
				this->parameters.pop_back();
				this->name = this->code;
				if (this->codenames.count(this->name))
					this->name = ANSICode::codenames.at(this->name);
				if (it+1 != this->buffer.end())
					return PARSE_INVALID; // The code ended before the buffer did.
				return PARSE_COMPLETE;
			}
			else {
				this->code = std::string("[") + *it; // Don't forget this is an extended code.
				this->name = this->code;
				if (this->codenames.count(this->name))
					this->name = ANSICode::codenames.at(this->name);
				if (it+1 != this->buffer.end())
					return PARSE_INVALID; // The code ended before the buffer did.
				return PARSE_COMPLETE;
			}
		}
		else
			return PARSE_INVALID; // Duno what we got, but it's not something we want.
	}
	return PARSE_INCOMPLETE; // We got to the end without bailing as invalid, but we didn't complete either.
}

const std::map<std::string, std::string> ANSICode::codenames {
	{"D", "SCROLL_DOWN"},
	{"M", "SCROLL_UP"},
	{"OP", "F1"},
	{"OQ", "F2"},
	{"OR", "F3"},
	{"OS", "F4"},
	{"[R", "CURSOR_POSITION_REPORT"},
	{"[1~", "HOME"},
	{"[2~", "INSERT"},
	{"[3~", "DELETE"},
	{"[4~", "END"},
	{"[5~", "PAGE_UP"},
	{"[6~", "PAGE_DOWN"},
	{"[2^", "CTRL_INSERT"},
	{"[3^", "CTRL_DELETE"},
	{"[5^", "CTRL_PAGE_UP"},
	{"[6^", "CTRL_PAGE_DOWN"},
	{"[2$", "SHIFT_INSERT"},
	{"[3$", "SHIFT_DELETE"},
	{"[5$", "SHIFT_PAGE_UP"},
	{"[6$", "SHIFT_PAGE_DOWN"},
	{"[2@", "CTRL_SHIFT_INSERT"},
	{"[3@", "CTRL_SHIFT_DELETE"},
	{"[5@", "CTRL_SHIFT_PAGE_UP"},
	{"[6@", "CTRL_SHIFT_PAGE_DOWN"},
	{"[15~", "F5"},
	{"[17~", "F6"},
	{"[18~", "F7"},
	{"[19~", "F8"},
	{"[20~", "F9"},
	{"[21~", "F10"},
	{"[23~", "F11"},
	{"[24~", "F12"},
	{"[A", "ARROW_UP"},
	{"[B", "ARROW_DOWN"},
	{"[C", "ARROW_RIGHT"},
	{"[D", "ARROW_LEFT"},
};

// Thank god for http://www.inwap.com/pdp10/ansicode.txt
const std::string ANSICode::ASCII_BELL = "\x07";
const std::string ANSICode::ASCII_BACKSPACE = "\x08";
const std::string ANSICode::ANSI_ERASE_TO_END_OF_LINE = "\x1B[K";
const std::string ANSICode::ANSI_ERASE_TO_START_OF_LINE = "\x1B[1K";
const std::string ANSICode::ANSI_ERASE_LINE = "\x1B[2K";
const std::string ANSICode::ANSI_ERASE_DOWN = "\x1B[J";
const std::string ANSICode::ANSI_CURSOR_SAVE = "\x1B[s";
const std::string ANSICode::ANSI_CURSOR_RESTORE = "\x1B[u";
const std::string ANSICode::ANSI_CURSOR_FORWARD_ONE = "\x1B[C";
const std::string ANSICode::ANSI_CURSOR_FORWARD_INTFMT = "\x1B[%dC";
const std::string ANSICode::ANSI_CURSOR_BACK_ONE = "\x1B[D";
const std::string ANSICode::ANSI_CURSOR_BACK_INTFMT = "\x1B[%dD";
const std::string ANSICode::ANSI_CURSOR_ABSOLUTE_HORIZONTAL_POSITION_INTFMT = "\x1B[%dG";
const std::string ANSICode::ANSI_CURSOR_UP_ONE = "\x1B[A";
const std::string ANSICode::ANSI_CURSOR_UP_INTFMT = "\x1B[%dA";
const std::string ANSICode::ANSI_CURSOR_DOWN_ONE = "\x1B[B";
const std::string ANSICode::ANSI_CURSOR_DOWN_INTFMT = "\x1B[%dB";
const std::string ANSICode::ANSI_CURSOR_HOME= "\x1B[H";
const std::string ANSICode::ANSI_CURSOR_HOME_2INTFMT = "\x1B[%d;%dH";
const std::string ANSICode::ANSI_CURSOR_QUERY_POSITION = "\x1B[6n";
const std::string ANSICode::ANSI_INSERT_CHARACTER_POSITION = "\x1b[1@";
const std::string ANSICode::ANSI_DELETE_CHARACTER_POSITION = "\x1b[1P";
const std::string ANSICode::ANSI_INSERT_LINE = "\x1b[L";
const std::string ANSICode::ANSI_INSERT_LINE_INTFMT = "\x1b[%dL";
const std::string ANSICode::ANSI_DELETE_LINE = "\x1b[M";
const std::string ANSICode::ANSI_DELETE_LINE_INTFMT = "\x1b[%dM";
