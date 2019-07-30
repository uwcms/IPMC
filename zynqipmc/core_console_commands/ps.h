#include <algorithm>

namespace {

/// A "ps" console command.
class ConsoleCommand_ps : public CommandParser::Command {
public:
	virtual std::string get_helptext(const std::string &command) const {
		return stdsprintf(
				"%s\n"
				"\n"
				"Print the system process listing & statistics.\n", command.c_str());
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		console->write(this->get_ps_string());
	}

	static std::string get_ps_string() {
		std::string out;
		UBaseType_t task_count = uxTaskGetNumberOfTasks();
		TaskStatus_t taskinfo[task_count+2];
		UBaseType_t total_runtime;
		if (!uxTaskGetSystemState(taskinfo, task_count+2, &total_runtime)) {
			return "Failed to generate process listing.\n";
		}

		// Runtime stats are accurate only if they havent rolled over.  It seems to be a tad under 666 per tick.
		bool runstats = get_tick64() < portMAX_DELAY/666;

		std::vector<TaskStatus_t> tasks;
		tasks.reserve(task_count);
		for (UBaseType_t i = 0; i < task_count; ++i)
			tasks.push_back(taskinfo[i]);
		if (runstats)
			std::sort(tasks.begin(), tasks.end(), [](const TaskStatus_t &a, const TaskStatus_t &b) -> bool { return a.ulRunTimeCounter > b.ulRunTimeCounter; });
		else
			std::sort(tasks.begin(), tasks.end(), [](const TaskStatus_t &a, const TaskStatus_t &b) -> bool { return (a.uxCurrentPriority > b.uxCurrentPriority) || (a.uxCurrentPriority == b.uxCurrentPriority && a.xTaskNumber < b.xTaskNumber); });

		out += "PID Name             BasePrio CurPrio StackHW State";
		if (runstats)
			out += " CPU% CPU";
		out += "\n";
		for (auto it = tasks.begin(), eit = tasks.end(); it != eit; ++it) {
			if (it->eCurrentState < 0 || it->eCurrentState > eInvalid)
				it->eCurrentState = eInvalid;
			static const char *taskstates[] = {
				"*Running*",
				"Ready",
				"Blocked",
				"Suspended",
				"Deleted",
				"Invalid"
			};
			out += stdsprintf("%3lu %-16s %8lu %7lu %7hu %5c",
					it->xTaskNumber,
					it->pcTaskName,
					it->uxBasePriority,
					it->uxCurrentPriority,
					it->usStackHighWaterMark,
					taskstates[it->eCurrentState][0]);
			if (runstats) {
				UBaseType_t cpu_percent = it->ulRunTimeCounter / (total_runtime/100);
				if (it->ulRunTimeCounter && cpu_percent < 1)
					out += stdsprintf("  <1%% %lu", it->ulRunTimeCounter);
				else
					out += stdsprintf("  %2lu%% %lu", cpu_percent, it->ulRunTimeCounter);
			}
			out += "\n";
		}
		if (!runstats)
			out += "\nNote: Runtime stats were not displayed, as we are likely past the point\nof counter wrapping and they are no longer accurate.\n";
		return out;
	}

	//virtual std::vector<std::string> complete(const CommandParser::CommandParameters &parameters) const { };
};

}
