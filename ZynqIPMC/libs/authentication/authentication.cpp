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

#include <libs/threading.h>
#include "authentication.h"
#include "services/persistentstorage/PersistentStorage.h"

//! Persistent storage is required to fetch the keys from hardware.
extern PersistentStorage *persistent_storage;

using namespace Auth;

bool Auth::validateCredentials(const std::string &user, const std::string &pass) {
	// Get the current hash from persistent storage
	uint16_t secver = persistent_storage->get_section_version(PersistentStorageAllocations::WISC_NETWORK_AUTH);
	HashPair *nv_hashes = (HashPair*)persistent_storage->get_section(PersistentStorageAllocations::WISC_NETWORK_AUTH, 1, sizeof(HashPair));
	if (secver == 0) {
		// Default password and user
		resetCredentials();
	}

	// Generate the hash for provided username and password
	HashPair provHashes;
	sha_256((const uint8_t*)user.data(), user.size(), provHashes.user);
	sha_256((const uint8_t*)pass.data(), pass.size(), provHashes.pass);

	// Compare keys
	if (memcmp(nv_hashes, &provHashes, sizeof(HashPair)) == 0) {
		// Credentials are valid!
		return true;
	} else {
		// Credentials are incorrect!
		return false;
	}
}

bool Auth::validateCredentials(const std::string &pass) {
	// Get the current hash from persistent storage
	uint16_t secver = persistent_storage->get_section_version(PersistentStorageAllocations::WISC_NETWORK_AUTH);
	HashPair *nv_hashes = (HashPair*)persistent_storage->get_section(PersistentStorageAllocations::WISC_NETWORK_AUTH, 1, sizeof(HashPair));
	if (secver == 0) {
		// Default password and user
		resetCredentials();
	}

	// Generate the hash for provided username and password
	uint8_t provPassHash[SHA_VALBYTES];
	sha_256((const uint8_t*)pass.data(), pass.size(), provPassHash);

	// Compare keys
	if (memcmp(nv_hashes->pass, provPassHash, SHA_VALBYTES) == 0) {
		// Password is valid!
		return true;
	} else {
		// Password is incorrect!
		return false;
	}
}

void Auth::changeCredentials(const std::string &user, const std::string &pass) {
	HashPair *nv_hashes = (HashPair*)persistent_storage->get_section(PersistentStorageAllocations::WISC_NETWORK_AUTH, 1, sizeof(HashPair));

	// Generate the hash for provided username and password
	HashPair new_hashes;
	sha_256((const uint8_t*)user.data(), user.size(), new_hashes.user);
	sha_256((const uint8_t*)pass.data(), pass.size(), new_hashes.pass);
	CriticalGuard critical(true);
	memcpy(nv_hashes, &new_hashes, sizeof(HashPair));
	critical.release();
	persistent_storage->flush(nv_hashes, sizeof(HashPair));
}

void Auth::resetCredentials() {
	HashPair *nv_hashes = (HashPair*)persistent_storage->get_section(PersistentStorageAllocations::WISC_NETWORK_AUTH, 1, sizeof(HashPair));

	// Default password and user
	const char* default_user = "\x0c\x7a\x0a\xbd\x06\x6e\xd5\x36\xc1\x05\xcf\xaf\x4e\x55\x14\xf5\x86\x65\x07\x9f\x5a\x2a\x52\x12\xea\x32\x01\x90\xd0\xbc\xb6\xd2";
	const char* default_pass = "\x0c\x7a\x0a\xbd\x06\x6e\xd5\x36\xc1\x05\xcf\xaf\x4e\x55\x14\xf5\x86\x65\x07\x9f\x5a\x2a\x52\x12\xea\x32\x01\x90\xd0\xbc\xb6\xd2";
	CriticalGuard critical(true);
	memcpy(nv_hashes->user, default_user, SHA_VALBYTES);
	memcpy(nv_hashes->pass, default_pass, SHA_VALBYTES);
	critical.release();
	persistent_storage->flush(nv_hashes, sizeof(HashPair));
}
