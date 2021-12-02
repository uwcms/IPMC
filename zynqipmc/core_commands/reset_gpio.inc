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

namespace CoreCommands
{

class ResetGPIOCommand final : public CommandParser::Command {
public:
	ResetGPIOCommand(std::string descriptor, GPIO &gpio, uint32_t reset_pin, bool active_high = false, bool neutral_z = true, TickType_t duration = 1000)
	    : gpio(gpio), pin(reset_pin), active_high(active_high), neutral_z(neutral_z), duration(duration), descriptor(descriptor) {
		configASSERT(this->mutex = xSemaphoreCreateMutex());
	};
	virtual ~ResetGPIOCommand() {
		vSemaphoreDelete(this->mutex);
	}

	virtual std::string getHelpText(const std::string &command) const {
		return command + " [hold]\n"
		                 "Reset the " +
		       this->descriptor + "\n"
		                          "And optionally hold it in reset.\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		MutexGuard<false> lock();
		console->write(stdsprintf("Asserting the %s reset for %lums.\n", this->descriptor.c_str(), (this->duration * 1000) / configTICK_RATE_HZ));
		this->gpio.setBusMask(active_high ? 1 : 0, 1 << this->pin);
		this->gpio.setPinToOutput(this->pin);

		vTaskDelay(this->duration);
		if (parameters.nargs() == 2 && parameters.parameters[1] == "hold") {
			console->write(stdsprintf("Leaving the %s reset asserted.\n", this->descriptor.c_str()));
		}
		else {
			if (this->neutral_z)
				this->gpio.setPinToInput(this->pin);
			else
				this->gpio.setBusMask(active_high ? 0 : 1, 1 << this->pin);
			console->write("Done.\n");
		}
	}

private:
	SemaphoreHandle_t mutex; ///< A mutex to ensure only one simultaneous reset is in progress.
	GPIO &gpio;              ///< The GPIO IP controlling the reset line.
	uint32_t pin;            ///< The pin controlling the reset line.
	bool active_high;        ///< The active level of the reset line.
	bool neutral_z;          ///< Whether the neutral level is Z rather than !active.
	TickType_t duration;     ///< The duration to hold this reset line active.
	std::string descriptor;  ///< The name of the peripheral controlled by this reset line.
};

} // namespace CoreCommands
