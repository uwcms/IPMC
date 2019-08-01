/*
 * This file is part of the ZYNQ-IPMC Framework.
 *
 * The ZYNQ-IPMC Framework is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The ZYNQ-IPMC Framework is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the ZYNQ-IPMC Framework.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <payload_manager.h>
#include <drivers/ad7689/ad7689.h>
#include <libs/ansi_code/ansi_code.h>
#include <libs/printf.h>
#include <vector>

namespace CoreCommands {

/// A "adc" console command.
class AdcCommand : public CommandParser::Command {
public:
	virtual std::string getHelpText(const std::string &command) const {
		return command + "\n\n"
				"Print the values of all ADC channels on the IPMC.\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {

		const std::string RED = ANSICode::color(ANSICode::RED, ANSICode::NOCOLOR, true);
		const std::string YELLOW = ANSICode::color(ANSICode::YELLOW, ANSICode::NOCOLOR, true);
		const std::string NORMAL = ANSICode::color();

		for (auto const &sensor : PayloadManager::getADCSensors()) {
			uint32_t raw = sensor.second.adc.readRaw();
			float value = sensor.second.adc.rawToFloat(raw);

			console->write(stdsprintf("%10s\t%4.3f\t%8s\t%d\n",
					sensor.first.c_str(), value, sensor.second.adc.getADC().getIdentifier().c_str(), sensor.second.adc.getChannelNumber()));

			//console->write(sensor.first + " " + std::to_string(value) + " " + sensor.second.adc.getChannelInfo() + "\n");
		}

//
//		std::map<char, std::string> uname = {{'v', "Volts"}, {'c', "Celsius"}, {'i', "Ampere"}};
//
//		struct ChannelEntry {
//			const char* name;
//			char unit;
//			float nominal;
//			float tolerance;
//			int intf;
//			int slave;
//			ADC::Channel adc;
//		};
//
//		ADC::Channel::Callback tmp36   = [](float r) -> float  { return (r - 0.5) * 100.0; };
//		ADC::Channel::Callback tmp36_r = [](float r) -> float  { return (r/100.0) + 0.5; };
//
//		ChannelEntry channels[] = {
//				{"+12VPYLD      ", 'v', 12.00, 0.05, 0, 0, ADC::Channel(*adc[0], 0, 5.640)},
//				{"+5VPYLD       ", 'v',  5.00, 0.05, 0, 0, ADC::Channel(*adc[0], 4, 2.400)},
//				{"+3.3PYLD      ", 'v',  3.30, 0.05, 0, 0, ADC::Channel(*adc[0], 6, 1.600)},
//				{"+3.3MP        ", 'v',  3.30, 0.05, 0, 0, ADC::Channel(*adc[0], 7, 1.600)},
//
//				{"T_BOTTOM      ", 'c', 50.00, 0.00, 1, 0, ADC::Channel(*adc[1], 0, tmp36, tmp36_r)},
//				{"+1.0ETH       ", 'v',  1.00, 0.05, 1, 0, ADC::Channel(*adc[1], 2)},
//				{"+2.5ETH       ", 'v',  2.50, 0.05, 1, 0, ADC::Channel(*adc[1], 4, 1.216)},
//				{"+1.2PHY       ", 'v',  1.20, 0.05, 1, 0, ADC::Channel(*adc[1], 5)},
//				{"T_TOP         ", 'c', 50.00, 0.00, 1, 0, ADC::Channel(*adc[1], 7, tmp36, tmp36_r)},
//		};
//
//		console->write(stdsprintf("Zynq Temp: %0.2f Celsius\n", xadc->getTemperature()));
//
//		for (int i = 0; i < 2; i++) {
//			console->write(stdsprintf("ADC %i Temp: %0.2f Celsius\n", i, adc[i]->getTemperature()));
//		}
//
//		for (auto &channel : channels) {
//			float value = channel.adc.readFloat();
//			uint16_t raw = channel.adc.readRaw();
//
//			std::string color = NORMAL;
//			switch (channel.unit) {
//			case 'v':
//				if ((value > (channel.nominal * (1.00 + channel.tolerance))) ||
//					(value < (channel.nominal * (1.00 - channel.tolerance)))) {
//					color = RED;
//				}
//				break;
//
//			case 'c':
//			case 'i':
//				if (value > channel.nominal) {
//					color = RED;
//				}
//				break;
//
//			default:
//				color = YELLOW;
//				break;
//			}
//
//			console->write(color + channel.adc.getChannelInfo() + " " + stdsprintf("%6.3f", value) + " " + uname[channel.unit] + " " + stdsprintf("(0x%04x)", raw) + NORMAL + "\n");
////			console->write(color + std::to_string(channel.intf) + "/" + std::to_string(channel.slave) + "/" + std::to_string(channel.adc.getChannelNumber()) + " " + channel.name + " " + stdsprintf("%6.3f", value) + " " + uname[channel.unit] + " " + stdsprintf("(0x%04x)", raw) + NORMAL + "\n");
//		}
	}
};

}
