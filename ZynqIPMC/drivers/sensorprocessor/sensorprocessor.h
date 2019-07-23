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

#ifndef SRC_COMMON_ZYNQIPMC_DRIVERS_SENSORPROCESSOR_SENSORPROCESSOR_H_
#define SRC_COMMON_ZYNQIPMC_DRIVERS_SENSORPROCESSOR_SENSORPROCESSOR_H_

// Only include driver if SensorProcessor is in the BSP.
#if XSDK_INDEXING || __has_include("ipmi_sensor_proc.h")

#include "ipmi_sensor_proc.h"
#include <FreeRTOS.h>
#include <stdint.h>
#include <queue.h>
#include <deque>
#include <vector>
#include <drivers/interrupt_based_driver.h>
#include <drivers/generics/adc.h>
#include <libs/statcounter/statcounter.h>

/**
 * Driver for the custom ZYNQ-IPMC SensorProc firmware IP.
 *
 * The IP receives sensor data from multiple sources and every time there is a new
 * value it will do hysteresis checks to be sure the new value is within set ranges.
 * If it is not, then it will generate an IRQ, letting the software know a threshold has been
 * reached.
 *
 * The IP is mostly used to quickly react to sensor changes with minimal software
 * overhead. The IP indicates faults that can be wired to a Management Zone controller.
 */
class SensorProcessor final : protected InterruptBasedDriver {
public:
	/**
	 * Creates and initializes the Sensor Processor driver for target IP.
	 * @param device_id Device ID, normally XPAR_IPMI_SENSOR_PROC_<>_DEVICE_ID. Check xparameters.h.
	 * @param intr_id Device interrupt ID from GIC, normally XPAR_FABRIC_IPMI_SENSOR_PROC_<>_IRQ_O_INTR. Check xparameters.h.
	 * @param adc_channels Array with ADC channels that are meant to be kept track of.
	 * @throw except::hardware_error if low level driver fails to configure.
	 */
	SensorProcessor(uint16_t device_id, uint16_t intr_id, std::vector<ADC::Channel*> &adc_channels);
	~SensorProcessor();

	/**
	 * Set the hysteresis values for a given channel.
	 * The hysteresis is defined as the range in raw units from the
	 * reading nominal threshold to cause a state change.
	 *
	 * For example, if the nominal reading is 100 and hyst_pos is set to 5 and
	 * hyst_neg is set to 10 then it will take a reading of 105 and 90 to cause a
	 * state change.
	 *
	 * @param channel Target channel.
	 * @param hyst_pos Upper hysteresis value.
	 * @param hyst_neg Lower hysteresis value.
	 * @throw std::out_of_range if target channel is out-of-range.
	 */
	void setHysteresis(size_t channel, uint16_t hyst_pos, uint16_t hyst_neg);
	void setHysteresis(size_t channel, const Hyst_Cfg &hysteresis);

	/**
	 * Retrieve the current hysteresis values from the IP.
	 * @param channel Target channel.
	 * @param hyst_pos Upper hysteresis value.
	 * @param hyst_neg Lower hysteresis value.
	 * @throw std::out_of_range if target channel is out-of-range.
	 */
	void getHysteresis(size_t channel, uint16_t &hyst_pos, uint16_t &hyst_neg) const;
	Hyst_Cfg getHysteresis(size_t channel) const;

	/**
	 * Set all three thresholds for a specific channel.
	 * @param channel Target channel.
	 * @param lnc Lower non-critical threshold.
	 * @param lcr Lower critical threshold.
	 * @param lnr Lower non-recoverable threshold.
	 * @param unc Upper non-critical threshold.
	 * @param ucr Upper critical threshold.
	 * @param unr Upper non-recoverable threshold.
	 * @throw std::out_of_range if target channel is out-of-range.
	 */
	void setThresholds(size_t channel, uint16_t lnc, uint16_t lcr, uint16_t lnr, uint16_t unc, uint16_t ucr, uint16_t unr);
	void setThresholds(size_t channel, const Thr_Cfg &thresholds);

	/**
	 * Retrieve all three thresholds for a specific channel.
	 * @param channel Target channel.
	 * @param lnc Lower non-critical threshold.
	 * @param lcr Lower critical threshold.
	 * @param lnr Lower non-recoverable threshold.
	 * @param unc Upper non-critical threshold.
	 * @param ucr Upper critical threshold.
	 * @param unr Upper non-recoverable threshold.
	 * @throw std::out_of_range if target channel is out-of-range.
	 */
	void getThresholds(size_t channel, uint16_t &lnc, uint16_t &lcr, uint16_t &lnr, uint16_t &unc, uint16_t &ucr, uint16_t &unr) const;
	Thr_Cfg getThresholds(size_t channel) const;

	// TODO: Discriminate the assert bits
	/**
	 * Enable or disable specific events from a channel.
	 * @param channel Target channel.
	 * @param assert Assertion event mask.
	 * @param deassert Deassertion event mask.
	 * @throw std::out_of_range if target channel is out-of-range.
	 */
	void setEventEnable(size_t channel, uint16_t assert, uint16_t deassert);

	/**
	 * Get currently enabled events from a specific channel.
	 * @param channel Target channel.
	 * @param assert Assertion event mask.
	 * @param deassert Deassertion event mask.
	 */
	void getEventEnable(size_t channel, uint16_t &assert, uint16_t &deassert) const;

	//! Used to retrieve specific events from the ISR
	struct Event {
		size_t   channel;					///< Channel that triggered the event.
		uint16_t reading_from_isr;			///< Sensor raw reading.
		uint16_t event_thresholds_assert;	///< Asserted events.
		uint16_t event_thresholds_deassert;	///< Deasserted events.
	};

	/**
	 * Retrieve or wait for an event to take place.
	 * @param event Where the event data will be stored.
	 * @param block_time Timeout in ticks to wait for an event.
	 * @return false if no event is available, true otherwise.
	 */
	bool getISREvent(struct Event &event, TickType_t block_time);

private:
	virtual void _InterruptHandler();

	IPMI_Sensor_Proc processor;		///< Sensor processor IP to operate with.
	const uint32_t kSensorCount;	///< Number of sensors connected to the processor.
	QueueHandle_t isrq;				///< Delivery queue for ISR events.
	std::deque<Event> events;		///< Cache of events received from the ISR but not yet handled.
	std::vector<ADC::Channel*> adc_channel_map;	///< Mapping of sensor processor id to ADC::Channel for in-ISR readout.

	StatCounter isr_events_received;
	StatCounter isr_event_queue_highwater;
	StatCounter userland_event_queue_highwater;
	StatCounter events_delivered;
};

#endif

#endif /* SRC_COMMON_ZYNQIPMC_DRIVERS_SENSORPROCESSOR_SENSORPROCESSOR_H_ */
