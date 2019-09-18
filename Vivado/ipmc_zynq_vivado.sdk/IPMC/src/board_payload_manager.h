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

#ifndef SRC_BOARD_PAYLOAD_MANAGER_H_
#define SRC_BOARD_PAYLOAD_MANAGER_H_

#include <payload_manager.h>

/**
 * This class implements all the default payload manager operations and
 * overrides a few so that board specific code can be implemented.
 *
 * @warning This is application specific and should be adjusted!
 *
 * Check which functions in PayloadManager are virtual, but not pure, to
 * understand what functions can be overridden for more custom operation.
 *
 * If a function from PayloadManager is overridden then it is recommended
 * that that function is copied in its entirety before proceeding to do
 * changes.
 */
class BoardPayloadManager final : public PayloadManager {
public:
	BoardPayloadManager(MStateMachine *mstate_machine, LogTree &log);
	virtual ~BoardPayloadManager();

	// Mandatory public implementations of PayloadManager:
	virtual void config();
	virtual PowerProperties getPowerProperties(uint8_t fru, bool recompute = false);
	virtual void setPowerLevel(uint8_t fru, uint8_t level);

	// Optional/overridden public implementations of PayloadManager:
	// None

protected:
	// Mandatory protected implementations of PayloadManager:
	virtual void implementPowerLevel(uint8_t level);

	// Optional protected implementations of PayloadManager:
	// None
};

#endif /* SRC_BOARD_PAYLOAD_MANAGER_H_ */
