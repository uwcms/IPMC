namespace {

/// A temporary "backend_power" command
class ConsoleCommand_backend_power : public CommandParser::Command {
public:
	virtual std::string get_helptext(const std::string &command) const {
		return stdsprintf(
				"%s [(on|off)]\n"
				"\n"
				"Enable/Disable MZs\n"
				"Without parameters, returns power status.\n", command.c_str());
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		if (parameters.nargs() == 1) {
			std::string out;
			bool transitioning;
			bool enabled = mgmt_zones[1]->get_power_state(&transitioning);
			if (transitioning)
				out += stdsprintf("ELM power status is (transitioning to) %s\n", (enabled?"on":"off"));
			else
				out += stdsprintf("ELM power status is %s\n", (enabled?"on":"off"));
			out += "\n";
			uint32_t pen_state = mgmt_zones[1]->get_pen_status(false);
			out += stdsprintf("The power enables are currently at 0x%08x\n", pen_state);
			console->write(out);
			return;
		}

		std::string action;
		if (!parameters.parse_parameters(1, true, &action)) {
			console->write("Invalid parameters.\n");
			return;
		}
		if (action == "on") {
			mgmt_zones[1]->set_power_state(MGMT_Zone::ON);
		}
		else if (action == "off") {
			mgmt_zones[1]->set_power_state(MGMT_Zone::OFF);
		}
		else {
			console->write("Unknown action.\n");
			return;
		}
	}

	//virtual std::vector<std::string> complete(const CommandParser::CommandParameters &parameters) const { };
};

}
