/*
 * PIM400.cc
 *
 *  Created on: Feb 26, 2018
 *      Author: mpv
 */

#include <drivers/pim400/PIM400.h>
#include <libs/printf.h>
#include <services/console/CommandParser.h>
#include <services/console/ConsoleSvc.h>

PIM400::PIM400(I2C &i2c, uint8_t i2c_addr) :
i2c(i2c), i2c_addr(i2c_addr>>1) {
	configASSERT(this->i2c_addr > 0);
}

PIM400::~PIM400() {
}

float PIM400::read_holdup_voltage() {
	return ((this->read_int_reg(PIM400_V_HLDP)*1.0) * 0.398);
}

float PIM400::read_out_current() {
	return ((this->read_int_reg(PIM400_NEG48V_IOUT)*1.0) * 0.094);
}

float PIM400::read_feedA_voltage() {
	return ((this->read_int_reg(PIM400_NEG48V_AF)*1.0) * 0.325);
}

float PIM400::read_feedB_voltage() {
	return ((this->read_int_reg(PIM400_NEG48V_BF)*1.0) * 0.325);
}

float PIM400::read_temperature() {
	return ((this->read_int_reg(PIM400_TEMP)*1.0) * 1.961 - 50.0);
}

PIM400::pim400status PIM400::read_status() {
	PIM400::pim400status r;
	r._raw = this->read_int_reg(PIM400_STATUS);
	return r;
}

namespace {
/// A "read" console command.
	class PIM400_read : public CommandParser::Command {
	public:
		PIM400 &pim400;

		/// Instantiate
		PIM400_read(PIM400 &pim400) : pim400(pim400) { };

		virtual std::string get_helptext(const std::string &command) const {
			return stdsprintf(
					"%s\n"
					"\n"
					"Read the PIM400 status.\n", command.c_str());
		}

		virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
			float holdup = pim400.read_holdup_voltage();
			float current = pim400.read_out_current();
			float feeda = pim400.read_feedA_voltage();
			float feedb = pim400.read_feedB_voltage();
			float temp = pim400.read_temperature();
			PIM400::pim400status status = pim400.read_status();

