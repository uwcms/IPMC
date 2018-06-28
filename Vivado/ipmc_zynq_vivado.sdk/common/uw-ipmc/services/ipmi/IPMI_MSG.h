/*
 * IPMI_MSG.h
 *
 *  Created on: Dec 22, 2017
 *      Author: jtikalsky
 */

#ifndef SRC_COMMON_UW_IPMC_SERVICES_IPMI_IPMI_MSG_H_
#define SRC_COMMON_UW_IPMC_SERVICES_IPMI_IPMI_MSG_H_

#include <stdint.h>
#include <string>
#include <vector>

/**
 * An IPMB Message record.
 *
 * This contains an IPMI command, complete with all relevant headers for IPMB-0
 * transit, as well as parsing and construction functions.
 */
class IPMI_MSG {
public:
	static const uint8_t max_data_len = 32-5; ///< The max length of command data.  (IPMI message max length 32, minus 5 (maybe 6?, but conservative.) bytes of header)
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

	bool parse_message(uint8_t *msg, uint8_t len, uint8_t LocalIPMBAddress);
	int unparse_message(uint8_t *msg, uint8_t maxlen) const;
	void prepare_reply(IPMI_MSG &reply) const;
	bool match(const IPMI_MSG &other) const;
	bool match_reply(const IPMI_MSG &response) const;
	std::string format() const;

	IPMI_MSG() { };
	IPMI_MSG(uint8_t rqLUN, uint8_t rqSA, uint8_t rsLUN, uint8_t rsSA, uint8_t netFn, uint8_t cmd, const std::vector<uint8_t> &data);
};

uint8_t ipmi_checksum(const uint8_t* buf, uint32_t len);
uint8_t ipmi_checksum(const std::vector<uint8_t> &buf);

#endif /* SRC_COMMON_UW_IPMC_SERVICES_IPMI_IPMI_MSG_H_ */
