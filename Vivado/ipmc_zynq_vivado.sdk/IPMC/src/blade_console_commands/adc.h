#include <libs/ANSICode.h>
#include <map>

namespace {

/// A "version" console command.
class ConsoleCommand_adc : public CommandParser::Command {
public:
	virtual std::string get_helptext(const std::string &command) const {
		return stdsprintf(
				"%s\n\n"
				"Print the values of all ADC channels on the IPMC.\n", command.c_str());
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		console->write(stdsprintf("Zynq Temp: %0.2f Celsius\n", xadc->getTemperature()));

		for (int i = 0; i < 2; i++) {
			console->write(stdsprintf("ADC %i Temp: %0.2f Celsius\n", i, adc[i]->getTemperature()));
		}

		for (int i = 0; i < 2; i++) {
			for (int k = 0; k < 8; k++) {
				console->write(stdsprintf("%d/0/%d %0.3fV (0x%04x)\n", i, k, adc[i]->getReading(k), adc[i]->getRawReading(k)));
			}
		}

	}
};

}