			std::string str;
			str += stdsprintf("Holdup voltage: %.2fV\n", holdup);
			str += stdsprintf("Out current: %.3fA\n", current);
			str += stdsprintf("Feed A voltage: %.2fV\n", feeda);
			str += stdsprintf("Feed B voltage: %.2fV\n", feedb);
			str += stdsprintf("Temperature: %.1fC\n", temp);
			str += stdsprintf("Status register:\n");
			str += stdsprintf("   ENABLE_AF is %s\n", (status.enable_af?"enabled":"disabled"));
			str += stdsprintf("   ENABLE_BF is %s\n", (status.enable_bf?"enabled":"disabled"));
			str += stdsprintf("   Alarm %s set\n", (status.alarm_set?"is":"not"));
			str += stdsprintf("   C_HLDP %s connected\n", (status.hdlp_connected?"is":"not"));
			str += stdsprintf("   Hotswap switch is %s\n", (status.hotswap_on?"on":"off"));
			str += stdsprintf("   -48V_OUT is %s threshold\n", (status.out_volt_undervoltage?"above":"below"));
			console->write(str);
		}
	};


	/*/// A "loop" console command.
	class PIM400_loop : public CommandParser::Command {
	public:
		PIM400 &pim400;

		/// Instantiate
		PIM400_loop(PIM400 &pim400) : pim400(pim400) { };

		virtual std::string get_helptext(const std::string &command) const {
			return stdsprintf(
					"%s\n"
					"\n"
					"Do something.\n", command.c_str());
		}

		virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
#if FALSE
			xTaskCreate([](void *p) {
				int i = 0;
				PIM400 *pim400 = (PIM400*)p;

				//uint64_t start = get_tick64();
				TickType_t start = xTaskGetTickCount();

				while (1) {

					++i;
					float holdup = pim400->read_holdup_voltage();
					float current = pim400->read_out_current();
					float feeda = pim400->read_feedA_voltage();
					float feedb = pim400->read_feedB_voltage();
					float temp = pim400->read_temperature();

					if (i % 250 == 0) {
						//uint64_t end = get_tick64();
						//uint64_t delta = end - start;
						TickType_t end = xTaskGetTickCount();
						TickType_t delta = end - start;

						std::string str;

						str += stdsprintf("Attempt: %d\n", ++i);
						str += stdsprintf("Holdup voltage: %.2fV\n", holdup);
						str += stdsprintf("Out current: %.3fA\n", current);
						str += stdsprintf("Feed A voltage: %.2fV\n", feeda);
						str += stdsprintf("Feed B voltage: %.2fV\n", feedb);
						str += stdsprintf("Temperature: %.1fC\n", temp);
						str += stdsprintf("Ticks: %d\n", delta);
						str += stdsprintf("Time per op: %.3fms\n", (1.0 * delta) / 250.0 / 5.0 / 2.0);
						printf(str.c_str());

						//start = get_tick64();
						start = xTaskGetTickCount();
					}

					//vTaskDelay( 250 / portTICK_RATE_MS );
				}
			}, "pim400", UWIPMC_STANDARD_STACK_SIZE, &pim400, TASK_PRIORITY_BACKGROUND, NULL);
#else
			int i = 0;
			TickType_t start = xTaskGetTickCount();

			while (1) {

				++i;
				float holdup = pim400.read_holdup_voltage();
				float current = pim400.read_out_current();
				float feeda = pim400.read_feedA_voltage();
				float feedb = pim400.read_feedB_voltage();
				float temp = pim400.read_temperature();
				PIM400::pim400status status = pim400.read_status();

				if (i % 250 == 0) {
					TickType_t end = xTaskGetTickCount();
					TickType_t delta = end - start;

					std::string str;

					str += stdsprintf("Attempt: %d\n", ++i);
					str += stdsprintf("Holdup voltage: %.2fV\n", holdup);
					str += stdsprintf("Out current: %.3fA\n", current);
					str += stdsprintf("Feed A voltage: %.2fV\n", feeda);
					str += stdsprintf("Feed B voltage: %.2fV\n", feedb);
					str += stdsprintf("Temperature: %.1fC\n\n", temp);
					str += stdsprintf("Ticks: %d\n", delta);
					str += stdsprintf("Time per op: %.3fms\n", (1.0 * delta) / 250.0 / 6.0 / 2.0);
					console->write(str);

					start = xTaskGetTickCount();
				}

			}
#endif
		}
	};*/
};

u8 PIM400::read_int_reg(const PIM400::PIM400_REGISTERS reg) {
	u8 a = reg;
	u8 r = 0xFF;

	this->i2c.chain([this, &a, &r]() -> void {
		this->i2c.write(this->i2c_addr, &a, 1, 1000 / portTICK_RATE_MS);
		this->i2c.read(this->i2c_addr, &r, 1, 1000 / portTICK_RATE_MS);
	});

	return r;
}

/**
 * Register console commands related to this storage.
 * @param parser The command parser to register to.
 * @param prefix A prefix for the registered commands.
 */
void PIM400::register_console_commands(CommandParser &parser, const std::string &prefix) {
	parser.register_command(prefix + ".read", std::make_shared<PIM400_read>(*this));
	//parser.register_command(prefix + ".loop", std::make_shared<PIM400_loop>(*this));
}

/**
 * Unregister console commands related to this storage.
 * @param parser The command parser to unregister from.
 * @param prefix A prefix for the registered commands.
 */
void PIM400::deregister_console_commands(CommandParser &parser, const std::string &prefix) {
	parser.register_command(prefix + ".read", NULL);
	//parser.register_command(prefix + ".loop", NULL);
}

