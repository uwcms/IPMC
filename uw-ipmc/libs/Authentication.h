/*
 * Authentication.h
 *
 *  Created on: Aug 13, 2018
 *      Author: mpv
 */

#ifndef SRC_COMMON_UW_IPMC_LIBS_AUTHENTICATION_H_
#define SRC_COMMON_UW_IPMC_LIBS_AUTHENTICATION_H_

#include <stdint.h>
#include <xilrsa.h>
#include <string>

namespace Auth {

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
bool ValidateCredentials(const std::string &user, const std::string &pass);

/**
 * Will attempt to authenticate just the password. User will be ignored.
 * @param pass The password.
 * @return true if authentication was successful, false if otherwise.
 */
bool ValidateCredentials(const std::string &pass);

/**
 * Will generate and store new credentials
 * @param user The new username.
 * @param pass The new password.
 */
void ChangeCredentials(const std::string &user, const std::string &pass);

/**
 * Will change the credentials to the default ones.
 */
void ResetCredentials();

};

#endif /* SRC_COMMON_UW_IPMC_LIBS_AUTHENTICATION_H_ */
