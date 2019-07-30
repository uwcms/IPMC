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

#ifndef SRC_COMMON_ZYNQIPMC_DRIVERS_GENERICS_GPIO_H_
#define SRC_COMMON_ZYNQIPMC_DRIVERS_GENERICS_GPIO_H_

#include <libs/printf.h>
#include <services/console/command_parser.h>
#include <services/console/consolesvc.h>
#include <zynqipmc_config.h>

/**
 * Abstract class for GPIOs with base functions for bus or individual pin operation declared.
 */
class GPIO : public ConsoleCommandSupport {
public:
	//! Get whole bus direction. Bits set as 1 correspond to inputs.
	virtual uint32_t getBusDirection() const = 0;

	//! Set direction of all pins in the bus. Bits set as 1 correspond to inputs.
	virtual void setBusDirection(uint32_t dir) = 0;

	/**
	 * Individually set the direction of pin in the bus.
	 * @param pin Pin number (from 0 to 31).
	 * @param input true if pin should become input, false if output.
	 */
	virtual void setPinDirection(uint32_t pin, bool input) = 0;

	//! Set a single pin to input.
	inline void setPinToInput(uint32_t pin) {
		this->setPinDirection(pin, true);
	}

	//! Set a single pin to output.
	inline void setPinToOutput(uint32_t pin) {
		this->setPinDirection(pin, false);
	}

	//! Get the current bus value.
	virtual uint32_t getBusValue() const = 0;

	//! Set the bus value. Only pins set as outputs will change.
	virtual void setBusValue(uint32_t value) = 0;

	/**
	 * Set the value of the bus while masking a set of bits.
	 * @param value Value to apply where each bit of the bus.
	 * @param mask Bits that will be masked from changing.
	 */
	inline void setBusMask(uint32_t value, uint32_t mask) {
		//MutexGuard<false>(this->mutex);
		uint32_t c = this->getBusValue();
		c = (c & ~mask) | (value & mask);
		this->setBusValue(c);
	}

	//! Set a single pin to low. Pin must be configured as output for changes to take effect.
	virtual void clearPin(uint32_t pin) = 0;

	//! Set a single pin to high. Pin must be configured as output for changes to take effect.
	virtual void setPin(uint32_t pin) = 0;

	//! Check if a single pin is set in the bus.
	inline bool isPinSet(uint32_t pin) const {
		return (this->getBusValue() >> pin) & 0x1;
	};

#ifdef ENABLE_DRIVER_COMMAND_SUPPORT
	//! Allows to change the bus direction from the console.
	class Direction final : public CommandParser::Command {
	public:
		Direction(GPIO &gpio) : gpio(gpio) {};

		virtual std::string get_helptext(const std::string &command) const {
			return command + " [$new_value]\n\n"
					"Retrieve or set the direction bit array of GPIO module. Bits set as 0 are inputs, outputs are set as 1.\n";
		}

		virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
			if (parameters.nargs() == 2) {
				uint32_t value = 0;
				if (!parameters.parseParameters(1, true, &value)) {
					console->write("Invalid arguments, see help.\n");
					return;
				}

				gpio.setBusDirection(value);
			} else {
				uint32_t value = gpio.getBusDirection();
				console->write(stdsprintf("0x%08lx\n", value));
			}
		}

	private:
		GPIO &gpio;
	};

	//! Allows to read the bus from the console.
	class Read final : public CommandParser::Command {
	public:
		Read(GPIO &gpio) : gpio(gpio) {};

		virtual std::string get_helptext(const std::string &command) const {
			return command + "\n\n"
					"Reads the current value of the GPIO module input pins.\n";
		}

		virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
			uint32_t value = gpio.getBusValue();
			console->write(stdsprintf("0x%08lx\n", value));
		}

	private:
		GPIO &gpio;
	};

	//! Allows to write the bus from the console.
	class Write final : public CommandParser::Command {
	public:
		Write(GPIO &gpio) : gpio(gpio) {};

		virtual std::string get_helptext(const std::string &command) const {
			return command + " $new_value\n\n"
					"Set the output value of pins set as outputs\n";
		}

		virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
			uint32_t value = 0;
			if (!parameters.parseParameters(1, true, &value)) {
				console->write("Invalid arguments, see help.\n");
				return;
			}

			gpio.setBusValue(value);
		}

	private:
		GPIO &gpio;
	};

	virtual void registerConsoleCommands(CommandParser &parser, const std::string &prefix="") {
		parser.registerCommand(prefix + "direction", std::make_shared<GPIO::Direction>(*this));
		parser.registerCommand(prefix + "read", std::make_shared<GPIO::Read>(*this));
		parser.registerCommand(prefix + "write", std::make_shared<GPIO::Write>(*this));
	}

	virtual void deregisterConsoleCommands(CommandParser &parser, const std::string &prefix="") {
		parser.registerCommand(prefix + "direction", NULL);
		parser.registerCommand(prefix + "read", NULL);
		parser.registerCommand(prefix + "write", NULL);
	}
#endif
};

/**
 * Generic reset pin class which implement the reset pin interface.
 */
class ResetPin {
public:
	virtual void release() = 0;					///< Release the pin, putting it in tri-state.
	virtual void assert() = 0;					///< Active the reset pin.
	virtual void deassert() = 0;				///< Deactivate the reset pin.
	virtual void toggle(uint32_t ms = 100) = 0;	///< Toggle (assert->pause->deassert) with a pre-defined time in between. Time is defined in milliseconds.
};

/**
 * Utility class to define individual negative asserted reset pins from a single GPIO pin.
 */
class NegResetPin : public ResetPin {
public:
	NegResetPin(GPIO &gpio, uint32_t pin) : gpio(gpio), pin(pin) {
		// Default value should be configured in the IP.
	};

	inline void release() { this->gpio.setPinToInput(this->pin); }
	inline void assert() { this->gpio.setPinToOutput(this->pin); this->gpio.clearPin(this->pin); }
	inline void deassert() { this->gpio.setPinToOutput(this->pin); this->gpio.setPin(this->pin); }
	inline void toggle(uint32_t ms) { this->assert(); vTaskDelay(pdMS_TO_TICKS(ms)); this->deassert(); }

private:
	GPIO &gpio;		///< Associated GPIO reference.
	uint32_t pin;	///< Associated pin number.
};

#endif /* SRC_COMMON_ZYNQIPMC_DRIVERS_GENERICS_GPIO_H_ */
