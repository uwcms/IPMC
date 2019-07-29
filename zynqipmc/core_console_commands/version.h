namespace {

/// A "version" console command.
class ConsoleCommand_version : public CommandParser::Command {
public:
	virtual std::string get_helptext(const std::string &command) const {
		return stdsprintf(
				"%s\n"
				"\n"
				"Print the current system version information.\n", command.c_str());
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		console->write(generateBanner());
	}

	//virtual std::vector<std::string> complete(const CommandParser::CommandParameters &parameters) const { };
};

}
