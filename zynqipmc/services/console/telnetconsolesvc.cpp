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

#include "telnetconsolesvc.h"
#include <core.h>
#include <drivers/tracebuffer/tracebuffer.h>

Telnet::TelnetConsoleSvc::TelnetConsoleSvc(std::shared_ptr<Socket> socket, std::shared_ptr<InputProtocolParser> proto, CommandParser &parser, const std::string &name, LogTree &logtree, bool echo, TickType_t read_data_timeout, std::function<void(TelnetConsoleSvc&)> shutdownComplete_cb)
		: ConsoleSvc(parser, name, logtree, echo, read_data_timeout), socket(socket), proto(proto), shutdownComplete_cb(shutdownComplete_cb) {
	//configASSERT(this->socket->valid());
	this->sock_mutex = xSemaphoreCreateMutex();
	configASSERT(this->sock_mutex);
}

Telnet::TelnetConsoleSvc::~TelnetConsoleSvc() {
	vSemaphoreDelete(this->sock_mutex);

	/* Once we return our socket will be destroyed.
	 *
	 * Let any remaining TCP packets flow for a tenth of a second or so.
	 *
	 * This is probably not needed.
	 */
	vTaskDelay(configTICK_RATE_HZ / 10);
}

void Telnet::TelnetConsoleSvc::close() {
	this->shutdown = 3;
	// This might be causing crashes, and our read_data_timeout is small anyway.  We'll just not worry about this.
	//this->socket->close();
}

void Telnet::TelnetConsoleSvc::shutdownComplete() {
	this->socket = nullptr;
	if (!!this->shutdownComplete_cb)
		this->shutdownComplete_cb(*this);
}

ssize_t Telnet::TelnetConsoleSvc::rawRead(char *buf, size_t len, TickType_t timeout, TickType_t read_data_timeout) {
	AbsoluteTimeout abstimeout( (read_data_timeout<timeout ? read_data_timeout : timeout) );
	std::string inattempt(this->log_input["in"]["att"].getPath());
	TRACE.log(inattempt.data(), inattempt.size(), LogTree::LOG_TRACE, "A", 1, false);

	do {
		int rv = this->socket->recv(buf, len);
		if (rv < 0) {
			TRACE.log(inattempt.data(), inattempt.size(), LogTree::LOG_TRACE, "XXX", 3, false);
			if (errno != EAGAIN)
				this->close(); // Not a timeout.  Terminate.
			return -1;
		}
		// Ok, we have input.  It might contain telnet control codes though.
		//TRACE.log(this->log_input.path.data(), this->log_input.path.size(), LogTree::LOG_TRACE, buf, rv, true); // Raw input.
		size_t protolen = rv;
		std::string inraw(this->log_input["in"]["raw"].getPath());
		std::string inproto(this->log_input["in"]["proto"].getPath());
		std::string inprpl(this->log_input["in"]["prpl"].getPath());
		TRACE.log(inraw.data(), inraw.size(), LogTree::LOG_TRACE, buf, protolen, true);
		std::string proto_reply = this->proto->parseInput(buf, &protolen);
		TRACE.log(inproto.data(), inproto.size(), LogTree::LOG_TRACE, buf, protolen, true);

		if (this->proto->incompatible_client) {
			this->close();
			return -1;
		}

		this->rawWrite(proto_reply.data(), proto_reply.size(), abstimeout.getTimeout());
		TRACE.log(inprpl.data(), inprpl.size(), LogTree::LOG_TRACE, proto_reply.data(), proto_reply.size(), true);

		if (protolen)
			return protolen;

	} while (abstimeout.getTimeout() > 0);

	return 0; // Didn't return protolen above, but timed out.
}

