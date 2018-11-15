/*
 * IPMI_MSG.cpp
 *
 *  Created on: Dec 22, 2017
 *      Author: jtikalsky
 */

#include <services/ipmi/IPMI_MSG.h>
#include <services/ipmi/IPMI.h>
#include <libs/printf.h>
#include <libs/except.h>
#include <IPMC.h>

/// Instantiate an IPMI_MSG as a blank slate.
IPMI_MSG::IPMI_MSG()
	: rsSA(0), netFn(0), rsLUN(0), rqSA(0), rqSeq(0), rqLUN(0), cmd(0), data_len(0), broadcast(false), duplicate(false) {
	for (std::vector<uint8_t>::size_type i = 0; i < this->max_data_len; ++i)
		this->data[i] = 0;
};

/// Instantiate an IPMI_MSG with parameters as a convenience.
IPMI_MSG::IPMI_MSG(uint8_t rqLUN, uint8_t rqSA, uint8_t rsLUN, uint8_t rsSA, uint8_t netFn, uint8_t cmd, const std::vector<uint8_t> &data)
	: rsSA(rsSA), netFn(netFn), rsLUN(rsLUN), rqSA(rqSA), rqSeq(0), rqLUN(rqLUN), cmd(cmd), data_len(data.size()), broadcast(false), duplicate(false) {
	if (data.size() > max_data_len)
		throw std::domain_error(stdsprintf("Only up to %hhu bytes of IPMI message data are supported.", max_data_len));
	for (std::vector<uint8_t>::size_type i = 0; i < data.size(); ++i)
		this->data[i] = data[i];
};

/**
 * Parse a raw IPMB request message into this structure.
 *
 * \param msg      The request message to parse.
 * \param len      The length of the message to parse.
 * \param LocalIPMBAddress The address of the recipient node on the IPMB.
 * \return         true if parse successful and checksums valid, else false
 *
 * \note This function will not correctly parse a response message.  It will
 *       reverse the sender/receiver identities in this case.
 */
bool IPMI_MSG::parse_message(uint8_t *msg, uint8_t len, uint8_t LocalIPMBAddress) {
	this->broadcast = false;
	if (len && msg[0] == 0) {
		// Broadcast Message!  ... Remove the leading 0x00.
		// For details see IPMI2 spec, "Figure 20-1, Broadcast Get Device ID Request Message"
		this->broadcast = true;
		++msg;
		--len;
	}
	if (len < 6)
		return false; // Invalid.

	this->rsSA = LocalIPMBAddress;
	this->netFn = msg[0] >> 2;
	this->rsLUN = msg[0] & 0x03;
	// hdr_sum == msg[1]
	this->rqSA = msg[2];
	this->rqSeq = msg[3] >> 2;
	this->rqLUN = msg[3] & 0x03;
	this->cmd = msg[4];
	this->data_len = len-6;
	for (int i = 0; i < this->data_len; ++i)
		this->data[i] = msg[5+i];

	uint8_t hdrsumbuf[3] = { LocalIPMBAddress, msg[0], msg[1] };
	if (ipmi_checksum(hdrsumbuf, 3))
		return false;
	if (ipmi_checksum(msg+2, len-2))
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

	msg[0] = (this->netFn << 2) | (this->rsLUN & 0x03);
	msg[1] = this->rsSA; // We need to include the I2C address + R/W bit in the checksum (i.e. IPMB Addr).  This checksum algorithm is order-agnostic.
	msg[1] = ipmi_checksum(msg, 2);
	msg[2] = this->rqSA;
	msg[3] = (this->rqSeq << 2) | (this->rqLUN & 0x03);
	msg[4] = this->cmd;
	for (int i = 0; i < this->data_len; ++i)
		msg[5+i] = this->data[i];
	msg[5+this->data_len] = ipmi_checksum(msg+2, 3+this->data_len);
	return this->data_len + 6;
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
	reply.netFn = this->netFn | 1; // Mark as response.
	reply.cmd = this->cmd;
	reply.rqSeq = this->rqSeq;
	reply.broadcast = false;
	for (int i = 0; i < IPMI_MSG::max_data_len; ++i)
		reply.data[i] = 0;
	reply.data_len = 0;
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
 * Check if the supplied IPMB_MSG is a response to this one.
 *
 * @param response The response IPMB message to compare.
 * @return true if matching else false
 */
bool IPMI_MSG::match_reply(const IPMI_MSG &response) const {
	return (this->rqSA  == response.rsSA &&
			this->rsSA  == response.rqSA &&
			this->rqLUN == response.rsLUN &&
			this->rsLUN == response.rqLUN &&
			this->rqSeq == response.rqSeq &&
			this->netFn == (response.netFn & 0xfe) &&
			this->cmd   == response.cmd);
}

/**
 * Format this IPMB message for log output.
 *
 * @return The formatted IPMB message.
 */
std::string IPMI_MSG::format(bool describe) const {
	char buf[this->max_data_len*3 + 1] = "";
	int i = 0;
	for (i = 0; i < this->data_len; ++i)
		snprintf(buf+(i*3), 4, "%02hhx ", this->data[i]);
	if (i)
		buf[(i*3) - 1] = '\0';
	std::string out = stdsprintf("%hhd.%02hhx -> %s%hhd.%02hhx: %02hhx.%02hhx (seq %02hhx) [%s]",
				this->rqLUN, this->rqSA,
				// " -> "
				(this->broadcast ? "*" : ""),
				this->rsLUN, this->rsSA,
				// ": "
				this->netFn, this->cmd,
				/* "(seq " */ this->rqSeq /* ") " */,
				/* "[" */ buf /* "]" */);
	if (describe) {
		out += " (";
		const uint16_t netcmd = ((this->netFn & 0xFE) << 8) | this->cmd;
		if (IPMI::id_to_cmd.count(netcmd))
			out += IPMI::id_to_cmd.at(netcmd).second;
		else
			out += "Unknown Command";
		if (this->netFn & 1 && this->data_len >= 1) {
			// Completion code available.
			if (IPMI::Completion::id_to_cmplcode.count(this->data[0]))
				out += std::string("; ") + IPMI::Completion::id_to_cmplcode.at(this->data[0]);
		}
		out += ")";
	}
	return out;
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

/// \overload
uint8_t ipmi_checksum(const std::vector<uint8_t> &buf) {
	uint8_t sum = 0;
	for (auto it = buf.begin(), eit = buf.end(); it != eit; ++it)
		sum += *it;
	return (~sum) + 1;
}
