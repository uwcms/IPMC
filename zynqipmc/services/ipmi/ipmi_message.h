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

#ifndef SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_IPMIMESSAGE_H_
#define SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_IPMIMESSAGE_H_

#include <stdint.h>
#include <string>
#include <vector>
#include <memory>

/**
 * An IPMB Message record.
 *
 * This contains an IPMI command, complete with all relevant headers for IPMB-0
 * transit, as well as parsing and construction functions.
 */
class IPMIMessage {
public:
	//! Instantiate an IPMIMessage as a blank slate.
	IPMIMessage();

	//! Instantiate an IPMIMessage with parameters as a convenience.
	IPMIMessage(uint8_t rqLUN, uint8_t rqSA, uint8_t rsLUN, uint8_t rsSA, uint8_t netFn, uint8_t cmd, const std::vector<uint8_t> &data);

	static const uint8_t max_data_len = 32-1-5; ///< The max length of command data.  (IPMI message max length 32, minus one byte of address, minus 5 bytes of header)
	uint8_t rsSA;               ///< (byte 0)       The responder slave address.
	uint8_t netFn;              ///< (byte 1[7:2])  The network function of the command.
	uint8_t rsLUN;              ///< (byte 1[1:0])  The responder LUN.
	// hdr_sum;                 ///< (byte 2)       The header checksum.
	uint8_t rqSA;               ///< (byte 3)       The requester slave address.
	uint8_t rqSeq;              ///< (byte 4[7:2])  The request sequence number.
	uint8_t rqLUN;              ///< (byte 4[1:0])  The requester LUN.
	uint8_t cmd;                ///< (byte 5)       The IPMI command number.
	uint8_t data[max_data_len]; ///< (byte 6-*)     The IPMI command parameter/response data.
	uint8_t data_len;           ///<                The length of the parameter/response data.
	// all_sum;                 ///< (byte last)    The message checksum.

	bool broadcast;             ///< Is this a broadcast message?  Nothing really has any reason to use this.
	bool duplicate;             ///< true if duplicate, else false.  Only applies to incoming requests.

	/**
	 * Parse a raw IPMB request message into this structure.
	 *
	 * @param msg      The request message to parse.
	 * @param len      The length of the message to parse.
	 * @param LocalIPMBAddress The address of the recipient node on the IPMB.
	 * @return         true if parse successful and checksums valid, else false.
	 *
	 * @note This function will not correctly parse a response message.  It will
	 *       reverse the sender/receiver identities in this case.
	 */
	bool parseMessage(uint8_t *msg, uint8_t len, uint8_t LocalIPMBAddress);

	/**
	 * Format this IPMIMessage into a valid raw message suitable for delivery.
	 *
	 * @param msg     The buffer to format this message into.
	 * @param maxlen  The size of the buffer to format this message into.
	 * @return        -1 if error, else the length of the formatted message.
	 */
	int unparseMessage(uint8_t *msg, uint8_t maxlen) const;

	/**
	 * Prepare a reply to this message by applying mirrored sender/recipient
	 * information to the passed IPMIMessage, modifying the NetFN to the matching
	 * response NetFN, copying the command itself, etc.  The response data is up to
	 * you.
	 *
	 * @param reply  The IPMIMessage to initialize as a reply to this one.
	 *
	 * @note You may call msg.reply(msg).
	 */
	void prepareReply(IPMIMessage &reply, const std::vector<uint8_t> &reply_data=std::vector<uint8_t>()) const __attribute__((deprecated));
	std::shared_ptr<IPMIMessage> prepareReply(const std::vector<uint8_t> &reply_data=std::vector<uint8_t>()) const;

	/**
	 * Match two IPMB messages as header-identical.
	 *
	 * @param other The IPMB message to compare.
	 * @return true if identical else false.
	 */
	bool match(const IPMIMessage &other) const;

	/**
	 * Check if the supplied IPMB_MSG is a response to this one.
	 *
	 * @param response The response IPMB message to compare.
	 * @return true if matching else false
	 */
	bool matchReply(const IPMIMessage &response) const;

	/**
	 * Format this IPMB message for log output.
	 *
	 * @return The formatted IPMB message.
	 */
	std::string format(bool describe=true) const;

	/**
	 * Calculate an IPMI checksum of an array of bytes.
	 *
	 * @note You can verify a checksum by ensuring that the computed checksum of the
	 *       data buffer with the checksum included, is zero.
	 *
	 * @param buf The buffer to compute the checksum of.
	 * @param len The length of the buffer to compute the checksum of.
	 * @return The one-byte IPMI checksum.
	 */
	static uint8_t checksum(const uint8_t* buf, uint32_t len);
	static uint8_t checksum(const std::vector<uint8_t> &buf);
};

#endif /* SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_IPMIMESSAGE_H_ */
