#include "elmlink_protocol.h"
#include <libs/base64/base64.h>
#include <libs/crc32/crc32.h>
#include <stdint.h>
#include <string>

using namespace ELMLink;

void crc32(const void *data, size_t n_bytes, uint32_t *crc);

/* The protocol is based on a checksumed, length-prefixed design.
 *
 * We have relatively reliable transport that is synchronized to byte
 * boundaries for us, but we still need to synchronize this byte-stream into
 * a packet stream.
 *
 * As such, each message will begin with a sync byte: 0xff.
 * No other byte will ever have bit 7 set.
 *
 * This is achieved by base64 encoding data (75% effiency is good enough
 * here).
 *
 * The MSB of all header fields will be split out and packed into the lower
 * bits of a dedicated "MSBs" field.
 *
 * Our packet header will be:
 *   1 byte 'sync' (0xff)
 *   1 byte 'MSBs of following bytes, "little endian"'
 *   2 bytes encoded data size (little endian)
 *   4 bytes crc32 over channel + encoded data (little endian)
 *   1 byte 'channel'
 *   $size bytes base64-encoded data
 */

bool Packet::digest(std::string &recvbuf) {

	while (true) {
		size_t pos = recvbuf.find_first_of('\xff');
		// Discard any out of sync data.
		if (pos && pos != std::string::npos)
			recvbuf.erase(0, pos);
		// Now if we have a buffer at all, we have the start of a packet.

		if (recvbuf.size() < HEADER_LENGTH)
			return false; // No packet header is ready.

		pos = recvbuf.find_first_of('\xff', 1); // Search for the NEXT sync char.
		if (pos < HEADER_LENGTH) {
			/* We found another sync char in the header, and therefore don't
             * have a complete header.  This is a false alarm.  Advance the
             * sync.
			 */
			recvbuf.erase(0, pos);
			continue;
		}

		// Copy the header into a mutable buffer.
		uint8_t header[HEADER_LENGTH];
		for (int i = 0; i < HEADER_LENGTH; ++i)
			header[i] = recvbuf[i];

		// Redistribute segregated MSBs.
		for (int i = 2; i < HEADER_LENGTH; ++i) {
			if (header[1] & 1)
				header[i] |= 0x80;
			header[1] >>= 1;
		}

		uint16_t size = (header[3] << 8) | header[2];
		uint32_t crc32_header = (header[7] << 24) | (header[6] << 16) | (header[5] << 8) | header[4];
		uint8_t channel = header[8];

		if (size > MAX_ENCODED_PAYLOAD_LENGTH) {
			// Invalid packet.  Discard our sync char and retry.
			recvbuf.erase(0, 1);
			continue;
		}

		if (recvbuf.size() < HEADER_LENGTH + size)
			return false; // We don't have the whole packet yet.

		std::string payload = recvbuf.substr(HEADER_LENGTH, size);
		recvbuf.erase(0, HEADER_LENGTH + size);

		uint32_t crc32_verify = 0;
		crc32(&channel, 1, &crc32_verify);
		crc32(payload.data(), payload.size(), &crc32_verify);
		if (crc32_verify != crc32_header) {
			// Invalid packet.  Discard our sync char and retry.
			recvbuf.erase(0, 1);
			continue;
		}

		this->data = base64_decode(payload);
		this->channel = channel;
		return true;
	}
}

std::string Packet::serialize() {
	if (this->channel & 0x80 || this->data.size() > MAX_DECODED_PACKET_LENGTH)
		return "";

	std::string data = base64_encode((const uint8_t *)this->data.data(), this->data.size());

	uint32_t crc32_header = 0;
	crc32(&this->channel, 1, &crc32_header);
	crc32(data.data(), data.size(), &crc32_header);

	uint8_t header[HEADER_LENGTH];
	header[0] = 0xff;
	header[1] = 0; // msbset
	header[2] = (data.size() >> 0) & 0xff;
	header[3] = (data.size() >> 8) & 0xff;
	header[4] = (crc32_header >> 0) & 0xff;
	header[5] = (crc32_header >> 8) & 0xff;
	header[6] = (crc32_header >> 16) & 0xff;
	header[7] = (crc32_header >> 24) & 0xff;
	header[8] = this->channel;

	// Segregate MSBs
	for (int i = HEADER_LENGTH - 1; i > 1; --i) {
		header[1] <<= 1;
		if (header[i] & 0x80)
			header[1] |= 1;
		header[i] &= 0x7f;
	}

	return std::string((char *)header, HEADER_LENGTH) + data;
}

std::map<uint8_t, std::string> Packet::decode_channel_index_update_packet(std::string data) {
	std::map<uint8_t, std::string> output;
	if (data.substr(0, 6) != "UPDATE")
		return {}; // Nope.
	data.erase(0, 6);
	while (data.size() >= 2) {
		uint8_t channel = (uint8_t)data[0];
		if (channel & 0x80)
			return {}; // Invalid Reserved. Nope.

		uint8_t size = (uint8_t)data[1];
		if (data.size() < 2 + (size_t)size)
			return {}; // Nope.

		output[channel] = data.substr(2, size); // Got one.
		data.erase(0, 2 + size);
	}
	if (data.size())
		return {}; // So close.
	else
		return output; // Woo!
}

std::string Packet::encode_channel_index_update_packet(const std::map<uint8_t, std::string> &index) {
	std::string message = "UPDATE";
	for (auto channel_info : index) {
		/* Packet format is a list of entries:
		 *
		 * An entry is:
		 *   1 channel number
		 *   2 name bytecount
		 *   n name bytes
		 */
		uint8_t channel = channel_info.first;
		uint8_t namelen = channel_info.second.size();
		message.append((const char *)&channel, 1);
		message.append((const char *)&namelen, 1);
		message.append(channel_info.second);
	}
	return message;
}
