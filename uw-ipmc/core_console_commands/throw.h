namespace {

/// A debug command for testing exception handling.
class ConsoleCommand_throw : public CommandParser::Command {
public:
	virtual std::string get_helptext(const std::string &command) const {
		return stdsprintf(
				"%s\n\n"
				"Throw and exception. Test command.\n", command.c_str());
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		//throw std::exception();
		throw 0;
	}
};

/// A debug command for testing backtrace handling.
class ConsoleCommand_trace : public CommandParser::Command {
public:
	virtual std::string get_helptext(const std::string &command) const {
		return stdsprintf(
				"%s\n\n"
				"Backtrace. Test command.\n", command.c_str());
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		BackTrace trace;
		trace.trace();

		console->write(trace.toString());

		//throw std::exception();

	}
};

}
