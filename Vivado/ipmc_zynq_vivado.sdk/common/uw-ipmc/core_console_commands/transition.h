#include <exception>

namespace {

/// A debugging command to transition between M-states. (Hotswap Sensor Only!)
class ConsoleCommand_transition : public CommandParser::Command {
public:
	virtual std::string get_helptext(const std::string &command) const {
		return stdsprintf(
				"%s $new_mstate $reason\n\n"
				"Transitions to the specified M-state.\n", command.c_str());
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		CommandParser::CommandParameters::xint8_t mstate, reason;
		if (!parameters.parse_parameters(1, true, &mstate, &reason)) {
			console->write("Invalid arguments, see help.\n");
			return;
		}
		std::shared_ptr<HotswapSensor> hotswap = std::dynamic_pointer_cast<HotswapSensor>(ipmc_sensors.find_by_name("Hotswap"));
		if (!hotswap)
			throw std::out_of_range("No sensor named \"Hotswap\" found!");
		hotswap->transition(mstate, static_cast<HotswapSensor::StateTransitionReason>(static_cast<uint8_t>(reason)));
	}
};

}
