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

#include <libs/printf.h>
#include <libs/except.h>
#include <services/ipmi/ipmi.h>
#include <services/ipmi/ipmi_message.h>

IPMIMessage::IPMIMessage()
	: rsSA(0), netFn(0), rsLUN(0), rqSA(0), rqSeq(0), rqLUN(0), cmd(0), data_len(0), broadcast(false), duplicate(false) {
	for (std::vector<uint8_t>::size_type i = 0; i < this->max_data_len; ++i) {
		this->data[i] = 0;
	}
};

IPMIMessage::IPMIMessage(uint8_t rqLUN, uint8_t rqSA, uint8_t rsLUN, uint8_t rsSA, uint8_t netFn, uint8_t cmd, const std::vector<uint8_t> &data)
	: rsSA(rsSA), netFn(netFn), rsLUN(rsLUN), rqSA(rqSA), rqSeq(0), rqLUN(rqLUN), cmd(cmd), data_len(data.size()), broadcast(false), duplicate(false) {
	if (data.size() > max_data_len) {
		throw std::domain_error(stdsprintf("Only up to %hhu bytes of IPMI message data are supported.", max_data_len));
	}
	unsigned int i = 0;
	for (; i < data.size(); ++i) {
		this->data[i] = data[i];
	}

	for (; i < IPMIMessage::max_data_len; ++i) {
		this->data[i] = 0;
	}

	this->data_len = data.size();
};

bool IPMIMessage::parseMessage(uint8_t *msg, uint8_t len, uint8_t LocalIPMBAddress) {
	this->broadcast = false;
	if (len && msg[0] == 0) {
		// Broadcast Message!  ... Remove the leading 0x00.
		// For details see IPMI2 spec, "Figure 20-1, Broadcast Get Device ID Request Message"
		this->broadcast = true;
		++msg;
		--len;
	}

	if (len < 6) return false; // Invalid.

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
	if (IPMIMessage::checksum(hdrsumbuf, 3)) return false;
	if (IPMIMessage::checksum(msg+2, len-2)) return false;
	return true;
}

int IPMIMessage::unparseMessage(uint8_t *msg, uint8_t maxlen) const {
	if (maxlen < this->data_len+7) {
		return -1;
	}

	msg[0] = (this->netFn << 2) | (this->rsLUN & 0x03);
	msg[1] = this->rsSA; // We need to include the I2C address + R/W bit in the checksum (i.e. IPMB Addr).  This checksum algorithm is order-agnostic.
	msg[1] = IPMIMessage::checksum(msg, 2);
	msg[2] = this->rqSA;
	msg[3] = (this->rqSeq << 2) | (this->rqLUN & 0x03);
	msg[4] = this->cmd;
	for (int i = 0; i < this->data_len; ++i) {
		msg[5+i] = this->data[i];
	}
	msg[5+this->data_len] = IPMIMessage::checksum(msg+2, 3+this->data_len);
	return this->data_len + 6;
}

void IPMIMessage::prepareReply(IPMIMessage &reply, const std::vector<uint8_t> &reply_data) const {
	if (reply_data.size() > max_data_len)
		throw std::domain_error(stdsprintf("Only up to %hhu bytes of IPMI message data are supported.", max_data_len));
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
	unsigned int i = 0;
	for (; i < reply_data.size(); ++i) {
		reply.data[i] = reply_data[i];
	}
	for (; i < IPMIMessage::max_data_len; ++i) {
		reply.data[i] = 0;
	}
	reply.data_len = reply_data.size();
}

/// \overload
std::shared_ptr<IPMIMessage> IPMIMessage::prepareReply(const std::vector<uint8_t> &reply_data) const {
	std::shared_ptr<IPMIMessage> rpl = std::make_shared<IPMIMessage>(this->rsLUN, this->rsSA, this->rqLUN, this->rqSA, this->netFn|1, this->cmd, reply_data);
	rpl->rqSeq = this->rqSeq;
	return rpl;
}

bool IPMIMessage::match(const IPMIMessage &other) const {
	return (this->rqSA  == other.rqSA &&
			this->rsSA  == other.rsSA &&
			this->rqLUN == other.rqLUN &&
			this->rsLUN == other.rsLUN &&
			this->rqSeq == other.rqSeq &&
			this->netFn == other.netFn &&
			this->cmd   == other.cmd);
}

bool IPMIMessage::matchReply(const IPMIMessage &response) const {
	return (this->rqSA  == response.rsSA &&
			this->rsSA  == response.rqSA &&
			this->rqLUN == response.rsLUN &&
			this->rsLUN == response.rqLUN &&
			this->rqSeq == response.rqSeq &&
			this->netFn == (response.netFn & 0xfe) &&
			this->cmd   == response.cmd);
}

std::string IPMIMessage::format(bool describe) const {
	char buf[this->max_data_len*3 + 1] = "";
	int i = 0;
	for (i = 0; i < this->data_len; ++i) {
		snprintf(buf+(i*3), 4, "%02hhx ", this->data[i]);
	}
	if (i) buf[(i*3) - 1] = '\0';
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

		if (IPMI::id_to_cmd.count(netcmd)) {
			out += IPMI::id_to_cmd.at(netcmd).second;
		} else {
			out += "Unknown Command";
		}

		if (this->netFn & 1 && this->data_len >= 1) {
			// Completion code available.
			if (IPMI::Completion::id_to_cmplcode.count(this->data[0]))
				out += std::string("; ") + IPMI::Completion::id_to_cmplcode.at(this->data[0]);
		}
		out += ")";
	}
	return out;
}

uint8_t IPMIMessage::checksum(const uint8_t* buf, uint32_t len) {
	uint8_t sum = 0;

	while (len) {
		sum += *buf++;
		len--;
	}
	return (~sum) + 1;
}

/// \overload
uint8_t IPMIMessage::checksum(const std::vector<uint8_t> &buf) {
	uint8_t sum = 0;

	for (auto it = buf.begin(), eit = buf.end(); it != eit; ++it) {
		sum += *it;
	}

	return (~sum) + 1;
}
