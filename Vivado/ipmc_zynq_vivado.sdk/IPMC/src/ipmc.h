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

/**
 * This is the place to put application specific functions and variables.
 */

#ifndef SRC_IPMC_H_
#define SRC_IPMC_H_

// Implemented in sdr_init.cpp:
void initDeviceSDRs(bool reinit);

// Implemented in fru_data_init.cpp:
void initFruData(bool reinit);

#endif /* SRC_IPMC_H_ */