ssize_t Telnet::TelnetConsoleSvc::rawWrite(const char *buf, size_t len, TickType_t timeout) {
	std::string inattempt(this->log_input["out"]["att"].getPath());
	TRACE.log(inattempt.data(), inattempt.size(), LogTree::LOG_TRACE, "A", 1, false);
	ssize_t rv = this->socket->send((void*)buf, len);
	if (rv < 0) {
		if (errno != EAGAIN)
			this->close(); // Not a timeout.  Terminate.

		TRACE.log(inattempt.data(), inattempt.size(), LogTree::LOG_TRACE, "XXX", 3, false);
		return -1;
	}
	TRACE.log(inattempt.data(), inattempt.size(), LogTree::LOG_TRACE, "Z", 1, false);
	return rv;
}

Telnet::InputProtocolParser::InputProtocolParser()
	: incompatible_client(false), last_keepalive(0) {
};

std::string Telnet::InputProtocolParser::parseInput(char *buf, size_t *len) {
	// We're just going to throw this into our code buffer, then parse & shift any codes.
	this->code_buffer.append(buf, *len);
	size_t bufi = 0; // Current position in the now-output buffer.
	std::string replybuf;

	if (this->last_keepalive < get_tick64() - 5*configTICK_RATE_HZ) {
		// Regardless of anything else, we'll reply with a NOP of our own, just as a "hey we're still alive" sort of thing.
		this->last_keepalive = get_tick64();
		replybuf += InputProtocolParser::CODE_IAC;
		replybuf += InputProtocolParser::CODE_NOP;
	}

	while (this->code_buffer.size()) {
		// Pass through non Interpret As Command sequences.
		if (this->code_buffer[0] != InputProtocolParser::CODE_IAC) {
			if (this->sb_buffer.size()) {
				this->sb_buffer += this->code_buffer[0];
			} else {
				if (this->code_buffer[0] == '\0')
					buf[bufi++] = '\n'; // My telnet client is sending \r\0 rather than \r\n.
				else
					buf[bufi++] = this->code_buffer[0];
			}

			this->code_buffer.erase(0,1);
			continue;
		}
		// If we ONLY have IAC, return.
		if (this->code_buffer.size() == 1) {
			*len = bufi;
			return replybuf;
		}

		// IAC was escaping another IAC.
		if (this->code_buffer[1] == InputProtocolParser::CODE_IAC) {
			if (this->sb_buffer.size())
				this->sb_buffer += this->code_buffer[1];
			else
				buf[bufi++] = this->code_buffer[1];
			this->code_buffer.erase(0,2);
			continue;
		}

		// If in subnegotiation, we support only CODE_SE.
		if (this->sb_buffer.size()) {
			if (this->code_buffer[1] == InputProtocolParser::CODE_SE) {
				replybuf += this->negotiate(InputProtocolParser::CODE_SB, this->sb_buffer[2], this->sb_buffer.substr(3));
				this->sb_buffer.clear();
				this->code_buffer.erase(0,2);
				continue;
			} else {
				// Well, bail on the subnegotiation and parse it as whatever the heck it is, I guess.
				this->sb_buffer.clear();
			}
		}

		if (this->code_buffer[1] == InputProtocolParser::CODE_SB) {
			if (this->code_buffer.size() < 3) {
				// Incomplete.  We don't even have a feature yet.
				*len = bufi;
				return replybuf;
			}
			// Copy the header to the subnegotiation buffer, which will redirect future input to that buffer until CODE_SE.
			this->sb_buffer = this->code_buffer.substr(0,3);
			this->code_buffer.erase(0, 3);
			continue;
		}
		// Perform any basic negotiation required.
		if (this->code_buffer[1] >= InputProtocolParser::CODE_WILL && this->code_buffer[1] <= InputProtocolParser::CODE_DONT) {
			if (this->code_buffer.size() < 3) {
				// Incomplete.  We don't have a feature yet.
				*len = bufi;
				return replybuf;
			}
			replybuf += this->negotiate(this->code_buffer[1], this->code_buffer[2]);
			this->code_buffer.erase(0, 3);
			continue;
		}

		// Some other random control code we're ignoring.
		// We know it's two bytes, at least, since it's not a negotiation command.
		this->code_buffer.erase(0, 2);
	}
	// Done!
	*len = bufi;
	return replybuf;
}

