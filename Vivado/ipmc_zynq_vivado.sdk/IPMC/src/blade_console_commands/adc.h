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

		const std::string RED = ANSICode::color(ANSICode::RED, ANSICode::NOCOLOR, true);
		const std::string YELLOW = ANSICode::color(ANSICode::YELLOW, ANSICode::NOCOLOR, true);
		const std::string NORMAL = ANSICode::color();

		std::map<char, std::string> uname = {{'v', "Volts"}, {'c', "Celsius"}, {'i', "Ampere"}};

		struct ChannelEntry {
			const char* name;
			char unit;
			float nominal;
			float tolerance;
			int intf;
			int slave;
			ADC::Channel adc;
		};

		ADC::Channel::Callback tmp36   = [](float r) -> float  { return (r - 0.5) * 100.0; };
		ADC::Channel::Callback tmp36_r = [](float r) -> float  { return (r/100.0) + 0.5; };
		ADC::Channel::Callback imon    = [](float r) -> float { return r * 1000.0 / 80.0; };
		ADC::Channel::Callback imon_r  = [](float r) -> float { return r / 1000.0 * 80.0; };

		ChannelEntry channels[] = {
			{"+12VPYLD      ", 'v', 12.00, 0.05, 0, 0, ADC::Channel(*adc[0], 0, 5.640)},
			{"+5VPYLD       ", 'v',  5.00, 0.05, 0, 0, ADC::Channel(*adc[0], 1, 2.467)},
			{"+3.3VDD       ", 'v',  3.30, 0.05, 1, 0, ADC::Channel(*adc[1], 6, 1.649)},
			{"+1.8VDD       ", 'v',  1.80, 0.05, 1, 0, ADC::Channel(*adc[1], 1)},
			{"+3.3VMP2      ", 'v',  3.30, 0.05, 0, 0, ADC::Channel(*adc[0], 3, 1.649)},

			{"+2.5VXPT      ", 'v',  2.50, 0.05, 1, 0, ADC::Channel(*adc[1], 5, 1.299)},
			{"+1.2VPHY      ", 'v',  1.20, 0.05, 2, 1, ADC::Channel(*adc[3], 2)},
			{"VLUTVDDIO     ", 'v',  1.20, 0.05, 2, 1, ADC::Channel(*adc[3], 5)},

			{"+3.55VBULK    ", 'v',  3.55, 0.05, 0, 0, ADC::Channel(*adc[0], 2, 1.749)},
			{"+1.95VBULK    ", 'v',  1.95, 0.05, 1, 0, ADC::Channel(*adc[1], 4)},
			{"+0.85VDD      ", 'v',  0.85, 0.01, 2, 0, ADC::Channel(*adc[2], 2)},

			{"+1.35VMGTT    ", 'v',  1.35, 0.01, 2, 1, ADC::Channel(*adc[3], 0)},
			{"+1.35VMGTB    ", 'v',  1.35, 0.01, 2, 1, ADC::Channel(*adc[3], 1)},
			{"+1.2VMGTT     ", 'v',  1.20, 0.01, 2, 1, ADC::Channel(*adc[3], 3)},
			{"+1.2VMGTB     ", 'v',  1.20, 0.01, 2, 1, ADC::Channel(*adc[3], 4)},
			{"+1.05VMGTT    ", 'v',  1.05, 0.01, 2, 1, ADC::Channel(*adc[3], 6)},
			{"+1.05VMGTB    ", 'v',  1.05, 0.01, 2, 1, ADC::Channel(*adc[3], 7)},
			{"+0.9VMGTT     ", 'v',  0.90, 0.01, 2, 0, ADC::Channel(*adc[2], 0)},
			{"+0.9VMGTB     ", 'v',  0.90, 0.01, 2, 0, ADC::Channel(*adc[2], 1)},

			{"MGT0.9VT_IMON ", 'i', 10.00, 0.00, 2, 0, ADC::Channel(*adc[2], 3, imon, imon_r)},
			{"MGT0.9VB_IMON ", 'i', 10.00, 0.00, 2, 0, ADC::Channel(*adc[2], 4, imon, imon_r)},
			{"MGT1.2VT_IMON ", 'i', 10.00, 0.00, 2, 0, ADC::Channel(*adc[2], 6, imon, imon_r)},
			{"MGT1.2VB_IMON ", 'i', 10.00, 0.00, 2, 0, ADC::Channel(*adc[2], 5, imon, imon_r)},

			{"+3.3VFFLY1    ", 'v',  3.30, 0.05, 1, 0, ADC::Channel(*adc[1], 7, 1.649)},
			{"+3.3VFFLY2    ", 'v',  3.30, 0.05, 0, 0, ADC::Channel(*adc[0], 7, 1.649)},
			{"+3.3VFFLY3    ", 'v',  3.30, 0.05, 0, 0, ADC::Channel(*adc[0], 6, 1.649)},
			{"+3.3VFFLY4    ", 'v',  3.30, 0.05, 0, 0, ADC::Channel(*adc[0], 5, 1.649)},
			{"+3.3VFFLY5    ", 'v',  3.30, 0.05, 0, 0, ADC::Channel(*adc[0], 4, 1.649)},
			{"+1.8VFFLY1    ", 'v',  1.80, 0.05, 1, 0, ADC::Channel(*adc[1], 2)},
			{"+1.8VFFLY2    ", 'v',  1.80, 0.05, 1, 0, ADC::Channel(*adc[1], 3)},
			{"+1.8VFFLY3    ", 'v',  1.80, 0.05, 2, 2, ADC::Channel(*adc[4], 0)},
			{"+1.8VFFLY4    ", 'v',  1.80, 0.05, 2, 2, ADC::Channel(*adc[4], 1)},
			{"+1.8VFFLY5    ", 'v',  1.80, 0.05, 2, 2, ADC::Channel(*adc[4], 2)},

			{"T_BOARD1      ", 'c', 50.00, 0.00, 2, 0, ADC::Channel(*adc[2], 7, tmp36, tmp36_r)},
			{"T_BOARD2      ", 'c', 50.00, 0.00, 2, 2, ADC::Channel(*adc[4], 3, tmp36, tmp36_r)},
		};

		console->write(stdsprintf("Zynq Temp: %0.2f Celsius\n", xadc->getTemperature()));

		for (int i = 0; i < 5; i++) {
			console->write(stdsprintf("ADC %i Temp: %0.2f Celsius\n", i, adc[i]->getTemperature()));
		}

		for (auto &channel : channels) {
			float value = (float)(channel.adc);
			uint16_t raw = (uint16_t)(channel.adc);

			std::string color = NORMAL;
			switch (channel.unit) {
			case 'v':
				if ((value > (channel.nominal * (1.00 + channel.tolerance))) ||
					(value < (channel.nominal * (1.00 - channel.tolerance)))) {
					color = RED;
				}
				break;

			case 'c':
			case 'i':
				if (value > channel.nominal) {
					color = RED;
				}
				break;

			default:
				color = YELLOW;
				break;
			}

			console->write(color + std::to_string(channel.intf) + "/" + std::to_string(channel.slave) + "/" + std::to_string(channel.adc.getChannel()) + " " + channel.name + " " + stdsprintf("%6.3f", value) + " " + uname[channel.unit] + " " + stdsprintf("(0x%04x)", raw) + NORMAL + "\n");
		}
	}
};

}
