namespace {

/// A "set_serial" console command.
class ConsoleCommand_set_serial: public CommandParser::Command {
public:
	virtual std::string get_helptext(const std::string &command) const {
		return stdsprintf(
				"%s $serial\n"
				"\n"
				"Set the IPMC serial number.\n"
				"NOTE: This cannot be changed once set!\n", command.c_str());
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		uint16_t serial;
		if (!parameters.parseParameters(1, true, &serial)) {
			console->write("Please provide a serial number.\n");
			return;
		}

		IPMC_SERIAL = serial;
		eeprom_mac->write(0, reinterpret_cast<uint8_t*>(&serial), sizeof(serial));
		console->write("Serial updated.  Reboot to lock.\n");
	}

	//virtual std::vector<std::string> complete(const CommandParser::CommandParameters &parameters) const { };
};

}
