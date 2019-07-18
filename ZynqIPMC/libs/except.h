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

#ifndef SRC_COMMON_ZYNQIPMC_LIBS_EXCEPT_H_
#define SRC_COMMON_ZYNQIPMC_LIBS_EXCEPT_H_

#include <exception>
#include <stdexcept>
#include <string>

/**
 * Implements custom exceptions specific for the ZYNQ-IPMC Framework.
 */

#define DEFINE_LOCAL_GENERIC_EXCEPTION(name, base) \
	class name : public base { \
	public: \
		explicit name(const std::string& what_arg) : base(what_arg) { }; \
		explicit name(const char* what_arg) : base(what_arg) { }; \
	};

#define DEFINE_GENERIC_EXCEPTION(name, base) \
	namespace except { DEFINE_LOCAL_GENERIC_EXCEPTION(name, base) }

DEFINE_GENERIC_EXCEPTION(hardware_error, std::runtime_error)

#endif /* SRC_COMMON_ZYNQIPMC_LIBS_EXCEPT_H_ */
