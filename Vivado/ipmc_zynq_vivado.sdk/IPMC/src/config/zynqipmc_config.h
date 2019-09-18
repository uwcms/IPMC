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
 * @file zynqipmc_config.h
 *
 * ZYNQ-IPMC framework application specific configurations.
 */

#ifndef SRC_CONFIG_ZYNQIPMC_CONFIG_H_
#define SRC_CONFIG_ZYNQIPMC_CONFIG_H_


//! Default memory allocation in words per task.
#define ZYNQIPMC_BASE_STACK_SIZE (16384/4)

//! Comment to disable exposure of low level driver commands, minimizes attack surface
#define ENABLE_DRIVER_COMMAND_SUPPORT

//! Comment to disabled the watchdog timer
#define ENABLE_WATCHDOGTIMER

//! Comment do disable IPMI and IPMC functionalities
#define ENABLE_IPMI

//! Defines how many bytes the trace buffer will have.
#define TRACEBUFFER_SIZE (1*1024*1024) // 1MB


#endif /* SRC_CONFIG_ZYNQIPMC_CONFIG_H_ */
