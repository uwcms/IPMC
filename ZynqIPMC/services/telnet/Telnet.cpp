/*
 * Telnet.cpp
 *
 *  Created on: Feb 8, 2018
 *      Author: mpv
 */

#include <IPMC.h>
#include <Core.h>
#include <FreeRTOS.h>
#include <task.h>

#include "Telnet.h"

#include <drivers/network/ServerSocket.h>
#include <libs/authentication.h>
#include <services/console/TelnetConsoleSvc.h>
#include <services/persistentstorage/PersistentStorage.h>
#include <libs/ThreadingPrimitives.h>
#include <libs/printf.h>
#include <functional>

// TODO

// TODO: If timeouts in sockets are required then use SO_RCVTIMEO and SO_SNDTIMEO

void _thread_telnetd(void *p) {
	reinterpret_cast<TelnetServer*>(p)->thread_telnetd();
}

TelnetServer::TelnetServer(LogTree &logtree)
	: logtree(logtree) {
	this->connection_pool_limiter = xSemaphoreCreateCounting(50,50);
	xTaskCreate(_thread_telnetd, "telnetd", ZYNQIPMC_BASE_STACK_SIZE, this, TASK_PRIORITY_SERVICE, NULL);
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
		std::shared_ptr<Socket> client = server->accept();

		if (!client->isValid()) {
			xSemaphoreGive(this->connection_pool_limiter); // Surrender the unused slot.
			continue;
		}

		client->enableNoDelay();

		// Launch a new telnet instance if client is valid

		TelnetClient *c = new TelnetClient(client, this->logtree, this->connection_pool_limiter);
		if (c == nullptr) {
			throw std::runtime_error("Failed to create Telnet client instance");
		}
	}

	vTaskDelete(NULL);
}

static void _thread_telnetc(void *p) {
	reinterpret_cast<TelnetClient*>(p)->thread_telnetc();
}

TelnetClient::TelnetClient(std::shared_ptr<Socket> s, LogTree &logtree, SemaphoreHandle_t connection_pool_limiter)
	: socket(s), logtree(logtree), connection_pool_limiter(connection_pool_limiter) {
	if (!s)
		throw std::runtime_error("A valid socket must be supplied.");
	configASSERT(connection_pool_limiter);
	CriticalGuard critical(true);
	this->session_serial = this->next_session_serial++;
	critical.release();
	xTaskCreate(_thread_telnetc, stdsprintf("telnetd.%lx", this->session_serial).c_str(), ZYNQIPMC_BASE_STACK_SIZE, this, TASK_PRIORITY_INTERACTIVE, NULL);
}

namespace {
/// A "logout" console command.
class ConsoleCommand_logout : public CommandParser::Command {
public:
	std::shared_ptr<Telnet::TelnetConsoleSvc> console; ///< The associated telnet console for this command.

	/// Instantiate
	ConsoleCommand_logout(std::shared_ptr<Telnet::TelnetConsoleSvc> console) : console(console) { };

	virtual std::string get_helptext(const std::string &command) const {
		return stdsprintf(
				"%s\n"
				"\n"
				"Disconnect from your telnet session.\n", command.c_str());
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		/* We will be courteous and give telnet time to absorb window size query
		 * replies from the previous prompt before asking it to terminate.
		 */
		vTaskDelay(configTICK_RATE_HZ / 4);
		this->console->close();
	}

	// Nothing to complete.
	//virtual std::vector<std::string> complete(const CommandParser::CommandParameters &parameters) const { };
};
} // anonymous namespace

uint32_t TelnetClient::next_session_serial = 0;

static void telnet_shutdown_cleanup(Telnet::TelnetConsoleSvc &svc, CommandParser *parser, LogTree::Filter *log_filter, SemaphoreHandle_t connection_pool_limiter, SocketAddress addr) {
	svc.logtree.log(stdsprintf("Telnet connection from %s:%hu terminated", addr.getAddress().c_str(), addr.getPort()), LogTree::LOG_INFO);
	delete log_filter;
	delete &svc.logtree;
	delete parser;
	xSemaphoreGive(connection_pool_limiter);
}

/**
 * Get the current bad password timeout delay.
 */
uint64_t TelnetClient::get_badpass_timeout() {
	CriticalGuard critical(true); // No atomic 64bit ops.
	uint64_t pressure = TelnetClient::bad_password_pressure;
	critical.release();
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
	CriticalGuard critical(true); // No atomic 64bit ops.
	const uint64_t now64 = get_tick64();
	if (TelnetClient::bad_password_pressure < now64)
		TelnetClient::bad_password_pressure = now64;
	TelnetClient::bad_password_pressure += 10*configTICK_RATE_HZ; // Push the pressure 10s into the future.
}

static void telnet_log_handler(std::shared_ptr<ConsoleSvc> console, LogTree &logtree, const std::string &message, enum LogTree::LogLevel level) {
	std::string logmsg = ConsoleSvc_log_format(message, level);

	// We have to use a short timeout here, rather than none, due to the mutex involved.
	// TODO: Maybe there's a better way?
	console->write(logmsg, 1);
}

