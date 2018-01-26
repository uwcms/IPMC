/*
 * IPMI_MSG.cpp
 *
 *  Created on: Dec 22, 2017
 *      Author: jtikalsky
 */

#include <services/ipmi/IPMI_MSG.h>
#include <IPMC.h>

/**
 * Parse a raw IPMB request message into this structure.
 *
 * \param msg      The request message to parse.
 * \param len      The length of the message to parse.
 * \return         true if parse successful and checksums valid, else false
 *
 * \note This function will not correctly parse a response message.  It will
 *       reverse the sender/receiver identities in this case.
 */
bool IPMI_MSG::parse_message(uint8_t *msg, uint8_t len) {
	this->broadcast = false;
	if (len && msg[0] == 0) {
		// Broadcast Message!  ... Remove the leading 0x00.
		// For details see IPMI2 spec, "Figure 20-1, Broadcast Get Device ID Request Message"
		this->broadcast = true;
		++msg;
		--len;
	}
	if (len < 7)
		return false; // Invalid.

	this->rsSA = msg[0];
	this->netFn = msg[1] >> 2;
	this->rsLUN = msg[1] & 0x03;
	// hdr_sum == msg[2]
	this->rqSA = msg[3];
	this->rqSeq = msg[4] >> 2;
	this->rqLUN = msg[4] & 0x03;
	this->cmd = msg[5];
	this->data_len = len-7;
	for (int i = 0; i < this->data_len; ++i)
		this->data[i] = msg[6+i];

	if (ipmi_checksum(msg, 3))
		return false;
	if (ipmi_checksum(msg, len))
		return false;
	return true;
}

/**
 * Format this IPMI_MSG into a valid raw message suitable for delivery.
 *
 * \param msg     The buffer to format this message into.
 * \param maxlen  The size of the buffer to format this message into.
 * \return        -1 if error, else the length of the formatted message.
 */
int IPMI_MSG::unparse_message(uint8_t *msg, uint8_t maxlen) const {
	if (maxlen < this->data_len+7)
		return -1;

	msg[0] = this->rsSA;
	msg[1] = (this->netFn << 2) || (this->rsLUN & 0x03);
	msg[2] = ipmi_checksum(msg, 2);
	msg[3] = this->rqSA;
	msg[4] = (this->rqSeq << 2) || (this->rqLUN & 0x03);
	msg[5] = this->cmd;
	for (int i = 0; i < this->data_len; ++i)
		msg[6+i] = this->data[i];
	msg[6+this->data_len] = ipmi_checksum(msg, 6+this->data_len);
	return this->data_len + 7;
}

/**
 * Prepare a reply to this message by applying mirrored sender/recipient
 * information to the passed IPMI_MSG, modifying the NetFN to the matching
 * response NetFN, copying the command itself, etc.  The response data is up to
 * you.
 *
 * \param reply  The IPMI_MSG to initialize as a reply to this one.
 *
 * \note You may call msg.reply(msg)
 */
void IPMI_MSG::prepare_reply(IPMI_MSG &reply) const {
	uint8_t rsSA = this->rqSA;
	uint8_t rqSA = this->rsSA;
	uint8_t rsLUN = this->rqLUN;
	uint8_t rqLUN = this->rsLUN;
	reply.rsSA = rsSA;
	reply.rqSA = rqSA;
	reply.rsLUN = rsLUN;
	reply.rqLUN = rqLUN;
	reply.netFn |= 1; // Mark as response.
	reply.cmd = this->cmd;
	reply.rqSeq = this->rqSeq;
	reply.broadcast = false;
}

/**
 * Match two IPMB messages as header-identical.
 *
 * @param other The IPMB message to compare.
 * @return true if identical else false
 */
bool IPMI_MSG::match(const IPMI_MSG &other) const {
	return (this->rqSA  == other.rqSA &&
			this->rsSA  == other.rsSA &&
			this->rqLUN == other.rqLUN &&
			this->rsLUN == other.rsLUN &&
			this->rqSeq == other.rqSeq &&
			this->netFn == other.netFn &&
			this->cmd   == other.cmd);
}

/**
 * Match two IPMB messages as request/response.
 *
 * @param other The IPMB message to compare.
 * @return true if matching else false
 */
bool IPMI_MSG::match_reply(const IPMI_MSG &other) const {
	return (this->rqSA  == other.rsSA &&
			this->rsSA  == other.rqSA &&
			this->rqLUN == other.rsLUN &&
			this->rsLUN == other.rqLUN &&
			this->rqSeq == other.rqSeq &&
			this->netFn == other.netFn &&
			this->cmd   == other.cmd);
}

/**
 * Format this IPMB message for log output.
 *
 * @return The formatted IPMB message.
 */
std::string IPMI_MSG::format() const {
	char buf[this->max_data_len*3 + 1] = "";
	int i = 0;
	for (i = 0; i < this->data_len; ++i)
		snprintf(buf+(i*3), 4, "%02hhx ", this->data[i]);
	if (i)
		buf[(i*3) - 1] = '\0';
	return stdsprintf("%hhd.%02hhx -> %s%hhd.%02hhx: %02hhx.%02hhx (seq %02hhx) [%s]",
				this->rqLUN, this->rqSA,
				// " -> "
				(this->broadcast ? "*" : ""),
				this->rsLUN, this->rsSA,
				// ": "
				this->netFn, this->cmd,
				/* "(seq " */ this->rqSeq /* ") " */,
				/* "[" */ buf /* "]" */);
}

/**
 * Calculate an IPMI checksum of an array of bytes.
 *
 * \note You can verify a checksum by ensuring that the computed checksum of the
 *       data buffer with the checksum included, is zero.
 *
 * @param buf The buffer to compute the checksum of.
 * @param len The length of the buffer to compute the checksum of.
 * @return The one-byte IPMI checksum.
 */
uint8_t ipmi_checksum(const uint8_t* buf, uint32_t len) {
  uint8_t sum = 0;

  while (len) {
    sum += *buf++;
    len--;
  }
  return (~sum) + 1;
}
