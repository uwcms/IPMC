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

// Only include driver if SensorProcessor is in the BSP.
#if XSDK_INDEXING || __has_include("ipmi_sensor_proc.h")

#include "sensorprocessor.h"

SensorProcessor::SensorProcessor(uint16_t device_id, uint16_t intr_id, std::vector<ADC::Channel*> &adc_channels) :
InterruptBasedDriver(intr_id),
kSensorCount(adc_channels.size()),
adc_channel_map(adc_channels),
isr_events_received("sensor_processor.isr_events_received"),
isr_event_queue_highwater("sensor_processor.isr_event_queue_highwater"),
userland_event_queue_highwater("sensor_processor.userland_event_queue_highwater"),
events_delivered("sensor_processor.isr_events_delivered") {

	IPMI_Sensor_Proc_Initialize(&this->processor, device_id);
	IPMI_Sensor_Proc_Reset(&this->processor);

	// We want to be able to handle every sensor getting an event at once, and then a bit extra.
	this->isrq = xQueueCreate(this->kSensorCount + this->kSensorCount/2, sizeof(Event));

	// In the ISR we'll need to sample the sensors latest values
//	for (unsigned int i = 0; i < adc_channels.size(); ++i)
//		this->adc_channel_map[i] = adc_channels[i];

	this->enableInterrupts();
}

SensorProcessor::~SensorProcessor() {
	this->disableInterrupts();
	vQueueDelete(this->isrq);
}

void SensorProcessor::setHysteresis(size_t channel, uint16_t hyst_pos, uint16_t hyst_neg) {
	Hyst_Cfg hystcfg;
	hystcfg.hyst_pos = hyst_pos;
	hystcfg.hyst_neg = hyst_neg;
	IPMI_Sensor_Proc_Set_Hyst(&this->processor, channel, &hystcfg);
}

void SensorProcessor::setHysteresis(size_t channel, const Hyst_Cfg &hysteresis) {
	if (IPMI_Sensor_Proc_Set_Hyst(&this->processor, channel, &hysteresis) != XST_SUCCESS) {
		throw std::out_of_range("Target channel is out-of-range");
	}
}

void SensorProcessor::getHysteresis(size_t channel, uint16_t &hyst_pos, uint16_t &hyst_neg) const {
	Hyst_Cfg cfg = this->getHysteresis(channel);
	hyst_pos = cfg.hyst_pos;
	hyst_neg = cfg.hyst_neg;
}

Hyst_Cfg SensorProcessor::getHysteresis(size_t channel) const {
	Hyst_Cfg hystcfg;

	if (IPMI_Sensor_Proc_Get_Hyst(&this->processor, channel, &hystcfg) != XST_SUCCESS) {
		throw std::out_of_range("Target channel is out-of-range");
	}

	return hystcfg;
}

void SensorProcessor::setThresholds(size_t channel, uint16_t lnc, uint16_t lcr, uint16_t lnr, uint16_t unc, uint16_t ucr, uint16_t unr) {
	Thr_Cfg cfg;
	cfg.LNC = lnc;
	cfg.LCR = lcr;
	cfg.LNR = lnr;
	cfg.UNC = unc;
	cfg.UCR = ucr;
	cfg.UNR = unr;
	this->setThresholds(channel, cfg);
}

void SensorProcessor::setThresholds(size_t channel, const Thr_Cfg &thresholds) {
	if (IPMI_Sensor_Proc_Set_Thr(&this->processor, channel, &thresholds) != XST_SUCCESS) {
		throw std::out_of_range("Target channel is out-of-range");
	}
}

void SensorProcessor::getThresholds(size_t channel,
		uint16_t &lnc, uint16_t &lcr, uint16_t &lnr,
		uint16_t &unc, uint16_t &ucr, uint16_t &unr) const {
	Thr_Cfg cfg = this->getThresholds(channel);
	lnc = cfg.LNC;
	lcr = cfg.LCR;
	lnr = cfg.LNR;
	unc = cfg.UNC;
	ucr = cfg.UCR;
	unr = cfg.UNR;
}

Thr_Cfg SensorProcessor::getThresholds(size_t channel) const {
	Thr_Cfg cfg;

	if (IPMI_Sensor_Proc_Get_Thr(&this->processor, channel, &cfg) != XST_SUCCESS) {
		throw std::out_of_range("Target channel is out-of-range");
	}

	return cfg;
}


void SensorProcessor::setEventEnable(size_t channel, uint16_t assert, uint16_t deassert) {
	if (channel >= this->processor.sensor_ch_cnt) {
		throw std::out_of_range("Target channel is out-of-range");
	}

	uint16_t old_assert, old_deassert;
	IPMI_Sensor_Proc_Get_Event_Enable(&this->processor, channel, &old_assert, &old_deassert);
	/* Rearm/clear any about to be enabled events, so old state doesn't end up
	 * confusing anything.  I'm not positive if this is necessary, but it won't
	 * hurt.
	 *
	 * The Sensor Processor IP will reassert any events active at the moment that
	 * they are enabled.
	 */
	IPMI_Sensor_Proc_Rearm_Event_Enable(&this->processor, channel, assert & ~old_assert, deassert & ~old_deassert);
	IPMI_Sensor_Proc_Set_Event_Enable(&this->processor, channel, assert, deassert);
}

void SensorProcessor::getEventEnable(size_t channel, uint16_t &assert, uint16_t &deassert) const {
	if (IPMI_Sensor_Proc_Get_Event_Enable(&this->processor, channel, &assert, &deassert) != XST_SUCCESS) {
		throw std::out_of_range("Target channel is out-of-range");
	}
}

void SensorProcessor::_InterruptHandler() {
	BaseType_t higher_priority_woken = pdFALSE;
	for (size_t channel = 0; channel < this->kSensorCount; ++channel)  {
		Event evt;
		IPMI_Sensor_Proc_Get_Latched_Event_Status(&this->processor, channel, &evt.event_thresholds_assert, &evt.event_thresholds_deassert);
		if (evt.event_thresholds_assert || evt.event_thresholds_deassert) {
			IPMI_Sensor_Proc_Rearm_Event_Enable(&this->processor, channel, evt.event_thresholds_assert, evt.event_thresholds_deassert);
			evt.channel = channel;
			// TODO: If this runs here it means that readRaw need to be ISR safe??
			evt.reading_from_isr = this->adc_channel_map[channel]->readRaw();
			this->isr_events_received.increment();
			xQueueSendFromISR(this->isrq, &evt, &higher_priority_woken);
		}
	}
	IPMI_Sensor_Proc_Ack_IRQ(&this->processor, IPMI_Sensor_Proc_Get_IRQ_Status(&this->processor));
	this->isr_event_queue_highwater.high_water(uxQueueMessagesWaitingFromISR(this->isrq));
}

bool SensorProcessor::getISREvent(struct Event &event, TickType_t block_time) {
	bool ret;
	do {
		Event evt;
		ret = xQueueReceive(this->isrq, &evt, block_time);
		if (ret)
			this->events.push_back(evt);
		this->userland_event_queue_highwater.high_water(this->events.size());
		block_time = 0; // Don't block for further extractions, but we want to clear the inelastic RTOS queue into our elastic deque.
	} while (ret == pdTRUE);
	if (this->events.empty())
		return false;
	event = this->events.front();
	this->events.pop_front();
	this->events_delivered.increment();
	return true;
}

#endif