std::string IAC_STR(1, Telnet::InputProtocolParser::CODE_IAC);
#define COMPOSE_WWDD(request, feature) ( IAC_STR + static_cast<char>(request) + static_cast<char>(feature) )

std::string Telnet::InputProtocolParser::negotiate(uint8_t req, uint8_t feature, std::string subnegotiation) {
	switch (feature) {
	case FEATURE_ECHO:
		switch (req) {
		case InputProtocolParser::CODE_WILL:
			// No, please do not.  This will go VERY poorly.  You are incompatible.  Stop that..
			this->incompatible_client = true;
			return COMPOSE_WWDD(CODE_DONT, feature);
		case CODE_DO:   return COMPOSE_WWDD(CODE_WILL, feature); // Yeah, we'll do that.
		case CODE_DONT: return COMPOSE_WWDD(CODE_WILL, feature); // We refuse.  We're going to echo back to you.  Sorry.
		}
		break;

	case FEATURE_SUPPRESS_GO_AHEAD:
		switch (req) {
		case CODE_DO:   return COMPOSE_WWDD(CODE_WILL, feature); // Yeah, we'll do that.
		case CODE_DONT: return COMPOSE_WWDD(CODE_WONT, feature); // Uhh... Whatever you say.  Sending it was never mandatory anyway.
		}
		break;

	case FEATURE_TERMINAL_TYPE:
		// Kudos to http://mud-dev.wikidot.com/telnet:negotiation
		switch (req) {
		case CODE_WILL:
			if (this->remote_terminal_type.empty())
				return IAC_STR + (char)CODE_SB + '\x01' /* SEND */ + (char)CODE_IAC + (char)CODE_SE; // Request next terminal type.
			else
				return ""; // Thanks, but we've already done it, and I'm not sure if you've reset your iterator to 0.  Not messing with this.
		case CODE_DO:   return COMPOSE_WWDD(CODE_WONT, feature); // We don't have a "terminal type" exactly... Let us negotiate yours.
		case CODE_DONT: return COMPOSE_WWDD(CODE_WONT, feature); // Ok.
		case CODE_SB:
			if (subnegotiation.size() > 2 && subnegotiation[0] == '\0' /* IS */) {
				if (this->remote_terminal_type == subnegotiation.substr(1)) {
					/* Negotiation already complete, we hit end of the list, or
					 * the thing we negotiated last time anyway happened to be
					 * first.
					 */
				}
				else {
					this->remote_terminal_type = subnegotiation.substr(1);
					if (
							this->remote_terminal_type == "ANSI" ||
							this->remote_terminal_type.substr(1) == "VT100" ||
							this->remote_terminal_type.substr(1) == "VT102"
						) {
						// We've reached our terminal preference.
					}
					else {
						return IAC_STR + (char)CODE_SB + '\x01' /* SEND */ + (char)CODE_IAC + (char)CODE_SE; // Request next terminal type.
					}
				}
			}
		}
		break;
	}

	return ""; // Unsupported or no response required.
}

std::string Telnet::InputProtocolParser::buildInitialNegotiation() {
	std::string negotiation;
	negotiation += COMPOSE_WWDD(CODE_WILL, FEATURE_ECHO); // We wish to echo back to you.
	negotiation += COMPOSE_WWDD(CODE_DONT, FEATURE_ECHO); // We DON'T want YOU to echo back to us.  It would be... bad.
	negotiation += COMPOSE_WWDD(CODE_WILL, FEATURE_SUPPRESS_GO_AHEAD); // We don't send these either way.
	negotiation += COMPOSE_WWDD(CODE_DO,   FEATURE_SUPPRESS_GO_AHEAD); // We don't need these.
	negotiation += COMPOSE_WWDD(CODE_DO,   FEATURE_TERMINAL_TYPE); // Please change to ANSI or VT10[02] officially, if possible.  We're sending those codes anyway.
	return negotiation;
}
