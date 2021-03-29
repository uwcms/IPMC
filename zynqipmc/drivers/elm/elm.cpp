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

// TODO: Remove printf

#include "elm.h"
#include <core.h>
#include <libs/threading.h>
#include <services/timer/timer.h>
#include "elmlink_protocol.h"

class ELM::LinkIndexChannel: public Channel {
public:
	LinkIndexChannel(ELM &elm, std::map<uint8_t, std::string> &channel_index);
	virtual ~LinkIndexChannel() {
		if (this->startup_refresh_timer)
			this->startup_refresh_timer->cancel(true);
	};
	void sendUpdate();
protected:
	virtual void recv(const uint8_t *content, size_t size);

	std::map<uint8_t, std::string> &channel_index;
	std::shared_ptr<TimerService::Timer> startup_refresh_timer;
};

ELM::LinkIndexChannel::LinkIndexChannel(ELM &elm,
		std::map<uint8_t, std::string> &channel_index) :
		Channel(elm, "link_index"), channel_index(channel_index) {

	/* We will wait 100 ticks for all channels to be registered, to prevent race
	 * conditions on the ELM side that break active clients which should remain
	 * valid across an IPMC reboot, but haven't had their channels registered
	 * yet.
	 */
	this->startup_refresh_timer = std::make_shared<TimerService::Timer>([this]()->void{
		this->startup_refresh_timer = NULL;
		this->sendUpdate();
	}, (TickType_t)100);
	TimerService::globalTimer(TASK_PRIORITY_SERVICE).submit(this->startup_refresh_timer);
}

void ELM::LinkIndexChannel::recv(const uint8_t *content, size_t size) {
	if (std::string((const char*) content, size) == "INDEX_REQUEST")
		this->sendUpdate();
}

void ELM::LinkIndexChannel::sendUpdate() {
	if (this->startup_refresh_timer) {
		/* Too early, sorry.  We're still waiting for any required channel
		 * registrations.  We'll get to you soon.
		 */
		return;
	}
	this->send(ELMLink::Packet::encode_channel_index_update_packet(this->channel_index));
}

ELM::ELM(UART &uart, GPIO *targetsel) :
uart(uart), targetsel(targetsel) {
	this->mutex = xSemaphoreCreateRecursiveMutex();
	configASSERT(this->mutex);

	this->link_index_channel = new LinkIndexChannel(*this, this->channel_index);

	// Start the digest thread
	runTask("elmlink", TASK_PRIORITY_SERVICE,
			[this]() -> void {this->recvThread();});
}

ELM::~ELM() {
	configASSERT(0); // Not supported, no way to kill thread.
	delete this->link_index_channel;
	vSemaphoreDelete(this->mutex);
}

void ELM::recvThread() {
	std::string recvbuf;
	uint8_t *uart_raw_recvbuf = (uint8_t*) malloc(
			ELMLink::MAX_SERIALIZED_PACKET_LENGTH);
	while (true) {
		/* Wait for data.
		 *
		 * We're waiting only 2 ticks when there's data available, because at
		 * 115200 baud, that's 230.4 bytes.  More than enough to have gotten a
		 * full, if smaller, packet.  At the same time, we don't want to delay
		 * such packets for terribly long.
		 */
		int rv = this->uart.read(uart_raw_recvbuf,
				ELMLink::MAX_SERIALIZED_PACKET_LENGTH, portMAX_DELAY, 2);
		if (rv <= 0)
			continue;

		recvbuf.append((char *) uart_raw_recvbuf, rv);

		ELMLink::Packet packet;
		while (packet.digest(recvbuf)) {
			MutexGuard<true> lock(this->mutex);
			if (this->channels.count(packet.channel)) {
				// Channel exists
				this->channels[packet.channel]->recv(
						(const uint8_t*) packet.data.data(),
						packet.data.size());
			} else {
				printf("Packet to unmapped ELMLink channel (%d)", packet.channel);
			}
		}

	}
}

class ELM::BootSource : public CommandParser::Command {
public:
	BootSource(ELM &elm) : elm(elm) { };

	virtual std::string getHelpText(const std::string &command) const {
		return command + " [release|sdcard|flash]\n\n"
				"Overrides the ELM boot source.\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		if (parameters.nargs() == 1) {
			if ((elm.targetsel->getBusDirection() & 0x03) == 0x03) {
				console->write("ELM override is disabled.\n");
			} else {
				if ((elm.targetsel->getBusValue() & 0x03) == 0x02) {
					console->write("ELM override set to sdcard.\n");
				} else {
					console->write("ELM override set to flash.\n");
				}
			}
		} else {
			if (!parameters.parameters[1].compare("release")) {
				// Set pins as inputs
				elm.targetsel->setBusDirection(0x3);
			} else if (!parameters.parameters[1].compare("sdcard")) {
				elm.targetsel->setBusValue(0x2);
				elm.targetsel->setBusDirection(0x0);
			} else if (!parameters.parameters[1].compare("flash")) {
				elm.targetsel->setBusValue(0x0);
				elm.targetsel->setBusDirection(0x0);
			} else {
				console->write("Invalid source, see help.\n");
			}
		}
	}

private:
	ELM &elm;
};

void ELM::registerConsoleCommands(CommandParser &parser, const std::string &prefix) {
	if (this->targetsel)
		parser.registerCommand(prefix + "bootsource", std::make_shared<ELM::BootSource>(*this));
}

void ELM::deregisterConsoleCommands(CommandParser &parser, const std::string &prefix) {
	if (this->targetsel)
		parser.registerCommand(prefix + "bootsource", nullptr);
}

// TODO: Proper timeouts

bool ELM::sendPacket(uint8_t channel, const std::string &data) {
	std::string serialized = ELMLink::Packet(channel, data).serialize();
	// Send message down the link
	MutexGuard<true> lock(this->mutex);
	while (true) {
		size_t rv = this->uart.write((const uint8_t*) serialized.data(),
				serialized.size());
		if (rv == serialized.size())
			return true;
		else
			serialized.erase(0, rv);
	}
}

void ELM::linkChannel(ELM::Channel *c, std::string channel_name) {
	MutexGuard<true> lock(this->mutex);

	for (int i = 0; i < 0x80; ++i) {
		if (this->channels.count(i))
			continue;
		this->channels[i] = c;
		c->channel_id = i;
		this->channel_index[i] = channel_name;
		this->link_index_channel->sendUpdate();
		return;
	}
	throw std::out_of_range("There are no further channel ids to allocate on this ELMLink.");
}

void ELM::unlinkChannel(ELM::Channel *c) {
	MutexGuard<true> lock(this->mutex);

	if (this->channels.count(c->channel_id))
		this->channels.erase(c->channel_id);
	if (this->channel_index.count(c->channel_id))
		this->channel_index.erase(c->channel_id);
	this->link_index_channel->sendUpdate();
}
