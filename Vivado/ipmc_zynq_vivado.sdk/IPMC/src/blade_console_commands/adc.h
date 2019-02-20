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

		struct ChannelEntry {
			std::string name;
			std::string unit;
			int intf;
			int slave;
			ADC::Channel adc;
		};

		ADC::Channel::Callback tmp36 = [](float r) -> float  { return (r - 0.5) * 100.0; };
		ADC::Channel::Callback imon = [](float r) -> float { return r * 1000.0 / 80.0; };

		std::vector< ChannelEntry > channels = {
			{"+12VPYLD      ", "Volt", 0, 0, ADC::Channel(*adc[0], 0, 5.640)},
			{"+5VPYLD       ", "Volt", 0, 0, ADC::Channel(*adc[0], 1, 2.467)},
			{"+3.55VBULK    ", "Volt", 0, 0, ADC::Channel(*adc[0], 2, 1.749)},
			{"+3.3VMP2      ", "Volt", 0, 0, ADC::Channel(*adc[0], 3, 1.649)},
			{"+3.3VFFLY5    ", "Volt", 0, 0, ADC::Channel(*adc[0], 4, 1.649)},
			{"+3.3VFFLY4    ", "Volt", 0, 0, ADC::Channel(*adc[0], 5, 1.649)},
			{"+3.3VFFLY3    ", "Volt", 0, 0, ADC::Channel(*adc[0], 6, 1.649)},
			{"+3.3VFFLY2    ", "Volt", 0, 0, ADC::Channel(*adc[0], 7, 1.649)},

			{"+3.3VFFLY1    ", "Volt", 1, 0, ADC::Channel(*adc[1], 7, 1.649)},
			{"+3.3VDD       ", "Volt", 1, 0, ADC::Channel(*adc[1], 6, 1.649)},
			{"+2.5VXPT      ", "Volt", 1, 0, ADC::Channel(*adc[1], 5, 1.299)},
			{"+1.95VBULK    ", "Volt", 1, 0, ADC::Channel(*adc[1], 4)},
			{"+1.8VFFLY2    ", "Volt", 1, 0, ADC::Channel(*adc[1], 3)},
			{"+1.8VFFLY1    ", "Volt", 1, 0, ADC::Channel(*adc[1], 2)},
			{"+1.8VDD       ", "Volt", 1, 0, ADC::Channel(*adc[1], 1)},

			{"+0.9VMGTT     ", "Volt", 2, 0, ADC::Channel(*adc[2], 0)},
			{"+0.9VMGTB     ", "Volt", 2, 0, ADC::Channel(*adc[2], 1)},
			{"+0.85VDD      ", "Volt", 2, 0, ADC::Channel(*adc[2], 2)},
			{"MGT0.5VT_IMON ", "Ampere", 2, 0, ADC::Channel(*adc[2], 3, imon)},
			{"MGT0.9VB_IMON ", "Ampere", 2, 0, ADC::Channel(*adc[2], 4, imon)},
			{"MGT1.2VT_IMON ", "Ampere", 2, 0, ADC::Channel(*adc[2], 5, imon)},
			{"MGT1.2VB_IMON ", "Ampere", 2, 0, ADC::Channel(*adc[2], 6, imon)},
			{"T_BOARD1      ", "Celsius", 2, 0, ADC::Channel(*adc[2], 7, tmp36)},

			{"+1.35VMGTT    ", "Volt", 2, 1, ADC::Channel(*adc[3], 0)},
			{"+1.35VMGTB    ", "Volt", 2, 1, ADC::Channel(*adc[3], 1)},
			{"+1.2VPHY      ", "Volt", 2, 1, ADC::Channel(*adc[3], 2)},
			{"+1.2VMGTT     ", "Volt", 2, 1, ADC::Channel(*adc[3], 3)},
			{"+1.2VMGTB     ", "Volt", 2, 1, ADC::Channel(*adc[3], 4)},
			{"VLUTVDDIO     ", "Volt", 2, 1, ADC::Channel(*adc[3], 5)},
			{"+1.05VMGTT    ", "Volt", 2, 1, ADC::Channel(*adc[3], 6)},
			{"+1.05VMGTB    ", "Volt", 2, 1, ADC::Channel(*adc[3], 7)},

			{"+1.8VFFLY3    ", "Volt", 2, 2, ADC::Channel(*adc[4], 0)},
			{"+1.8VFFLY4    ", "Volt", 2, 2, ADC::Channel(*adc[4], 1)},
			{"+1.8VFFLY5    ", "Volt", 2, 2, ADC::Channel(*adc[4], 2)},
			{"T_BOARD2      ", "Celsius", 2, 2, ADC::Channel(*adc[4], 3, tmp36)},
		};

		console->write(stdsprintf("Zynq Temp: %0.2f Celsius\n", xadc->getTemperature()));

		for (int i = 0; i < 5; i++) {
			console->write(stdsprintf("ADC %i Temp: %0.2f Celsius\n", i, adc[i]->getTemperature()));
		}

		for (auto &channel : channels) {
			console->write(std::to_string(channel.intf) + "/" + std::to_string(channel.slave) + "/" + std::to_string(channel.adc.getChannel()) + " " + channel.name + " " + stdsprintf("%6.3f", (float)channel.adc) + " " + channel.unit + "\n");
		}
	}
};

}
