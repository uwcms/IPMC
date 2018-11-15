namespace {

/// A temporary "restart" command
class ConsoleCommand_restart : public CommandParser::Command {
public:
	virtual std::string get_helptext(const std::string &command) const {
		return stdsprintf(
				"%s\n"
				"\n"
				"Do a complete restart to the IPMC, loading firmware and software from flash.\n", command.c_str());
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		// See section 26.2.3 in UG585 - System Software Reset

		if (firmwareUpdateFailed == false) {
			console->write("Flushing persistent storage...\n");
			SemaphoreHandle_t psdone = xSemaphoreCreateBinary();
			persistent_storage->flush([&psdone]() -> void { xSemaphoreGive(psdone); });
			xSemaphoreTake(psdone, portMAX_DELAY);
			vSemaphoreDelete(psdone);

			console->write("Restarting...\n");
			vTaskDelay(pdMS_TO_TICKS(100));

			volatile uint32_t* slcr_unlock_reg = (uint32_t*)(0xF8000000 + 0x008); // SLCR is at 0xF8000000
			volatile uint32_t* pss_rst_ctrl_reg = (uint32_t*)(0xF8000000 + 0x200);

			*slcr_unlock_reg = 0xDF0D; // Unlock key
			*pss_rst_ctrl_reg = 1;
		} else {
			console->write("Restart is disabled due to a failed firmware attempt (flash may be corrupted).\n");
		}
	}

	//virtual std::vector<std::string> complete(const CommandParser::CommandParameters &parameters) const { };
};

}
