/*
 * TelnetConsole.cpp
 *
 *  Created on: Feb 9, 2018
 *      Author: jtikalsky
 */

#include <drivers/tracebuffer/tracebuffer.h>
#include <services/console/TelnetConsoleSvc.h>
#include <IPMC.h>

/**
 * Instantiate a UART console service.
 *
 * \note This will take ownership of the socket, and of itself, and delete
 *       both upon termination.
 *
 * @param socket The socket
 * @param proto The protocol parser associated with this socket.
 * @param parser The command parser to use.
 * @param name The name of the service for the process and such things.
 * @param logtree The log tree root for this service.
 * @param echo If true, enable echo and interactive management.
 * @param read_data_timeout The timeout for reads when data is available.
 * @param shutdown_complete_cb A function to call to notify external resources about a completed shutdown.
 */
Telnet::TelnetConsoleSvc::TelnetConsoleSvc(std::shared_ptr<Socket> socket, std::shared_ptr<InputProtocolParser> proto, CommandParser &parser, const std::string &name, LogTree &logtree, bool echo, TickType_t read_data_timeout, std::function<void(TelnetConsoleSvc&)> shutdown_complete_cb)
		: ConsoleSvc(parser, name, logtree, echo, read_data_timeout), socket(socket), proto(proto), shutdown_complete_cb(shutdown_complete_cb) {
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

/**
 * Close and delete the underlying socket.
 *
 * \warning This will trigger a "delete this".
 *
 * @param wait If true, block until shutdown is complete
 */
void Telnet::TelnetConsoleSvc::close() {
	this->shutdown = 3;
	// This might be causing crashes, and our read_data_timeout is small anyway.  We'll just not worry about this.
	//this->socket->close();
}

void Telnet::TelnetConsoleSvc::shutdown_complete() {
	this->socket = nullptr;
	if (!!this->shutdown_complete_cb)
		this->shutdown_complete_cb(*this);
}

ssize_t Telnet::TelnetConsoleSvc::raw_read(char *buf, size_t len, TickType_t timeout, TickType_t read_data_timeout) {
	AbsoluteTimeout abstimeout( (read_data_timeout<timeout ? read_data_timeout : timeout) );
	std::string inattempt(this->log_input["in"]["att"].path);
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
		std::string inraw(this->log_input["in"]["raw"].path);
		std::string inproto(this->log_input["in"]["proto"].path);
		std::string inprpl(this->log_input["in"]["prpl"].path);
		TRACE.log(inraw.data(), inraw.size(), LogTree::LOG_TRACE, buf, protolen, true);
		std::string proto_reply = this->proto->parse_input(buf, &protolen);
		TRACE.log(inproto.data(), inproto.size(), LogTree::LOG_TRACE, buf, protolen, true);
		if (this->proto->incompatible_client) {
			this->close();
			return -1;
		}
		this->raw_write(proto_reply.data(), proto_reply.size(), abstimeout.get_timeout());
		TRACE.log(inprpl.data(), inprpl.size(), LogTree::LOG_TRACE, proto_reply.data(), proto_reply.size(), true);
		if (protolen)
			return protolen;
	} while (abstimeout.get_timeout() > 0);
	return 0; // Didn't return protolen above, but timed out.
}

