#ifndef ELMLINK_PROTOCOL_H
#define ELMLINK_PROTOCOL_H

#include <map>
#include <stdint.h>
#include <string>

namespace ELMLink
{

/* 8192 byte packets will base64encode to 10924 payload bytes, which will take
 * about 0.1 seconds to transfer at 115200 and still be large enough for most
 * common ELM Link purposes.  If you need high bandwidth, exchange IPs and use
 * ethernet.
 *
 * This size limit must be synchronized and respected across all link endpoints
 * and clients.
 *
 * There is a hard cap in that MAX_ENCODED_PAYLOAD_LENGTH must fit in a uint16_t.
 */
const int MAX_DECODED_PACKET_LENGTH = 8192;
const int MAX_ENCODED_PAYLOAD_LENGTH = 10924;

const int HEADER_LENGTH = 9;
const int MAX_SERIALIZED_PACKET_LENGTH = HEADER_LENGTH + MAX_ENCODED_PAYLOAD_LENGTH;

class Packet {
public:
	uint8_t channel;
	std::string data;

	Packet() : channel(0xFF), data(""){};
	Packet(uint8_t channel, std::string data) : channel(channel), data(data){};
	Packet(uint8_t channel, const uint8_t *data, size_t length) : channel(channel), data(std::string((const char *)data, length)){};

	/**
     * Fill this packet with the contents of the next packet from the provided
     * input buffer.
	 *
	 * \note This will modify the buffer to remove the digested packet, as well
     *       as any unrecognized data preceding the first valid packet, even if
     *       no packet was found.
	 *
	 * \param recvbuf The receive buffer to digest.
	 *
	 * \return true if a packet was digested, else false
     */
	bool digest(std::string &recvbuf);

	/**
	 * Generate this packet's serial data format, suitable for sending over the
	 * wire.
	 *
	 * \return A string containing the binary serial data that represents this
	 *         packet, or "" if the packet was invalid (invalid channel or
	 *         oversize data).
	 */
	std::string serialize();

	/**
	 * If this is a valid channel index update packet, this function will decode
	 * it and return its contents.
	 *
	 * \param data The packet payload.
	 * \return A channel index if this can be parsed as a channel index update,
	 *         else an empty map.
	 */
	static std::map<uint8_t, std::string> decode_channel_index_update_packet(std::string data);

	/**
	 * Encode the provided channel index into a channel index update message.
	 *
	 * No channel may be >= 0x80.  No name may be >255 chars.
	 *
	 * \param index The channel index to encode.
	 * \return The packet contents.
	 */
	static std::string encode_channel_index_update_packet(const std::map<uint8_t, std::string> &index);
};

} // namespace ELMLink

#endif // ELMLINK_PROTOCOL_H
