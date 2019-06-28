/*
 * SensorProcessor.h
 *
 *  Created on: Mar 7, 2019
 *      Author: jtikalsky
 */

#ifndef SRC_COMMON_UW_IPMC_DRIVERS_SENSORPROCESSOR_SENSORPROCESSOR_H_
#define SRC_COMMON_UW_IPMC_DRIVERS_SENSORPROCESSOR_SENSORPROCESSOR_H_

#if XSDK_INDEXING  || __has_include(<ipmi_sensor_proc.h>)
#define SENSORPROCESSOR_DRIVER_INCLUDED

#include <stdint.h>
#include <ipmi_sensor_proc.h>
#include <FreeRTOS.h>
#include <queue.h>
#include <deque>
#include <vector>
#include <drivers/InterruptBasedDriver.h>
#include <drivers/generics/ADC.h>
#include <libs/StatCounter.h>

class SensorProcessor : protected InterruptBasedDriver {
public:
	SensorProcessor(uint16_t DeviceId, uint32_t InterruptId, std::vector<ADC::Channel*> &adc_channels);
	virtual ~SensorProcessor();

	void set_hysteresis(uint32_t channel, uint16_t hystp, uint16_t hystn);
	void set_hysteresis(uint32_t channel, const Hyst_Cfg &hysteresis);
	void get_hysteresis(uint32_t channel, uint16_t &hystp, uint16_t &hystn);
	Hyst_Cfg get_hysteresis(uint32_t channel);

	void set_thresholds(uint32_t channel, uint16_t lnc, uint16_t lcr, uint16_t lnr, uint16_t unc, uint16_t ucr, uint16_t unr);
	void set_thresholds(uint32_t channel, const Thr_Cfg &thresholds);
	void get_thresholds(uint32_t channel, uint16_t &lnc, uint16_t &lcr, uint16_t &lnr, uint16_t &unc, uint16_t &ucr, uint16_t &unr);
	Thr_Cfg get_thresholds(uint32_t channel);

	void set_event_enable(uint32_t channel, uint16_t assert, uint16_t deassert);
	void get_event_enable(uint32_t channel, uint16_t &assert, uint16_t &deassert);

	struct Event {
		uint32_t channel;
		uint16_t reading_from_isr;
		uint16_t event_thresholds_assert;
		uint16_t event_thresholds_deassert;
	};

	bool get_isr_event(struct Event &event, TickType_t block_time);

	void get_current_event_status(uint32_t channel, uint16_t &assert, uint16_t &deassert);

	StatCounter isr_events_received;
	StatCounter isr_event_queue_highwater;
	StatCounter userland_event_queue_highwater;
	StatCounter events_delivered;

protected:

	virtual void _InterruptHandler();

	IPMI_Sensor_Proc processor; ///< The sensor processor IP to operate with.
	const uint32_t sensor_count; ///< The number of sensors connected to the processor.
	QueueHandle_t isrq; ///< A delivery queue for ISR events
	std::deque<Event> events; ///< A cache of events received from the ISR but not yet handled.
	std::vector<ADC::Channel*> adc_channel_map; ///< A mapping of sensor processor id to ADC::Channel for in-ISR readout.
};

#endif

#endif /* SRC_COMMON_UW_IPMC_DRIVERS_SENSORPROCESSOR_SENSORPROCESSOR_H_ */