ssize_t Telnet::TelnetConsoleSvc::raw_write(const char *buf, size_t len, TickType_t timeout) {
	std::string inattempt(this->log_input["out"]["att"].path);
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

/**
 * Parse telnet protocol input, prepare responses, and strip the supplied input buffer.
 * @param buf The input buffer to process and strip codes from.
 * @param[in,out] len A pointer to the length of the input buffer.  Updated as codes are stripped.
 * @return Protocol response data to return to the client verbatim.
 */
std::string Telnet::InputProtocolParser::parse_input(char *buf, size_t *len) {
	// We're just going to throw this into our code buffer, then parse & shift any codes.
	this->code_buffer.append(buf, *len);
	size_t bufi = 0; // Current position in the now-output buffer.
	std::string replybuf;

	if (this->last_keepalive < get_tick64() - 5*configTICK_RATE_HZ) {
		// Regardless of anything else, we'll reply with a NOP of our own, just as a "hey we're still alive" sort of thing.
		this->last_keepalive = get_tick64();
		replybuf += Telnet::CODE_IAC;
		replybuf += Telnet::CODE_NOP;
	}

	while (this->code_buffer.size()) {
		// Pass through non Interpret As Command sequences.
		if (this->code_buffer[0] != Telnet::CODE_IAC) {
			if (this->sb_buffer.size()) {
				this->sb_buffer += this->code_buffer[0];
			}
			else {
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
		if (this->code_buffer[1] == Telnet::CODE_IAC) {
			if (this->sb_buffer.size())
				this->sb_buffer += this->code_buffer[1];
			else
				buf[bufi++] = this->code_buffer[1];
			this->code_buffer.erase(0,2);
			continue;
		}
		// If in subnegotiation, we support only CODE_SE.
		if (this->sb_buffer.size()) {
			if (this->code_buffer[1] == Telnet::CODE_SE) {
				replybuf += this->negotiate(Telnet::CODE_SB, this->sb_buffer[2], this->sb_buffer.substr(3));
				this->sb_buffer.clear();
				this->code_buffer.erase(0,2);
				continue;
			}
			else {
				// Well, bail on the subnegotiation and parse it as whatever the heck it is, I guess.
				this->sb_buffer.clear();
			}
		}
		if (this->code_buffer[1] == Telnet::CODE_SB) {
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
		if (this->code_buffer[1] >= Telnet::CODE_WILL && this->code_buffer[1] <= Telnet::CODE_DONT) {
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

std::string IAC_STR(1, Telnet::CODE_IAC);
#define COMPOSE_WWDD(request, feature) ( IAC_STR + static_cast<char>(request) + static_cast<char>(feature) )

std::string Telnet::InputProtocolParser::negotiate(uint8_t req, uint8_t feature, std::string subnegotiation) {
	switch (feature) {
	case FEATURE_Echo:
		switch (req) {
		case CODE_WILL:
			// No, please do not.  This will go VERY poorly.  You are incompatible.  Stop that..
			this->incompatible_client = true;
			return COMPOSE_WWDD(CODE_DONT, feature);
		case CODE_DO:   return COMPOSE_WWDD(CODE_WILL, feature); // Yeah, we'll do that.
		case CODE_DONT: return COMPOSE_WWDD(CODE_WILL, feature); // We refuse.  We're going to echo back to you.  Sorry.
		}
		break;
	case FEATURE_Suppress_Go_Ahead:
		switch (req) {
		case CODE_DO:   return COMPOSE_WWDD(CODE_WILL, feature); // Yeah, we'll do that.
		case CODE_DONT: return COMPOSE_WWDD(CODE_WONT, feature); // Uhh... Whatever you say.  Sending it was never mandatory anyway.
		}
		break;
	case FEATURE_Terminal_Type:
		// Kudos to http://mud-dev.wikidot.com/telnet:negotiation
		switch (req) {
		case CODE_WILL:
			if (this->remote_terminal_type.empty())
				return IAC_STR + CODE_SB + '\x01' /* SEND */ + CODE_IAC+CODE_SE; // Request next terminal type.
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
						return IAC_STR + CODE_SB + '\x01' /* SEND */ + CODE_IAC+CODE_SE; // Request next terminal type.
					}
				}
			}
		}
		break;
	}
	return ""; // Unsupported or no response required.
}

/**
 * Build an initial negotiation string for ourselevs.
 *
 * @return The initial negotiation string.
 */
std::string Telnet::InputProtocolParser::build_initial_negotiation() {
	std::string negotiation;
	negotiation += COMPOSE_WWDD(CODE_WILL, FEATURE_Echo); // We wish to echo back to you.
	negotiation += COMPOSE_WWDD(CODE_DONT, FEATURE_Echo); // We DON'T want YOU to echo back to us.  It would be... bad.
	negotiation += COMPOSE_WWDD(CODE_WILL, FEATURE_Suppress_Go_Ahead); // We don't send these either way.
	negotiation += COMPOSE_WWDD(CODE_DO,   FEATURE_Suppress_Go_Ahead); // We don't need these.
	negotiation += COMPOSE_WWDD(CODE_DO,   FEATURE_Terminal_Type); // Please change to ANSI or VT10[02] officially, if possible.  We're sending those codes anyway.
	return negotiation;
}
