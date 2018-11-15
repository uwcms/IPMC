namespace {

/// A "date" console command.
class ConsoleCommand_date : public CommandParser::Command {
public:
	virtual std::string get_helptext(const std::string &command) const {
		return stdsprintf(
				"%s\n"
				"\n"
				"Print the current system time. Updated by SNTP and kept by FreeRTOS.\n", command.c_str());
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		timeval tv;
		gettimeofday(&tv, NULL);
		if (tv.tv_sec == 0) {
			console->write("Time information unavailable.\n");
		} else {
			struct tm *timeinfo = localtime(&tv.tv_sec);
			console->write(std::string(asctime(timeinfo))); // GMT
		}
	}
};

}