void TelnetClient::thread_telnetc() {
	//AbsoluteTimeout unauthenticated_timeout(60 * configTICK_RATE_HZ);

	const SocketAddress &addr = this->socket->getSocketAddress();
	LogTree &log = this->logtree[stdsprintf("%s:%hu-%lx", addr.getAddress().c_str(), addr.getPort(), this->session_serial)];
	log.log(stdsprintf("Telnet connection received from %s:%hu", addr.getAddress().c_str(), addr.getPort()), LogTree::LOG_INFO);

	uint64_t bptimeout = TelnetClient::get_badpass_timeout();
	if (bptimeout) {
		this->socket->send(stdsprintf("This service is currently unavailable for %llu seconds due to excessive password failures.\r\n", bptimeout/configTICK_RATE_HZ));
		log.log(stdsprintf("Telnet connection from %s:%hu rejected", addr.getAddress().c_str(), addr.getPort()), LogTree::LOG_INFO);
		delete &log;
		delete this;
		vTaskDelete(NULL);
	}

	this->socket->send("Password: ");
	std::string pass;
	std::shared_ptr<Telnet::InputProtocolParser> proto = std::make_shared<Telnet::InputProtocolParser>();
	this->socket->send(proto->build_initial_negotiation());
	while (true) {
		char nextc;
		int rv = 0;
		try {
			rv = this->socket->recv(&nextc, 1, 60000); // Timeout of 60 seconds
		} catch (const Socket::Timeout& e) {
			// We waited too long for input
			log.log(stdsprintf("Telnet connection from %s:%hu timed out", addr.getAddress().c_str(), addr.getPort()), LogTree::LOG_INFO);
			xSemaphoreGive(this->connection_pool_limiter);
			delete &log;
			break;
		}
		if (rv < 0) {
			log.log(stdsprintf("Telnet connection from %s:%hu broke", addr.getAddress().c_str(), addr.getPort()), LogTree::LOG_INFO);
			xSemaphoreGive(this->connection_pool_limiter);
			delete &log;
			break;
		}
		if (rv == 0)
			continue;
		size_t ccount = 1;
		std::string protoreply = proto->parse_input(&nextc, &ccount);
		this->socket->send(protoreply);
		if (ccount == 0)
			continue;

		bptimeout = TelnetClient::get_badpass_timeout();
		if (bptimeout) {
			this->socket->send(stdsprintf("This service is currently unavailable for %llu seconds due to excessive password failures.\r\n", bptimeout/configTICK_RATE_HZ));
			log.log(stdsprintf("Telnet connection from %s:%hu rejected", addr.getAddress().c_str(), addr.getPort()), LogTree::LOG_INFO);
			delete &log;
			break;
		}

		if (pass.size() > 1024)
			nextc = '\r'; // Noone's password is this long, let's put a stop to it.

		if (nextc == '\r' || nextc == '\n') {
			// Password complete.

			if (Auth::validateCredentials(pass)) {
				log.log(stdsprintf("Telnet login successful from %s:%hu", addr.getAddress().c_str(), addr.getPort()), LogTree::LOG_NOTICE);

				std::string banner = generate_banner();
				windows_newline(banner);
				(void)banner.c_str();
				vTaskDelay(1); // yield to get stack overflow checked.
				(void)stdsprintf("\r\n\r\n%s\r\n", banner.c_str());
				vTaskDelay(1); // yield to get stack overflow checked.
				this->socket->send(stdsprintf("\r\n\r\n%s\r\n", banner.c_str()));
				CommandParser *telnet_command_parser = new CommandParser(&console_command_parser);
				LogTree::Filter *log_filter = new LogTree::Filter(LOG, NULL, LogTree::LOG_NOTICE);
				log_filter->registerConsoleCommands(*telnet_command_parser);
				std::shared_ptr<Telnet::TelnetConsoleSvc> console = Telnet::TelnetConsoleSvc::create(
						this->socket,
						proto,
						*telnet_command_parser,
						stdsprintf("telnetd.%lx", this->session_serial),
						log,
						true,
						4,
						std::bind(telnet_shutdown_cleanup, std::placeholders::_1, telnet_command_parser, log_filter, this->connection_pool_limiter, this->socket->getSocketAddress())
				);
				CriticalGuard critical(true);
				log_filter->handler = std::bind(telnet_log_handler, console, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
				critical.release();
				std::shared_ptr<ConsoleCommand_logout> logoutcmd = std::make_shared<ConsoleCommand_logout>(console);
				telnet_command_parser->register_command("logout", logoutcmd);
				telnet_command_parser->register_command("exit", logoutcmd);
				console->start();

				/* Our work here is done.
				 *
				 * The TelnetConsoleSvc now owns and will clean up all created components.
				 *
				 * Terminate this pre-authentication thread.
				 */
				break;
			}
			else {
				TelnetClient::inc_badpass_timeout();
				this->socket->send(std::string("\r\nIncorrect password.  Goodbye.\r\n"));
				log.log(stdsprintf("Incorrect password from %s:%hu", addr.getAddress().c_str(), addr.getPort()), LogTree::LOG_NOTICE);
				delete &log;
				vTaskDelay(configTICK_RATE_HZ/10);
				xSemaphoreGive(this->connection_pool_limiter);
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

	// Terminate
	delete this;
	vTaskDelete(NULL);
}

