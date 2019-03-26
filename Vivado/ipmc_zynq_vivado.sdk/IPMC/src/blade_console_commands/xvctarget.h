/*
 * xvctarget.h
 *
 *  Created on: Mar 4, 2019
 *      Author: mpv
 */

#ifndef SRC_BLADE_CONSOLE_COMMANDS_XVCTARGET_H_
#define SRC_BLADE_CONSOLE_COMMANDS_XVCTARGET_H_

namespace {

/// A "xvctarget" console command.
class ConsoleCommand_xvctarget : public CommandParser::Command {
public:
	PL_GPIO *gpio; ///< GPIO IP with pins.
	PL_GPIO::Channel channel;
	uint32_t pins[2]; ///< Pin 0 & 1 for JTAG address

	///! Construct the command.
	ConsoleCommand_xvctarget(PL_GPIO *gpio, PL_GPIO::Channel channel, uint32_t addr0_pin, uint32_t addr1_pin)
	: gpio(gpio), channel(channel) {
		pins[0] = addr0_pin;
		pins[1] = addr1_pin;
	};

	virtual std::string get_helptext(const std::string &command) const {
		return command + " [disconnect|elm|fpga]\n\n"
				"Set the XVC target on the APd, priorities may vary depending on front-panel switch position.\n"
				"With no arguments will show the current connectivity.\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		if (parameters.nargs() == 1) {
			uint32_t value = this->gpio->getChannel(this->channel);
			uint32_t masked = value & ((1 << this->pins[0]) | (1 << this->pins[1]));

			if (masked == (1 << this->pins[0])) {
				console->write("XVC target set to ELM.\n");
			} else if (masked == (1 << this->pins[1])) {
				console->write("XVC target set to FPGA.\n");
			} else {
				console->write("XVC target is disconnected.\n");
			}
			return;

		} else {
			uint32_t mask = (1 << this->pins[0]) | (1 << this->pins[1]);

			if (!parameters.parameters[1].compare("disconnect")) {
				// Disconnect (A0 = 1, A1 = 1)
				uint32_t value = (1 << this->pins[0]) | (1 << this->pins[1]);
				this->gpio->setChannelMask(value, mask, this->channel);
			} else if (!parameters.parameters[1].compare("elm")) {
				// ELM  (A0 = 1, A1 = 0)
				uint32_t value = (1 << this->pins[0]);
				this->gpio->setChannelMask(value, mask, this->channel);
			} else if (!parameters.parameters[1].compare("fpga")) {
				// FPGA (A0 = 0, A1 = 1)
				uint32_t value = (1 << this->pins[1]);
				this->gpio->setChannelMask(value, mask, this->channel);
			} else {
				console->write("Unknown target, see help.\n");
			}
		}
	}
};

}

#endif /* SRC_BLADE_CONSOLE_COMMANDS_XVCTARGET_H_ */
