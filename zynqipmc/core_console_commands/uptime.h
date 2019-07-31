namespace {

/// A "uptime" console command.
class ConsoleCommand_uptime : public CommandParser::Command {
public:
	virtual std::string get_helptext(const std::string &command) const {
		return stdsprintf(
				"%s\n"
				"\n"
				"Print the current system uptime.\n", command.c_str());
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		uint64_t now64 = get_tick64();
		//uint16_t ms = now64 % 1000;
		uint16_t s  = (now64 / 1000) % 60;
		uint16_t m  = (now64 / (60*1000)) % 60;
		uint16_t h  = (now64 / (60*60*1000)) % 24;
		uint32_t d  = (now64 / (24*60*60*1000));
		std::string out = "Up for ";
		if (d)
			out += stdsprintf("%lud", d);
		if (d||h)
			out += stdsprintf("%huh", h);
		if (d||h||m)
			out += stdsprintf("%hum", m);
		out += stdsprintf("%hus", s);
		console->write(out + "\n");
	}

	//virtual std::vector<std::string> complete(const CommandParser::CommandParameters &parameters) const { };
};

}
