/*
 * Authentication.cpp
 *
 *  Created on: Aug 13, 2018
 *      Author: mpv
 */

#include "Authentication.h"

// TODO: Maybe have this in a configuration function?
#include "services/persistentstorage/PersistentStorage.h"
extern PersistentStorage *persistent_storage;

using namespace Auth;

bool Auth::ValidateCredentials(const std::string &user, const std::string &pass) {
	// Get the current hash from persistent storage
	uint16_t secver = persistent_storage->get_section_version(PersistentStorageAllocations::WISC_NETWORK_AUTH);
	HashPair *nvHashes = (HashPair*)persistent_storage->get_section(PersistentStorageAllocations::WISC_NETWORK_AUTH, 1, sizeof(HashPair));
	if (secver == 0) {
		// Default password and user
		ResetCredentials();
	}

	// Generate the hash for provided username and password
	HashPair provHashes;
	sha_256((const uint8_t*)user.data(), user.size(), provHashes.user);
	sha_256((const uint8_t*)pass.data(), pass.size(), provHashes.pass);

	// Compare keys
	if (memcmp(nvHashes, &provHashes, sizeof(HashPair)) == 0) {
		// Credentials are valid!
		return true;
	} else {
		// Credentials are incorrect!
		return false;
	}
}

bool Auth::ValidateCredentials(const std::string &pass) {
	// Get the current hash from persistent storage
	uint16_t secver = persistent_storage->get_section_version(PersistentStorageAllocations::WISC_NETWORK_AUTH);
	HashPair *nvHashes = (HashPair*)persistent_storage->get_section(PersistentStorageAllocations::WISC_NETWORK_AUTH, 1, sizeof(HashPair));
	if (secver == 0) {
		// Default password and user
		ResetCredentials();
	}

	// Generate the hash for provided username and password
	uint8_t provPassHash[SHA_VALBYTES];
	sha_256((const uint8_t*)pass.data(), pass.size(), provPassHash);

	// Compare keys
	if (memcmp(nvHashes->pass, provPassHash, SHA_VALBYTES) == 0) {
		// Password is valid!
		return true;
	} else {
		// Password is incorrect!
		return false;
	}
}

void Auth::ChangeCredentials(const std::string &user, const std::string &pass) {
	HashPair *nvHashes = (HashPair*)persistent_storage->get_section(PersistentStorageAllocations::WISC_NETWORK_AUTH, 1, sizeof(HashPair));

	// Generate the hash for provided username and password
	taskENTER_CRITICAL();
	sha_256((const uint8_t*)user.data(), user.size(), nvHashes->user);
	sha_256((const uint8_t*)pass.data(), pass.size(), nvHashes->pass);
	taskEXIT_CRITICAL();
	persistent_storage->flush(nvHashes, sizeof(HashPair));
}

void Auth::ResetCredentials() {
	HashPair *nvHashes = (HashPair*)persistent_storage->get_section(PersistentStorageAllocations::WISC_NETWORK_AUTH, 1, sizeof(HashPair));

	// Default password and user
	taskENTER_CRITICAL();
	const char* default_user = "\x0c\x7a\x0a\xbd\x06\x6e\xd5\x36\xc1\x05\xcf\xaf\x4e\x55\x14\xf5\x86\x65\x07\x9f\x5a\x2a\x52\x12\xea\x32\x01\x90\xd0\xbc\xb6\xd2";
	const char* default_pass = "\x0c\x7a\x0a\xbd\x06\x6e\xd5\x36\xc1\x05\xcf\xaf\x4e\x55\x14\xf5\x86\x65\x07\x9f\x5a\x2a\x52\x12\xea\x32\x01\x90\xd0\xbc\xb6\xd2";
	memcpy(nvHashes->user, default_user, SHA_VALBYTES);
	memcpy(nvHashes->pass, default_pass, SHA_VALBYTES);
	taskEXIT_CRITICAL();
	persistent_storage->flush(nvHashes, sizeof(HashPair));
}
