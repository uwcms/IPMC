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

#ifndef SRC_COMMON_ZYNQIPMC_DRIVERS_IPMB_IPMB_PAIR_H_
#define SRC_COMMON_ZYNQIPMC_DRIVERS_IPMB_IPMB_PAIR_H_

#include <drivers/generics/ipmb.h>
#include <libs/LogTree.h>

/**
 * An IPMB pair, bonded and used as a single IPMB, such as IPMB-0 in ATCA.
 */
class IPMBPair : public IPMB {
public:
	/**
	 * Instantiate an IPMBPair, from two provided subordinate IPMBs.
	 *
	 * @param ipmbA Subordinate IPMB 0.
	 * @param ipmbB Subordinate IPMB 1.
	 * @param log   An optional logtree for request dispatch tracking.
	 */
	IPMBPair(IPMB *ipmbA, IPMB *ipmbB, LogTree *log = nullptr)
		: IPMB(), ipmb{ipmbA, ipmbB}, ipmb_enabled{true, true}, log(log) { };
	virtual ~IPMBPair() { };

	virtual void setIncomingMessageQueue(QueueHandle_t incoming_message_queue);

	/**
	 * This function will send a message out on the correct IPMB in a blocking manner.
	 *
	 * The sub-IPMB used for a transmission is chosen by a rudimentary hash algorithm
	 * and will be switched between retries of the same message.  If one IPMB is
	 * disabled, it will not be used and all messages will be directed to the other.
	 * If both IPMBs are disabled, this will return false immediately.
	 *
	 * @param msg   The IPMI_MSG to deliver.
	 * @param retry The retry count for this message.
	 * @return      true if message was delivered else false.
	 */
	virtual bool sendMessage(IPMI_MSG &msg, uint32_t retry = 0);

private:
	IPMB *ipmb[2];        ///< The IPMBs bonded into this configuration.
	bool ipmb_enabled[2]; ///< Whether each IPMB of the pair is enabled.
	LogTree *log;         ///< An optional logtree for request dispatch tracking.
};

#endif /* SRC_COMMON_ZYNQIPMC_DRIVERS_IPMB_IPMB_PAIR_H_ */
