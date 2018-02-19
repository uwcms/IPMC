/*
 * Telnet.cpp
 *
 *  Created on: Feb 8, 2018
 *      Author: mpv
 */

#include <IPMC.h>
#include <FreeRTOS.h>
#include <task.h>

#include "Telnet.h"

#include <drivers/network/ServerSocket.h>
#include <services/console/TelnetConsoleSvc.h>
#include <services/persistentstorage/PersistentStorage.h>

#include <xilrsa.h>

#include <functional>

void _thread_telnetd(void *p) {
	reinterpret_cast<TelnetServer*>(p)->thread_telnetd();
}

TelnetServer::TelnetServer(LogTree &logtree)
	: logtree(logtree) {
	this->connection_pool_limiter = xSemaphoreCreateCounting(50,50);
	xTaskCreate(_thread_telnetd, "telnetd", UWIPMC_STANDARD_STACK_SIZE, this, TASK_PRIORITY_SERVICE, NULL);
}

TelnetServer::~TelnetServer() {
	configASSERT(0); // No task shutdown mechanism available
	vSemaphoreDelete(this->connection_pool_limiter);
}

void TelnetServer::thread_telnetd() {
	ServerSocket *server = new ServerSocket(23, 3);

	int err = server->listen();

	if (err != 0) {
		printf(strerror(err));
		vTaskDelete(NULL);
	}

	while (true) {
		// Wait for a free slot.
		xSemaphoreTake(this->connection_pool_limiter, portMAX_DELAY);
		Socket *client = server->accept();

		if (!client->valid()) {
			xSemaphoreGive(this->connection_pool_limiter); // Surrender the unused slot.
			delete client;
			continue;
		}

		// Launch a new telnet instance if client is valid

		TelnetClient *c = new TelnetClient(client, this->logtree, this->connection_pool_limiter);
	}

	vTaskDelete(NULL);
}

static void _thread_telnetc(void *p) {
	reinterpret_cast<TelnetClient*>(p)->thread_telnetc();
}

TelnetClient::TelnetClient(Socket *s, LogTree &logtree, SemaphoreHandle_t connection_pool_limiter)
	: socket(s), logtree(logtree), connection_pool_limiter(connection_pool_limiter) {
	configASSERT(s);
	configASSERT(connection_pool_limiter);
	taskENTER_CRITICAL();
	this->session_serial = this->next_session_serial++;
	taskEXIT_CRITICAL();
	xTaskCreate(_thread_telnetc, stdsprintf("telnetd.%x", this->session_serial).c_str(), UWIPMC_STANDARD_STACK_SIZE, this, TASK_PRIORITY_INTERACTIVE, NULL);
}

namespace {
/// A "logout" console command.
class ConsoleCommand_logout : public CommandParser::Command {
public:
	Telnet::TelnetConsoleSvc &console; ///< The associated telnet console for this command.

	/// Instantiate
	ConsoleCommand_logout(Telnet::TelnetConsoleSvc &console) : console(console) { };

	virtual std::string get_helptext(const std::string &command) const {
		return stdsprintf(
				"%s\n"
				"\n"
				"Disconnect from your telnet session.\n", command.c_str());
	}

	virtual void execute(ConsoleSvc &console, const CommandParser::CommandParameters &parameters) {
		/* We will be courteous and give telnet time to absorb window size query
		 * replies from the previous prompt before asking it to terminate.
		 */
		vTaskDelay(configTICK_RATE_HZ / 4);
		this->console.close();
	}

	// Nothing to complete.
	//virtual std::vector<std::string> complete(const CommandParser::CommandParameters &parameters) const { };
};
} // anonymous namespace

uint32_t TelnetClient::next_session_serial = 0;

static void telnet_shutdown_cleanup(Telnet::TelnetConsoleSvc &svc, CommandParser *parser, LogTree::Filter *log_filter, SemaphoreHandle_t connection_pool_limiter, SocketAddress addr) {
	svc.logtree.log(stdsprintf("Telnet connection from %s:%hu terminated", addr.get_address().c_str(), addr.get_port()), LogTree::LOG_INFO);
	delete log_filter;
	delete &svc.logtree;
	delete parser;
	xSemaphoreGive(connection_pool_limiter);
}

/**
 * Get the current bad password timeout delay.
 */
uint64_t TelnetClient::get_badpass_timeout() {
	taskENTER_CRITICAL(); // No atomic 64bit ops.
	uint64_t pressure = TelnetClient::bad_password_pressure;
	taskEXIT_CRITICAL();
	uint64_t critical_pressure = get_tick64() + 60*configTICK_RATE_HZ;
	if (pressure > critical_pressure)
		return pressure - critical_pressure;
	else
		return 0;
}

volatile uint64_t TelnetClient::bad_password_pressure = 0;

/**
 * Increment the current bad password timeout delay.
 */
