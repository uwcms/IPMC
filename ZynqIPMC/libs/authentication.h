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

#ifndef SRC_COMMON_ZYNQIPMC_LIBS_AUTHENTICATION_H_
#define SRC_COMMON_ZYNQIPMC_LIBS_AUTHENTICATION_H_

#include <stdint.h>
#include <xilrsa.h> // MUST be included in the BSP.
#include <string>

//! Authentication functions, avoids poluting namespace.
namespace Auth {

//! Hash is composed of a user key and a password key.
typedef struct {
	uint8_t user[SHA_VALBYTES];	///< sha256 user key
	uint8_t pass[SHA_VALBYTES]; ///< sha256 password key
} HashPair;

/**
 * Will attempt to authenticate a user.
 * @param user The username.
 * @param pass The password.
 * @return true if authentication was successful, false if otherwise.
 */
bool validateCredentials(const std::string &user, const std::string &pass);

/**
 * Will attempt to authenticate just the password. User will be ignored.
 * @param pass The password.
 * @return true if authentication was successful, false if otherwise.
 */
bool validateCredentials(const std::string &pass);

/**
 * Will generate and store new credentials.
 * @param user The new username.
 * @param pass The new password.
 */
void changeCredentials(const std::string &user, const std::string &pass);

/**
 * Will change the credentials to the default ones.
 */
void resetCredentials();

};

#endif /* SRC_COMMON_ZYNQIPMC_LIBS_AUTHENTICATION_H_ */
