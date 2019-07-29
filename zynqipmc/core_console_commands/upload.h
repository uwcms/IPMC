namespace {

class ConsoleCommand_upload : public CommandParser::Command {
public:
	virtual std::string get_helptext(const std::string &command) const {
		return stdsprintf(
				"%s $filename $bytes $sha256\n\n"
				"Uploads a file using the serial console. Check Github for instructions.\n", command.c_str());
	}

	virtual void execute(std::shared_ptr<ConsoleSvc> console, const CommandParser::CommandParameters &parameters) {
		// TODO: Right now this command takes over the UART driver. This will need to be changed.
		constexpr size_t MAX_FILE_SIZE = 16 * 1024 * 1024; // 16 Mbytes
		constexpr size_t MAX_BASE64_SIZE = MAX_FILE_SIZE * 4 / 3; // base64 implies 4:3 overhead

		std::string filename;
		size_t size;
		std::string hash;

		if (!parameters.parseParameters(1, false, &filename, &size, &hash)) {
			console->write("Invalid argument, see help.\n");
			return;
		}

		// Validate the arguments
		VFS::File *file = VFS::getFileFromPath(filename);
		if (!file || !file->write) {
			console->write("Destination file not found or no write callback defined.\n");
			return;
		}

		if (size > MAX_BASE64_SIZE || size > (file->size * 4 / 3)) {
			console->write("Requested size is too large.\n");
			return;
		}

		if (hash.length() != SHA_VALBYTES*2) {
			console->write("Provided hash doesn't have " + std::to_string(SHA_VALBYTES*2) + " characters.\n");
			return;
		}

		std::shared_ptr<u8> buf(new u8[size]);
		size_t timeout_sec = 5 + (size / 10000); // Will wait 5 seconds plus 1 second for every 10kByte.

		// Discard any incoming window size data, etc.
		psuart0->clear();
		console->write("Reading incoming serial stream for " + std::to_string(timeout_sec) + " seconds..\n");

		// Read the file from serial
		size_t bytesread = psuart0->read(buf.get(), size, portMAX_DELAY, configTICK_RATE_HZ * timeout_sec);

		if (bytesread != size) {
			console->write("Failed to read all bytes from the stream, only " + std::to_string(bytesread) + " bytes were read.\n");
			return;
		}

		console->write(std::to_string(bytesread) + " bytes successfully read from serial stream.\n");

		// Check hash validity
		u8 s256[SHA_VALBYTES] = {0};
		sha_256(buf.get(), bytesread, s256);

		std::string shahex;
		shahex.reserve(SHA_VALBYTES*2);
		for (int i = 0; i < SHA_VALBYTES; i++)
			   shahex.append(stdsprintf("%02hhx", s256[i]));
		console->write("Received hash is " + shahex + "\n");

		// Compare hash keys
		if (memcmp(shahex.data(), hash.data(), SHA_VALBYTES*2) != 0) {
			console->write("Hashes DO NOT match.\n");
			return;
		}

		// Decodes from base64 to binary
		std::string decoded = base64_decode((char*)buf.get());
		console->write(std::to_string(decoded.size()) + " bytes decoded from received base64 stream.\n");

		if (decoded.size() > file->size) {
			console->write("Decoded size is larger than file's maximum size.\n");
			return;
		}

		if (file->write((uint8_t*)decoded.data(), decoded.size()) == decoded.size()) {
			console->write("File was written successfully.\n");
		} else {
			console->write("Failed to write to file.\n");
		}
	}
};

}