void TelnetClient::inc_badpass_timeout() {
	taskENTER_CRITICAL(); // No atomic 64bit ops.
	const uint64_t now64 = get_tick64();
	if (TelnetClient::bad_password_pressure < now64)
		TelnetClient::bad_password_pressure = now64;
	TelnetClient::bad_password_pressure += 10*configTICK_RATE_HZ; // Push the pressure 10s into the future.
	taskEXIT_CRITICAL();
}

static void telnet_log_handler(ConsoleSvc &console, LogTree &logtree, const std::string &message, enum LogTree::LogLevel level) {
	std::string logmsg = ConsoleSvc_log_format(message, level);

	// We have to use a short timeout here, rather than none, due to the mutex involved.
	// TODO: Maybe there's a better way?
	console.write(logmsg, 1);
}

void TelnetClient::thread_telnetc() {
	AbsoluteTimeout unauthenticated_timeout(60 * configTICK_RATE_HZ);

	const SocketAddress &addr = this->socket->get_socketaddress();
	LogTree &log = this->logtree[stdsprintf("%s:%hu-%x", addr.get_address().c_str(), addr.get_port(), this->session_serial)];
	log.log(stdsprintf("Telnet connection received from %s:%hu", addr.get_address().c_str(), addr.get_port()), LogTree::LOG_INFO);

	uint64_t bptimeout = TelnetClient::get_badpass_timeout();
	if (bptimeout) {
		this->socket->send(stdsprintf("This service is currently unavailable for %u seconds due to excessive password failures.\r\n", bptimeout/configTICK_RATE_HZ), unauthenticated_timeout.get_timeout());
		log.log(stdsprintf("Telnet connection from %s:%hu rejected", addr.get_address().c_str(), addr.get_port()), LogTree::LOG_INFO);
		delete &log;
		delete this->socket;
		delete this;
		vTaskDelete(NULL);
	}

	this->socket->send(std::string("Password: "), unauthenticated_timeout.get_timeout());
	std::string pass;
	std::shared_ptr<Telnet::InputProtocolParser> proto = std::make_shared<Telnet::InputProtocolParser>();
	this->socket->send(proto->build_initial_negotiation(), unauthenticated_timeout.get_timeout());
	while (true) {
		char nextc;
		int rv = this->socket->read(&nextc, 1, unauthenticated_timeout.get_timeout());
		if (rv < 0) {
			log.log(stdsprintf("Telnet connection from %s:%hu broke", addr.get_address().c_str(), addr.get_port()), LogTree::LOG_INFO);
			xSemaphoreGive(this->connection_pool_limiter);
			delete &log;
			delete this->socket;
			delete this;
			break;
		}
		if (rv == 0)
			continue;
		size_t ccount = 1;
		std::string protoreply = proto->parse_input(&nextc, &ccount);
		this->socket->send(protoreply, unauthenticated_timeout.get_timeout());
		if (ccount == 0)
			continue;

		bptimeout = TelnetClient::get_badpass_timeout();
		if (bptimeout) {
			this->socket->send(stdsprintf("This service is currently unavailable for %u seconds due to excessive password failures.\r\n", bptimeout/configTICK_RATE_HZ), unauthenticated_timeout.get_timeout());
			log.log(stdsprintf("Telnet connection from %s:%hu rejected", addr.get_address().c_str(), addr.get_port()), LogTree::LOG_INFO);
			delete &log;
			delete this->socket;
			delete this;
			break;
		}

		if (pass.size() > 1024)
			nextc = '\r'; // Noone's password is this long, let's put a stop to it.

		if (nextc == '\r' || nextc == '\n') {
			// Password complete.

			uint16_t nv_secver = persistent_storage->get_section_version(PersistentStorageAllocations::WISC_NETWORK_CONSOLE_AUTH);
			configASSERT(nv_secver <= 1);
			unsigned char *nvhash = (unsigned char*)persistent_storage->get_section(PersistentStorageAllocations::WISC_NETWORK_CONSOLE_AUTH, 1, 256/8);
			if (nv_secver == 0) {
				// Default password is blank
				taskENTER_CRITICAL();
				// echo -n | sha26sum
				const char *default_pw = "\xe3\xb0\xc4\x42\x98\xfc\x1c\x14\x9a\xfb\xf4\xc8\x99\x6f\xb9\x24\x27\xae\x41\xe4\x64\x9b\x93\x4c\xa4\x95\x99\x1b\x78\x52\xb8\x55";
				for (int i = 0; i < 32; ++i)
					nvhash[i] = default_pw[i];
				taskEXIT_CRITICAL();
				persistent_storage->flush(nvhash, 32);
			}
			unsigned char pwhash[32];
			sha_256(reinterpret_cast<const unsigned char*>(pass.data()), pass.size(), pwhash);
			if (memcmp(pwhash, nvhash, 32) == 0) {
				log.log(stdsprintf("Telnet login successful from %s:%hu", addr.get_address().c_str(), addr.get_port()), LogTree::LOG_NOTICE);

				std::string banner = generate_banner();
				windows_newline(banner);
				(void)banner.c_str();
				vTaskDelay(1); // yield to get stack overflow checked.
				(void)stdsprintf("\r\n\r\n%s\r\n", banner.c_str());
				vTaskDelay(1); // yield to get stack overflow checked.
				this->socket->send(stdsprintf("\r\n\r\n%s\r\n", banner.c_str()));
				CommandParser *telnet_command_parser = new CommandParser(&console_command_parser);
				LogTree::Filter *log_filter = new LogTree::Filter(LOG, NULL, LogTree::LOG_NOTICE);
				log_filter->register_console_commands(*telnet_command_parser);
				Telnet::TelnetConsoleSvc *console = new Telnet::TelnetConsoleSvc(
						*this->socket,
						proto,
						*telnet_command_parser,
						stdsprintf("telnetd.%x", this->session_serial),
						log,
						true,
						4,
						std::bind(telnet_shutdown_cleanup, std::placeholders::_1, telnet_command_parser, log_filter, this->connection_pool_limiter, this->socket->get_socketaddress())
				);
				taskENTER_CRITICAL();
				log_filter->handler = std::bind(telnet_log_handler, std::ref(*console), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
				taskEXIT_CRITICAL();
				std::shared_ptr<ConsoleCommand_logout> logoutcmd = std::make_shared<ConsoleCommand_logout>(*console);
				telnet_command_parser->register_command("logout", logoutcmd);
				telnet_command_parser->register_command("exit", logoutcmd);
				console->start();

				/* Our work here is done.
				 *
				 * The TelnetConsoleSvc now owns and will clean up all created components.
				 *
				 * Terminate this pre-authentication thread.
				 */
				delete this;
				break;
			}
			else {
				TelnetClient::inc_badpass_timeout();
				this->socket->send(std::string("\r\nIncorrect password.  Goodbye.\r\n"), unauthenticated_timeout.get_timeout());
				log.log(stdsprintf("Incorrect password from %s:%hu", addr.get_address().c_str(), addr.get_port()), LogTree::LOG_NOTICE);
				delete &log;
				vTaskDelay(configTICK_RATE_HZ/10);
				xSemaphoreGive(this->connection_pool_limiter);
				delete this->socket;
				delete this;
				break;
			}
		}
		if (nextc == '\x7f') {
			// DEL (backspace).  We won't support arrow keys but we will support that.
			if (pass.size())
				pass.pop_back();
			continue;
		}
		pass += std::string(&nextc, 1);
	}
	vTaskDelete(NULL);
}

namespace {
/// A "setpass" console command.
class ConsoleCommand_setpass: public CommandParser::Command {
public:
	virtual std::string get_helptext(const std::string &command) const {
		return stdsprintf(
				"%s $sha256_hash\n"
				"\n"
				"Change network access password.\n"
				"\n"
				"Use `read PW; echo -n \"$PW\" | sha256sum` to generate a hash.\n", command.c_str());
	}

	virtual void execute(ConsoleSvc &console, const CommandParser::CommandParameters &parameters) {
		std::string hash;
		if (!parameters.parse_parameters(1, true, &hash)) {
			console.write("Invalid parameters, see help.\n");
			return;
		}
		if (hash.size() != 64) {
			console.write("Invalid password hash supplied, see help.\n");
			return;
		}

		unsigned char binhash[32];
		memset(binhash, 0, 32);
		for (int i = 0; i < 64; ++i) {
			unsigned char nibble = 255;
			if ('0' <= hash[i] && hash[i] <= '9')
				nibble = hash[i] - '0';
			else if ('A' <= hash[i] && hash[i] <= 'F')
				nibble = hash[i] - 'A' + 10;
			else if ('a' <= hash[i] && hash[i] <= 'f')
				nibble = hash[i] - 'a' + 10;
			else {
				console.write("Invalid password hash supplied, see help.\n");
				return;
			}
			binhash[i / 2] |= (i%2) ? (nibble & 0x0f) : (nibble << 4);
		}

		uint16_t nv_secver = persistent_storage->get_section_version(PersistentStorageAllocations::WISC_NETWORK_CONSOLE_AUTH);
		configASSERT(nv_secver <= 1);
		unsigned char *nvhash = (unsigned char*)persistent_storage->get_section(PersistentStorageAllocations::WISC_NETWORK_CONSOLE_AUTH, 1, 256/8);
		memcpy(nvhash, binhash, 32);
		persistent_storage->flush(nvhash, 32);
		console.write("Password updated.\n");
	}

	// Nothing to complete.
	//virtual std::vector<std::string> complete(const CommandParser::CommandParameters &parameters) const { };
};
} // anonymous namespace

void TelnetClient::register_console_commands(CommandParser &parser, const std::string &prefix) {
	parser.register_command(prefix + "setpass", std::make_shared<ConsoleCommand_setpass>());
}

void TelnetClient::deregister_console_commands(CommandParser &parser, const std::string &prefix) {
	parser.register_command(prefix + "setpass", NULL);
}
