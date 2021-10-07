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

#ifndef SRC_COMMON_ZYNQIPMC_DRIVERS_TRACEBUFFER_FTPTRACEBUFFER_H_
#define SRC_COMMON_ZYNQIPMC_DRIVERS_TRACEBUFFER_FTPTRACEBUFFER_H_

#include "tracebuffer.h"
#include <libs/vfs/vfs.h>
#include <services/console/consolesvc.h>

/**
 * This provides a Trace Buffer facility to allow memory dumps of structured
 * high-volume detailed tracing data.
 */
class FTPTraceBuffer : public ConsoleCommandSupport {
public:
	/**
	 * Instantiate a new FTP TraceBuffer interface.
	 * @param tracebuffer  The TraceBuffer to provide an interface to.
	 * @param secure       The default security state for this interface.
	 */
	FTPTraceBuffer(const TraceBuffer &tracebuffer, bool secure = true) : tracebuffer(tracebuffer), size(tracebuffer.export_size), secure(secure){};
	virtual ~FTPTraceBuffer(){};

	/**
	 * Generates an VFS file linked to the TraceBuffer that can be added to the
	 * virtual file system, allowing download via ethernet.
	 *
	 * @return VFS::File that can be used with VFS::addFile.
	 */
	virtual VFS::File createExportFile();

	const TraceBuffer &tracebuffer; ///< The TraceBuffer to provide an interface to.
	const size_t size;              ///< The size of the TraceBuffer.
	bool secure;                    ///< Whether the tracebuffer interface is secure, or unlocked.

	virtual void registerConsoleCommands(CommandParser &parser, const std::string &prefix);
	virtual void deregisterConsoleCommands(CommandParser &parser, const std::string &prefix);
};

#endif /* SRC_COMMON_ZYNQIPMC_DRIVERS_TRACEBUFFER_FTPTRACEBUFFER_H_ */
