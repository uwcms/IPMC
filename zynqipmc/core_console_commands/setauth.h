namespace {

/// A "setauth" console command.
class ConsoleCommand_setauth: public CommandParser::Command {
public:
	virtual std::string get_helptext(const std::string &command) const {
		return stdsprintf(
				"%s $username $password\n"
				"\n"
				"Change network access username and password.\n", command.c_str());
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		std::string user, pass;
		if (!parameters.parseParameters(1, true, &user, &pass)) {
			console->write("Invalid parameters, see help.\n");
			return;
		}
		Auth::changeCredentials(user, pass);
		console->write("Password updated.\n");
	}

	//virtual std::vector<std::string> complete(const CommandParser::CommandParameters &parameters) const { };
};

}
