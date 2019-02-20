namespace {

/// A "version" console command.
class ConsoleCommand_dac : public CommandParser::Command {
public:
	virtual std::string get_helptext(const std::string &command) const {
		return stdsprintf(
				"%s <channel: A|B|C|D> <command: W|U|S|D> [val]\n\n"
				"Operate the DAC, check LTC2654 datasheet. Mid-point is 0x7ff.\n"
				"Example usage: %s B W 0x7ff\n", command.c_str(), command.c_str());
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		if (parameters.nargs() < 3 || parameters.nargs() > 4) {
			console->write("Invalid arguments, see help.\n");
			return;
		}

		std::string arg = "";
		if (!parameters.parse_parameters(1, true, &arg) && arg.length() != 1) {
			console->write("Invalid DAC channel, see help.\n");
			return;
		}

		LTC2654F::Address addr = LTC2654F::ALL_DACS;
		switch (arg[0]) {
		case 'A': addr = LTC2654F::DAC_A; break;
		case 'B': addr = LTC2654F::DAC_B; break;
		case 'C': addr = LTC2654F::DAC_C; break;
		case 'D': addr = LTC2654F::DAC_D; break;
		default:
			console->write("Invalid DAC channel, see help.\n");
			return;
		}

		if (!parameters.parse_parameters(2, true, &arg) && arg.length() != 1) {
			console->write("Invalid command, see help.\n");
			return;
		}

		LTC2654F::Command cmd = LTC2654F::NO_OP;
		switch (arg[0]) {
		case 'W': cmd = LTC2654F::WRITE_INPUT_REG; break;
		case 'U': cmd = LTC2654F::UPDATE_DAC_REG; break;
		case 'S': cmd = LTC2654F::WRITE_AND_UPDATE_REG; break;
		case 'D': cmd = LTC2654F::POWER_DOWN; break;
		default:
			console->write("Invalid command, see help.\n");
			return;
		}

		uint32_t val = 0;
		if (cmd == LTC2654F::WRITE_INPUT_REG || cmd == LTC2654F::WRITE_AND_UPDATE_REG) {
			if (parameters.nargs() != 4) {
				console->write("Value is required to write, see help.\n");
				return;
			}

			// Not a double
			if (!parameters.parse_parameters(3, true, &arg)) {
				console->write("Invalid value parameter, see help.\n");
				return;
			}

			if (!toUint32(arg, val)) {
				console->write("Value is neither 0x (hex) or b (binary), see help.\n");
				return;
			}

			if (val > 0x0fff) {
				console->write("Value is higher then 2^12, see help.\n");
				return;
			}
		}

		dac->sendCommand(addr, cmd, val);
	}
};

}
