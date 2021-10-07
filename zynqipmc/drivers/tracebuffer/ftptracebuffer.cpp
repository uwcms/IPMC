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

#include "ftptracebuffer.h"

VFS::File FTPTraceBuffer::createExportFile() {
	return VFS::File(
	    [this](uint8_t *buffer, size_t size) -> size_t {
		    // Read
		    if (this->secure)
			    return 0; // We are in secure mode.  No access is permitted.
		    if (size != this->size)
			    return 0; // We only accept full file reads.
		    if (this->tracebuffer.export_buffer(buffer, size) != this->size)
			    return 0;
		    return size;
	    },
	    nullptr,
	    this->size);
}

namespace
{
class FTPTraceBufferLockUnlockCommand final : public CommandParser::Command {
public:
	FTPTraceBufferLockUnlockCommand(FTPTraceBuffer &ftptracebuffer) : ftptracebuffer(ftptracebuffer){};

	virtual std::string getHelpText(const std::string &command) const {
		return command + " [lock|unlock]\n\n"
		                 "Change or view the FTP TraceBuffer interface security state.\n";
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		if (parameters.nargs() == 1) {
			// All's well.  We'll print out below.
		}
		else if (parameters.nargs() == 2 && parameters.parameters[1] == "lock") {
			this->ftptracebuffer.secure = true;
		}
		else if (parameters.nargs() == 2 && parameters.parameters[1] == "unlock") {
			this->ftptracebuffer.secure = false;
		}
		if (this->ftptracebuffer.secure)
			console->write("The FTP TraceBuffer is LOCKED.  Readout is restricted.\n");
		else
			console->write("The FTP TraceBuffer is UNLOCKED.  Readout is permitted.\n");
	}

private:
	FTPTraceBuffer &ftptracebuffer;
};
}; // anonymous namespace

void FTPTraceBuffer::registerConsoleCommands(CommandParser &parser, const std::string &prefix) {
	// We only provide one command, and use our prefix as its name.
	parser.registerCommand(prefix + "", std::make_shared<FTPTraceBufferLockUnlockCommand>(*this));
}

void FTPTraceBuffer::deregisterConsoleCommands(CommandParser &parser, const std::string &prefix) {
	parser.registerCommand(prefix + "", nullptr);
}
