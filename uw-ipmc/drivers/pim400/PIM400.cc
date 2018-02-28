/*
 * PIM400.cc
 *
 *  Created on: Feb 26, 2018
 *      Author: mpv
 */

#include <drivers/pim400/PIM400.h>
#include <services/console/CommandParser.h>
#include <services/console/ConsoleSvc.h>

/**
 * Initialize the PIM400 interface driver
 *
 * \param MasterI2C            The I2C interface to where the PIM400 is connected to.
 * \param Addr                 The PIM400 I2C address (does NOT require to be shifted!)
 */
PIM400::PIM400(I2C &MasterI2C, u8 Addr) :
i2c(MasterI2C), PimAddr(Addr>>1) {
	configASSERT(this->PimAddr > 0);
}

PIM400::~PIM400() {

}

/**
 * Read the hold-up voltage in 0.398V steps.
 */
float PIM400::read_holdup_voltage() {
	return ((this->read_int_reg(PIM400_V_HLDP)*1.0) * 0.398);
}

/**
 * Read the output current in 0.094A steps.
 */
float PIM400::read_out_current() {
	return ((this->read_int_reg(PIM400_NEG48V_IOUT)*1.0) * 0.094);
}

/**
 * Read the feed A voltage in 0.325V steps.
 */
float PIM400::read_feedA_voltage() {
	return ((this->read_int_reg(PIM400_NEG48V_AF)*1.0) * 0.325);
}

/**
 * Read the feed B voltage in 0.325V steps.
 */
float PIM400::read_feedB_voltage() {
	return ((this->read_int_reg(PIM400_NEG48V_BF)*1.0) * 0.325);
}

/**
 * Read the module temperature in 1.961C steps.
 */
float PIM400::read_temperature() {
	return ((this->read_int_reg(PIM400_TEMP)*1.0) * 1.961 - 50.0);
}

namespace {
	/// A "config" console command.
	class PIM400_status : public CommandParser::Command {
	public:
		PIM400 &pim400;

		/// Instantiate
		PIM400_status(PIM400 &pim400) : pim400(pim400) { };

		virtual std::string get_helptext(const std::string &command) const {
			return stdsprintf(
					"%s\n"
					"\n"
					"Do something.\n", command.c_str());
		}

		virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {

			int i = 0;

			while (1) {
				std::string str;

				str += stdsprintf("Attempt: %d\n", ++i);
				/*str += stdsprintf("Holdup voltage: %.2fV\n", pim400.read_holdup_voltage());
				str += stdsprintf("Out current: %.3fA\n", pim400.read_out_current());
				str += stdsprintf("Feed A voltage: %.2fV\n", pim400.read_feedA_voltage());
				str += stdsprintf("Feed B voltage: %.2fV\n", pim400.read_feedB_voltage());
				str += stdsprintf("Temperature: %.1fC\n\n", pim400.read_temperature());*/

				//str += stdsprintf("IOUT is %d", pim400.read_int_reg(PIM400::PIM400_NEG48V_IOUT));

				pim400.read_holdup_voltage();
				pim400.read_out_current();
				pim400.read_feedA_voltage();
				pim400.read_feedB_voltage();
				pim400.read_temperature();


				if (i % 1000 == 0)
					console->write(str);

				//vTaskDelay( 250 / portTICK_RATE_MS );
			}
		}

		//virtual std::vector<std::string> complete(const CommandParser::CommandParameters &parameters) const { };
	};
};

/**
 * Register console commands related to this storage.
 * @param parser The command parser to register to.
 * @param prefix A prefix for the registered commands.
 */
void PIM400::register_console_commands(CommandParser &parser, const std::string &prefix) {
	parser.register_command(prefix + ".status", std::make_shared<PIM400_status>(*this));
}

/**
 * Unregister console commands related to this storage.
 * @param parser The command parser to unregister from.
 * @param prefix A prefix for the registered commands.
 */
void PIM400::deregister_console_commands(CommandParser &parser, const std::string &prefix) {
	parser.register_command(prefix + ".status", NULL);
}

u8 PIM400::read_int_reg(const PIM400::PIM400_REGISTERS reg) {
	u8 a = reg;
	u8 r = 0xFF;

	this->i2c.write(this->PimAddr, &a, 1, 1000 / portTICK_RATE_MS);
	this->i2c.read(this->PimAddr, &r, 1, 1000 / portTICK_RATE_MS);

	return r;
}
