namespace {

/// A temporary "restart" command
class ConsoleCommand_restart : public CommandParser::Command {
public:
	virtual std::string get_helptext(const std::string &command) const {
		return stdsprintf(
				"%s [$image]\n"
				"\n"
				"Do a complete restart to the IPMC, loading firmware and software from flash.\n"
				"A target image to reboot to can be picked by selecting fallback, A, B, or test.\n", command.c_str());
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		// See section 26.2.3 in UG585 - System Software Reset

#define SLCR_UNLOCK_REG (XPS_SYS_CTRL_BASEADDR + 0x008)
#define PSS_RST_CTRL_REG (XPS_SYS_CTRL_BASEADDR + 0x200)
#define REBOOT_STATUS_REG (XPS_SYS_CTRL_BASEADDR + 0x258)

		if (parameters.nargs() > 1) {
			uint32_t target = 0;

			if (!parameters.parameters[1].compare("fallback")) {
				target = 0;
			} else if (!parameters.parameters[1].compare("A")) {
				target = 1;
			} else if (!parameters.parameters[1].compare("B")) {
				target = 2;
			} else if (!parameters.parameters[1].compare("test")) {
				target = 0x04;
			} else {
				console->write("Unknown image name, see help.\n");
				return;
			}

			target |= 0x08;

			// Force boot
			uint32_t reboot_status = Xil_In32(REBOOT_STATUS_REG);
			reboot_status &= ~(0x0F000000);
			reboot_status |= ((target << 24) & 0x0F000000);
			console->write(stdsprintf("Setting REBOOT_STATUS to 0x%08x.\n", reboot_status));

			Xil_Out32(SLCR_UNLOCK_REG, 0xDF0D); // Unlock key
			Xil_Out32(REBOOT_STATUS_REG, reboot_status);
		}

		if (wasFlashUpgradeSuccessful == true) {
			console->write("Flushing persistent storage...\n");
			SemaphoreHandle_t psdone = xSemaphoreCreateBinary();
			persistent_storage->flush([&psdone]() -> void { xSemaphoreGive(psdone); });
			xSemaphoreTake(psdone, portMAX_DELAY);
			vSemaphoreDelete(psdone);

			if (qspiflash != nullptr)
				SPIFlash::ResetBank(qspiflash);

			console->write("Restarting...\n");
			vTaskDelay(pdMS_TO_TICKS(100));

			Xil_Out32(SLCR_UNLOCK_REG, 0xDF0D); // Unlock key
			Xil_Out32(PSS_RST_CTRL_REG, 1);
		} else {
			console->write("Restart is disabled due to a failed firmware attempt (flash may be corrupted).\n");
		}
	}

	//virtual std::vector<std::string> complete(const CommandParser::CommandParameters &parameters) const { };
};

}
