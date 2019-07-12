/*
 * SensorProcessor.cpp
 *
 *  Created on: Mar 7, 2019
 *      Author: jtikalsky
 */

#include "SensorProcessor.h"

#if XSDK_INDEXING || __has_include(<ipmi_sensor_proc.h>)

SensorProcessor::SensorProcessor(uint16_t DeviceId, uint32_t InterruptId, std::vector<ADC::Channel*> &adc_channels) :
	InterruptBasedDriver(InterruptId),
	isr_events_received("sensor_processor.isr_events_received"),
	isr_event_queue_highwater("sensor_processor.isr_event_queue_highwater"),
	userland_event_queue_highwater("sensor_processor.userland_event_queue_highwater"),
	events_delivered("sensor_processor.isr_events_delivered"),
	sensor_count(adc_channels.size()),
	adc_channel_map(adc_channels) {

	IPMI_Sensor_Proc_Initialize(&this->processor, DeviceId);
	IPMI_Sensor_Proc_Reset(&this->processor);

	// We want to be able to handle every sensor getting an event at once, and then a bit extra.
	this->isrq = xQueueCreate(this->sensor_count + this->sensor_count/2, sizeof(Event));

	// In the ISR we'll need to sample the sensors latest values
//	for (unsigned int i = 0; i < adc_channels.size(); ++i)
//		this->adc_channel_map[i] = adc_channels[i];

	this->enableInterrupts();
}

SensorProcessor::~SensorProcessor() {
	this->disableInterrupts();
	vQueueDelete(this->isrq);
}

void SensorProcessor::set_hysteresis(uint32_t channel, uint16_t hystp, uint16_t hystn) {
	Hyst_Cfg hystcfg;
	hystcfg.hyst_pos = hystp;
	hystcfg.hyst_neg = hystn;
	IPMI_Sensor_Proc_Set_Hyst(&this->processor, channel, &hystcfg);
}

void SensorProcessor::set_hysteresis(uint32_t channel, const Hyst_Cfg &hysteresis) {
	IPMI_Sensor_Proc_Set_Hyst(&this->processor, channel, &hysteresis);
}

void SensorProcessor::get_hysteresis(uint32_t channel, uint16_t &hystp, uint16_t &hystn) {
	Hyst_Cfg cfg = this->get_hysteresis(channel);
	hystp = cfg.hyst_pos;
	hystn = cfg.hyst_neg;
}

Hyst_Cfg SensorProcessor::get_hysteresis(uint32_t channel) {
	Hyst_Cfg hystcfg;
	IPMI_Sensor_Proc_Get_Hyst(&this->processor, channel, &hystcfg);
	return hystcfg;
}

void SensorProcessor::set_thresholds(uint32_t channel, uint16_t lnc, uint16_t lcr, uint16_t lnr, uint16_t unc, uint16_t ucr, uint16_t unr) {
	Thr_Cfg cfg;
	cfg.LNC = lnc;
	cfg.LCR = lcr;
	cfg.LNR = lnr;
	cfg.UNC = unc;
	cfg.UCR = ucr;
	cfg.UNR = unr;
	this->set_thresholds(channel, cfg);
}

void SensorProcessor::set_thresholds(uint32_t channel, const Thr_Cfg &thresholds) {
	IPMI_Sensor_Proc_Set_Thr(&this->processor, channel, &thresholds);
}

void SensorProcessor::get_thresholds(uint32_t channel, uint16_t &lnc, uint16_t &lcr, uint16_t &lnr, uint16_t &unc, uint16_t &ucr, uint16_t &unr) {
	Thr_Cfg cfg = this->get_thresholds(channel);
	lnc = cfg.LNC;
	lcr = cfg.LCR;
	lnr = cfg.LNR;
	unc = cfg.UNC;
	ucr = cfg.UCR;
	unr = cfg.UNR;
}

Thr_Cfg SensorProcessor::get_thresholds(uint32_t channel) {
	Thr_Cfg cfg;
	IPMI_Sensor_Proc_Get_Thr(&this->processor, channel, &cfg);
	return cfg;
}


void SensorProcessor::set_event_enable(uint32_t channel, uint16_t assert, uint16_t deassert) {
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

void SensorProcessor::get_event_enable(uint32_t channel, uint16_t &assert, uint16_t &deassert) {
	IPMI_Sensor_Proc_Get_Event_Enable(&this->processor, channel, &assert, &deassert);
}

void SensorProcessor::_InterruptHandler() {
	BaseType_t higher_priority_woken = pdFALSE;
	for (uint32_t channel = 0; channel < this->sensor_count; ++channel)  {
		Event evt;
		IPMI_Sensor_Proc_Get_Latched_Event_Status(&this->processor, channel, &evt.event_thresholds_assert, &evt.event_thresholds_deassert);
		if (evt.event_thresholds_assert || evt.event_thresholds_deassert) {
			IPMI_Sensor_Proc_Rearm_Event_Enable(&this->processor, channel, evt.event_thresholds_assert, evt.event_thresholds_deassert);
			evt.channel = channel;
			evt.reading_from_isr = this->adc_channel_map[channel]->readRaw();
			this->isr_events_received.increment();
			xQueueSendFromISR(this->isrq, &evt, &higher_priority_woken);
		}
	}
	IPMI_Sensor_Proc_Ack_IRQ(&this->processor, IPMI_Sensor_Proc_Get_IRQ_Status(&this->processor));
	this->isr_event_queue_highwater.high_water(uxQueueMessagesWaitingFromISR(this->isrq));
}

bool SensorProcessor::get_isr_event(struct Event &event, TickType_t block_time) {
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

void SensorProcessor::get_current_event_status(uint32_t channel, uint16_t &assert, uint16_t &deassert) {
	IPMI_Sensor_Proc_Get_Latched_Event_Status(&this->processor, channel, &assert, &deassert);
}

#endif
