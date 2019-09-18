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

/**
 * @file sdr_init.cpp
 *
 * Code related to IPMI SDR initialization goes here.
 *
 * mcdlr and hotswap are always mandatory and can be modified in code.
 *
 * All other records can be generated using the webpage tool present in
 * the repository by copy pasting from one side to the other.
 *
 * The IPMI SDR repository (device_sdr_repo) is already initialized in
 * the core and will be available when this code runs.
 *
 * Each sensor in the target application should have a dedicated SDR.
 */

// TODO
#include <core.h>
#include <FreeRTOS.h>
#include <payload_manager.h>
#include <services/ipmi/ipmbsvc/ipmbsvc.h>
#include <services/ipmi/m_state_machine.h>

#include <services/ipmi/sdr/sensor_data_record_01.h>
#include <services/ipmi/sdr/sensor_data_record_02.h>
#include <services/ipmi/sdr/sensor_data_record_12.h>
#include <services/ipmi/sdr/sensor_data_repository.h>
#include <services/ipmi/sensor/hotswap_sensor.h>
#include <services/ipmi/sensor/sensor.h>
#include <services/ipmi/sensor/sensor_set.h>
#include <services/ipmi/sensor/severity_sensor.h>
#include <services/ipmi/sensor/threshold_sensor.h>
#include <services/persistentstorage/persistent_storage.h>
#include "ipmc.h"

/**
 * Local function to help add sensor records to the repository.
 * @param repo Target repository to add sensors to.
 * @param sdr The sensor data record that will get added.
 * @param reservation First reservation id, will be replaced with the next one after update.
 */
static void addToSDRRepo(SensorDataRepository &repo, const SensorDataRecord &sdr, SensorDataRepository::reservation_t &reservation) {
	while (true) {
		try {
			repo.add(sdr, reservation);
			return;
		}
		catch (SensorDataRepository::reservation_cancelled_error) {
			reservation = repo.reserve();
		}
	}
}

//! Initialize Device SDRs for this controller.
void initDeviceSDRs(bool reinit) {
	SensorDataRepository::reservation_t reservation = device_sdr_repo.reserve();

#define ADD_TO_REPO(sdr) addToSDRRepo(device_sdr_repo, sdr, reservation)

	{
		// Management Controller Device Locator Record for ourself.
		SensorDataRecord12 mcdlr;
		mcdlr.initializeBlank("UW ZYNQ IPMC");
		mcdlr.device_slave_address(ipmb0->getIPMBAddress());
		mcdlr.channel(0);
		mcdlr.acpi_device_power_state_notification_required(false);
		mcdlr.acpi_system_power_state_notification_required(false);
		mcdlr.is_static(false);
		mcdlr.init_agent_logs_errors(false);
		mcdlr.init_agent_log_errors_accessing_this_controller(false);
		mcdlr.init_agent_init_type(SensorDataRecord12::INIT_ENABLE_EVENTS);
		mcdlr.cap_chassis_device(false);
		mcdlr.cap_bridge(false);
		mcdlr.cap_ipmb_event_generator(true);
		mcdlr.cap_ipmb_event_receiver(true); // Possibly not required.  See also Get PICMG Properties code.
		mcdlr.cap_fru_inventory_device(true);
		mcdlr.cap_sel_device(false);
		mcdlr.cap_sdr_repository_device(true);
		mcdlr.cap_sensor_device(true);
		mcdlr.entity_id(0xA0);
		mcdlr.entity_instance(0x60);
		ADD_TO_REPO(mcdlr);
	}

	{
		SensorDataRecord02 hotswap;
		hotswap.initializeBlank("Hotswap");
		hotswap.sensor_owner_id(0); // Tag as "self". This will be auto-calculated in "Get SDR" commands.
		hotswap.sensor_owner_channel(0); // See above.
		hotswap.sensor_owner_lun(0); // See above.
		hotswap.sensor_number(1);
		hotswap.entity_id(0xA0);
		hotswap.entity_instance(0x60);
		hotswap.events_enabled_default(true);
		hotswap.scanning_enabled_default(true);
		hotswap.sensor_auto_rearm(true);
		hotswap.sensor_hysteresis_support(SensorDataRecordReadableSensor::ACCESS_READWRITE);
		hotswap.sensor_threshold_access_support(SensorDataRecordReadableSensor::ACCESS_READWRITE);
		hotswap.sensor_event_message_control_support(SensorDataRecordReadableSensor::EVTCTRL_GRANULAR);
		hotswap.sensor_type_code(0xf0); // Hotswap
		hotswap.event_type_reading_code(0x6f); // Sensor-specific discrete
		hotswap.assertion_lower_threshold_reading_mask(0x00ff); // M7:M0
		hotswap.deassertion_upper_threshold_reading_mask(0); // M7:M0
		hotswap.discrete_reading_setable_threshold_reading_mask(0x00ff); // M7:M0
		// I don't need to specify unit type codes for this sensor.
		ADD_TO_REPO(hotswap);
		if (!ipmc_sensors.get(hotswap.sensor_number()))
			ipmc_sensors.add(std::make_shared<HotswapSensor>(hotswap.recordKey(), LOG["sensors"]["Hotswap"]));
	}

	{
		SensorDataRecord01 sensor;
		// SDR Calculator.html#precision=3&s-na=%2B12VPYLD&s-no=2&s-t=0x02&s-u-p=4&lnrf=10.8&lcrf=11.1&lncf=11.4&uncf=12.6&ucrf=12.9&unrf=13.2&nominalf=12&hystpi=1&hystni=1&minf=0&granularity=0.06
		sensor.initializeBlank("+12VPYLD");
		sensor.sensor_owner_id(0); // Tag as "self". This will be auto-calculated in "Get SDR" commands.
		sensor.sensor_owner_channel(0); // See above.
		sensor.sensor_owner_lun(0); // Generally zero
		sensor.sensor_number(2);
		sensor.entity_id(0xA0);
		sensor.entity_instance(0x60);
		//sensor.sensor_setable(false); // Default, Unsupported
		//sensor.initialize_scanning_enabled(false); // Default (An Init Agent is not required.)
		//sensor.initialize_events_enabled(false); // Default (An Init Agent is not required.)
		//sensor.initialize_thresholds(false); // Default (An Init Agent is not required.)
		//sensor.initialize_hysteresis(false); // Default (An Init Agent is not required.)
		//sensor.initialize_sensor_type(false); // Default (An Init Agent is not required.)
		sensor.ignore_if_entity_absent(true);
		sensor.events_enabled_default(true);
		sensor.scanning_enabled_default(true);
		sensor.sensor_auto_rearm(true);
		sensor.sensor_hysteresis_support(SensorDataRecordReadableSensor::ACCESS_READWRITE);
		sensor.sensor_threshold_access_support(SensorDataRecordReadableSensor::ACCESS_READWRITE);
		sensor.sensor_event_message_control_support(SensorDataRecordReadableSensor::EVTCTRL_GRANULAR);
		sensor.sensor_type_code(0x02); // Voltage
		sensor.event_type_reading_code(SensorDataRecordReadableSensor::EVENT_TYPE_THRESHOLD_SENSOR);
		sensor.threshold_comparisons_returned(0x3f); // All comparisons returned
		sensor.assertion_lower_threshold_reading_mask(0x0fff); // LNR, LCR, LNC, UNC, UCR, UNR assertions supported.
		sensor.deassertion_upper_threshold_reading_mask(0x0fff); // LNR, LCR, LNC, UNC, UCR, UNR deassertions supported.
		sensor.discrete_reading_setable_threshold_reading_mask(0x3f3f); // All thresholds are configurable.
		sensor.units_numeric_format(SensorDataRecord01::UNITS_UNSIGNED);
		sensor.units_rate_unit(SensorDataRecordReadableSensor::RATE_UNIT_NONE);
		sensor.units_base_unit(4); // Volts
		sensor.units_modifier_unit(0); // unspecified
		sensor.units_modifier_unit_method(SensorDataRecordReadableSensor::MODIFIER_UNIT_NONE);
		sensor.linearization(SensorDataRecord01::LIN_LINEAR);
		// IPMI Specifies a linearization function of: y = L[(Mx + (B * 10^(Bexp) ) ) * 10^(Rexp) ]
		// Our settings produce a valid range of 0 (Volts) to 15.3 (Volts) with 0.06 Volts granularity.
		sensor.conversion_m(6);
		sensor.conversion_b(0);
		sensor.conversion_b_exp(0);
		sensor.conversion_r_exp(-2);
		sensor.sensor_direction(SensorDataRecordReadableSensor::DIR_UNSPECIFIED);
		//sensor.normal_min_specified(false); // Default
		//sensor.normal_min_rawvalue(0); // Unspecified
		//sensor.normal_max_specified(false); // Default
		//sensor.normal_max_rawvalue(0); // Unspecified
		sensor.nominal_reading_specified(true);
		sensor.nominal_reading_rawvalue(200); // 12 Volts
		sensor.threshold_unr_rawvalue(220); // 13.2 Volts
		sensor.threshold_ucr_rawvalue(215); // 12.9 Volts
		sensor.threshold_unc_rawvalue(210); // 12.6 Volts
		sensor.threshold_lnc_rawvalue(190); // 11.4 Volts
		sensor.threshold_lcr_rawvalue(185); // 11.1 Volts
		sensor.threshold_lnr_rawvalue(180); // 10.8 Volts
		sensor.hysteresis_high(1); // +0.06 Volts
		sensor.hysteresis_low(1); // -0.06 Volts
		ADD_TO_REPO(sensor);
		if (!ipmc_sensors.get(sensor.sensor_number()))
			ipmc_sensors.add(std::make_shared<ThresholdSensor>(sensor.recordKey(), LOG["sensors"]["+12VPYLD"]));
	}

	{
		SensorDataRecord01 sensor;
		// SDR Calculator.html#precision=4&s-na=%2B5VPYLD&s-no=3&s-t=0x02&s-u-p=4&lnrf=4.5&lcrf=4.625&lncf=4.75&uncf=5.25&ucrf=5.375&unrf=5.5&nominalf=5&hystpi=1&hystni=1&minf=0&granularity=0.0226
		sensor.initializeBlank("+5VPYLD");
		sensor.sensor_owner_id(0); // Tag as "self". This will be auto-calculated in "Get SDR" commands.
		sensor.sensor_owner_channel(0); // See above.
		sensor.sensor_owner_lun(0); // Generally zero
		sensor.sensor_number(3);
		sensor.entity_id(0xA0);
		sensor.entity_instance(0x60);
		//sensor.sensor_setable(false); // Default, Unsupported
		//sensor.initialize_scanning_enabled(false); // Default (An Init Agent is not required.)
		//sensor.initialize_events_enabled(false); // Default (An Init Agent is not required.)
		//sensor.initialize_thresholds(false); // Default (An Init Agent is not required.)
		//sensor.initialize_hysteresis(false); // Default (An Init Agent is not required.)
		//sensor.initialize_sensor_type(false); // Default (An Init Agent is not required.)
		sensor.ignore_if_entity_absent(true);
		sensor.events_enabled_default(true);
		sensor.scanning_enabled_default(true);
		sensor.sensor_auto_rearm(true);
		sensor.sensor_hysteresis_support(SensorDataRecordReadableSensor::ACCESS_READWRITE);
		sensor.sensor_threshold_access_support(SensorDataRecordReadableSensor::ACCESS_READWRITE);
		sensor.sensor_event_message_control_support(SensorDataRecordReadableSensor::EVTCTRL_GRANULAR);
		sensor.sensor_type_code(0x02); // Voltage
		sensor.event_type_reading_code(SensorDataRecordReadableSensor::EVENT_TYPE_THRESHOLD_SENSOR);
		sensor.threshold_comparisons_returned(0x3f); // All comparisons returned
		sensor.assertion_lower_threshold_reading_mask(0x0fff); // LNR, LCR, LNC, UNC, UCR, UNR assertions supported.
		sensor.deassertion_upper_threshold_reading_mask(0x0fff); // LNR, LCR, LNC, UNC, UCR, UNR deassertions supported.
		sensor.discrete_reading_setable_threshold_reading_mask(0x3f3f); // All thresholds are configurable.
		sensor.units_numeric_format(SensorDataRecord01::UNITS_UNSIGNED);
		sensor.units_rate_unit(SensorDataRecordReadableSensor::RATE_UNIT_NONE);
		sensor.units_base_unit(4); // Volts
		sensor.units_modifier_unit(0); // unspecified
		sensor.units_modifier_unit_method(SensorDataRecordReadableSensor::MODIFIER_UNIT_NONE);
		sensor.linearization(SensorDataRecord01::LIN_LINEAR);
		// IPMI Specifies a linearization function of: y = L[(Mx + (B * 10^(Bexp) ) ) * 10^(Rexp) ]
		// Our settings produce a valid range of 0 (Volts) to 5.763 (Volts) with 0.0226 Volts granularity.
		sensor.conversion_m(226);
		sensor.conversion_b(0);
		sensor.conversion_b_exp(0);
		sensor.conversion_r_exp(-4);
		sensor.sensor_direction(SensorDataRecordReadableSensor::DIR_UNSPECIFIED);
		//sensor.normal_min_specified(false); // Default
		//sensor.normal_min_rawvalue(0); // Unspecified
		//sensor.normal_max_specified(false); // Default
		//sensor.normal_max_rawvalue(0); // Unspecified
		sensor.nominal_reading_specified(true);
		sensor.nominal_reading_rawvalue(221); // 5 Volts
		sensor.threshold_unr_rawvalue(243); // 5.5 Volts
		sensor.threshold_ucr_rawvalue(237); // 5.375 Volts
		sensor.threshold_unc_rawvalue(232); // 5.25 Volts
		sensor.threshold_lnc_rawvalue(210); // 4.75 Volts
		sensor.threshold_lcr_rawvalue(204); // 4.625 Volts
		sensor.threshold_lnr_rawvalue(199); // 4.5 Volts
		sensor.hysteresis_high(1); // +0.0226 Volts
		sensor.hysteresis_low(1); // -0.0226 Volts
		ADD_TO_REPO(sensor);
		if (!ipmc_sensors.get(sensor.sensor_number()))
			ipmc_sensors.add(std::make_shared<ThresholdSensor>(sensor.recordKey(), LOG["sensors"]["+5VPYLD"]));
	}

	{
		SensorDataRecord01 sensor;
		// SDR Calculator.html#precision=4&s-na=%2B3.3VPYLD&s-no=4&s-t=0x02&s-u-p=4&lnrf=2.97&lcrf=3.0525&lncf=3.135&uncf=3.465&ucrf=3.5475&unrf=3.63&nominalf=3.3&hystpi=1&hystni=1&minf=0&granularity=0.0149
		sensor.initializeBlank("+3.3VPYLD");
		sensor.sensor_owner_id(0); // Tag as "self". This will be auto-calculated in "Get SDR" commands.
		sensor.sensor_owner_channel(0); // See above.
		sensor.sensor_owner_lun(0); // Generally zero
		sensor.sensor_number(4);
		sensor.entity_id(0xA0);
		sensor.entity_instance(0x60);
		//sensor.sensor_setable(false); // Default, Unsupported
		//sensor.initialize_scanning_enabled(false); // Default (An Init Agent is not required.)
		//sensor.initialize_events_enabled(false); // Default (An Init Agent is not required.)
		//sensor.initialize_thresholds(false); // Default (An Init Agent is not required.)
		//sensor.initialize_hysteresis(false); // Default (An Init Agent is not required.)
		//sensor.initialize_sensor_type(false); // Default (An Init Agent is not required.)
		sensor.ignore_if_entity_absent(true);
		sensor.events_enabled_default(true);
		sensor.scanning_enabled_default(true);
		sensor.sensor_auto_rearm(true);
		sensor.sensor_hysteresis_support(SensorDataRecordReadableSensor::ACCESS_READWRITE);
		sensor.sensor_threshold_access_support(SensorDataRecordReadableSensor::ACCESS_READWRITE);
		sensor.sensor_event_message_control_support(SensorDataRecordReadableSensor::EVTCTRL_GRANULAR);
		sensor.sensor_type_code(0x02); // Voltage
		sensor.event_type_reading_code(SensorDataRecordReadableSensor::EVENT_TYPE_THRESHOLD_SENSOR);
		sensor.threshold_comparisons_returned(0x3f); // All comparisons returned
		sensor.assertion_lower_threshold_reading_mask(0x0fff); // LNR, LCR, LNC, UNC, UCR, UNR assertions supported.
		sensor.deassertion_upper_threshold_reading_mask(0x0fff); // LNR, LCR, LNC, UNC, UCR, UNR deassertions supported.
		sensor.discrete_reading_setable_threshold_reading_mask(0x3f3f); // All thresholds are configurable.
		sensor.units_numeric_format(SensorDataRecord01::UNITS_UNSIGNED);
		sensor.units_rate_unit(SensorDataRecordReadableSensor::RATE_UNIT_NONE);
		sensor.units_base_unit(4); // Volts
		sensor.units_modifier_unit(0); // unspecified
		sensor.units_modifier_unit_method(SensorDataRecordReadableSensor::MODIFIER_UNIT_NONE);
		sensor.linearization(SensorDataRecord01::LIN_LINEAR);
		// IPMI Specifies a linearization function of: y = L[(Mx + (B * 10^(Bexp) ) ) * 10^(Rexp) ]
		// Our settings produce a valid range of 0 (Volts) to 3.7995 (Volts) with 0.0149 Volts granularity.
		sensor.conversion_m(149);
		sensor.conversion_b(0);
		sensor.conversion_b_exp(0);
		sensor.conversion_r_exp(-4);
		sensor.sensor_direction(SensorDataRecordReadableSensor::DIR_UNSPECIFIED);
		//sensor.normal_min_specified(false); // Default
		//sensor.normal_min_rawvalue(0); // Unspecified
		//sensor.normal_max_specified(false); // Default
		//sensor.normal_max_rawvalue(0); // Unspecified
		sensor.nominal_reading_specified(true);
		sensor.nominal_reading_rawvalue(221); // 3.3 Volts
		sensor.threshold_unr_rawvalue(243); // 3.63 Volts
		sensor.threshold_ucr_rawvalue(238); // 3.5475 Volts
		sensor.threshold_unc_rawvalue(232); // 3.465 Volts
		sensor.threshold_lnc_rawvalue(210); // 3.135 Volts
		sensor.threshold_lcr_rawvalue(204); // 3.0525 Volts
		sensor.threshold_lnr_rawvalue(199); // 2.97 Volts
		sensor.hysteresis_high(1); // +0.0149 Volts
		sensor.hysteresis_low(1); // -0.0149 Volts
		ADD_TO_REPO(sensor);
		if (!ipmc_sensors.get(sensor.sensor_number()))
			ipmc_sensors.add(std::make_shared<ThresholdSensor>(sensor.recordKey(), LOG["sensors"]["+3.3VPYLD"]));
	}

	{
		SensorDataRecord01 sensor;
		// SDR Calculator.html#precision=4&s-na=%2B3.3VMP&s-no=5&s-t=0x02&s-u-p=4&lnrf=2.97&lcrf=3.0525&lncf=3.135&uncf=3.465&ucrf=3.5475&unrf=3.63&nominalf=3.3&hystpi=1&hystni=1&minf=0&granularity=0.0149
		sensor.initializeBlank("+3.3VMP");
		sensor.sensor_owner_id(0); // Tag as "self". This will be auto-calculated in "Get SDR" commands.
		sensor.sensor_owner_channel(0); // See above.
		sensor.sensor_owner_lun(0); // Generally zero
		sensor.sensor_number(5);
		sensor.entity_id(0xA0);
		sensor.entity_instance(0x60);
		//sensor.sensor_setable(false); // Default, Unsupported
		//sensor.initialize_scanning_enabled(false); // Default (An Init Agent is not required.)
		//sensor.initialize_events_enabled(false); // Default (An Init Agent is not required.)
		//sensor.initialize_thresholds(false); // Default (An Init Agent is not required.)
		//sensor.initialize_hysteresis(false); // Default (An Init Agent is not required.)
		//sensor.initialize_sensor_type(false); // Default (An Init Agent is not required.)
		sensor.ignore_if_entity_absent(true);
		sensor.events_enabled_default(true);
		sensor.scanning_enabled_default(true);
		sensor.sensor_auto_rearm(true);
		sensor.sensor_hysteresis_support(SensorDataRecordReadableSensor::ACCESS_READWRITE);
		sensor.sensor_threshold_access_support(SensorDataRecordReadableSensor::ACCESS_READWRITE);
		sensor.sensor_event_message_control_support(SensorDataRecordReadableSensor::EVTCTRL_GRANULAR);
		sensor.sensor_type_code(0x02); // Voltage
		sensor.event_type_reading_code(SensorDataRecordReadableSensor::EVENT_TYPE_THRESHOLD_SENSOR);
		sensor.threshold_comparisons_returned(0x3f); // All comparisons returned
		sensor.assertion_lower_threshold_reading_mask(0x0fff); // LNR, LCR, LNC, UNC, UCR, UNR assertions supported.
		sensor.deassertion_upper_threshold_reading_mask(0x0fff); // LNR, LCR, LNC, UNC, UCR, UNR deassertions supported.
		sensor.discrete_reading_setable_threshold_reading_mask(0x3f3f); // All thresholds are configurable.
		sensor.units_numeric_format(SensorDataRecord01::UNITS_UNSIGNED);
		sensor.units_rate_unit(SensorDataRecordReadableSensor::RATE_UNIT_NONE);
		sensor.units_base_unit(4); // Volts
		sensor.units_modifier_unit(0); // unspecified
		sensor.units_modifier_unit_method(SensorDataRecordReadableSensor::MODIFIER_UNIT_NONE);
		sensor.linearization(SensorDataRecord01::LIN_LINEAR);
		// IPMI Specifies a linearization function of: y = L[(Mx + (B * 10^(Bexp) ) ) * 10^(Rexp) ]
		// Our settings produce a valid range of 0 (Volts) to 3.7995 (Volts) with 0.0149 Volts granularity.
		sensor.conversion_m(149);
		sensor.conversion_b(0);
		sensor.conversion_b_exp(0);
		sensor.conversion_r_exp(-4);
		sensor.sensor_direction(SensorDataRecordReadableSensor::DIR_UNSPECIFIED);
		//sensor.normal_min_specified(false); // Default
		//sensor.normal_min_rawvalue(0); // Unspecified
		//sensor.normal_max_specified(false); // Default
		//sensor.normal_max_rawvalue(0); // Unspecified
		sensor.nominal_reading_specified(true);
		sensor.nominal_reading_rawvalue(221); // 3.3 Volts
		sensor.threshold_unr_rawvalue(243); // 3.63 Volts
		sensor.threshold_ucr_rawvalue(238); // 3.5475 Volts
		sensor.threshold_unc_rawvalue(232); // 3.465 Volts
		sensor.threshold_lnc_rawvalue(210); // 3.135 Volts
		sensor.threshold_lcr_rawvalue(204); // 3.0525 Volts
		sensor.threshold_lnr_rawvalue(199); // 2.97 Volts
		sensor.hysteresis_high(1); // +0.0149 Volts
		sensor.hysteresis_low(1); // -0.0149 Volts
		ADD_TO_REPO(sensor);
		if (!ipmc_sensors.get(sensor.sensor_number()))
			ipmc_sensors.add(std::make_shared<ThresholdSensor>(sensor.recordKey(), LOG["sensors"]["+3.3VMP"]));
	}

	{
		SensorDataRecord01 sensor;
		// SDR Calculator.html#precision=4&s-na=%2B1.0VETH&s-no=6&s-t=0x02&s-u-p=4&lnrf=0.9&lcrf=0.925&lncf=0.95&uncf=1.05&ucrf=1.075&unrf=1.1&nominalf=1&hystpi=1&hystni=1&minf=0&granularity=0.02
		sensor.initializeBlank("+1.0VETH");
		sensor.sensor_owner_id(0); // Tag as "self". This will be auto-calculated in "Get SDR" commands.
		sensor.sensor_owner_channel(0); // See above.
		sensor.sensor_owner_lun(0); // Generally zero
		sensor.sensor_number(6);
		sensor.entity_id(0xA0);
		sensor.entity_instance(0x60);
		//sensor.sensor_setable(false); // Default, Unsupported
		//sensor.initialize_scanning_enabled(false); // Default (An Init Agent is not required.)
		//sensor.initialize_events_enabled(false); // Default (An Init Agent is not required.)
		//sensor.initialize_thresholds(false); // Default (An Init Agent is not required.)
		//sensor.initialize_hysteresis(false); // Default (An Init Agent is not required.)
		//sensor.initialize_sensor_type(false); // Default (An Init Agent is not required.)
		sensor.ignore_if_entity_absent(true);
		sensor.events_enabled_default(true);
		sensor.scanning_enabled_default(true);
		sensor.sensor_auto_rearm(true);
		sensor.sensor_hysteresis_support(SensorDataRecordReadableSensor::ACCESS_READWRITE);
		sensor.sensor_threshold_access_support(SensorDataRecordReadableSensor::ACCESS_READWRITE);
		sensor.sensor_event_message_control_support(SensorDataRecordReadableSensor::EVTCTRL_GRANULAR);
		sensor.sensor_type_code(0x02); // Voltage
		sensor.event_type_reading_code(SensorDataRecordReadableSensor::EVENT_TYPE_THRESHOLD_SENSOR);
		sensor.threshold_comparisons_returned(0x3f); // All comparisons returned
		sensor.assertion_lower_threshold_reading_mask(0x0fff); // LNR, LCR, LNC, UNC, UCR, UNR assertions supported.
		sensor.deassertion_upper_threshold_reading_mask(0x0fff); // LNR, LCR, LNC, UNC, UCR, UNR deassertions supported.
		sensor.discrete_reading_setable_threshold_reading_mask(0x3f3f); // All thresholds are configurable.
		sensor.units_numeric_format(SensorDataRecord01::UNITS_UNSIGNED);
		sensor.units_rate_unit(SensorDataRecordReadableSensor::RATE_UNIT_NONE);
		sensor.units_base_unit(4); // Volts
		sensor.units_modifier_unit(0); // unspecified
		sensor.units_modifier_unit_method(SensorDataRecordReadableSensor::MODIFIER_UNIT_NONE);
		sensor.linearization(SensorDataRecord01::LIN_LINEAR);
		// IPMI Specifies a linearization function of: y = L[(Mx + (B * 10^(Bexp) ) ) * 10^(Rexp) ]
		// Our settings produce a valid range of 0 (Volts) to 5.1 (Volts) with 0.02 Volts granularity.
		sensor.conversion_m(2);
		sensor.conversion_b(0);
		sensor.conversion_b_exp(0);
		sensor.conversion_r_exp(-2);
		sensor.sensor_direction(SensorDataRecordReadableSensor::DIR_UNSPECIFIED);
		//sensor.normal_min_specified(false); // Default
		//sensor.normal_min_rawvalue(0); // Unspecified
		//sensor.normal_max_specified(false); // Default
		//sensor.normal_max_rawvalue(0); // Unspecified
		sensor.nominal_reading_specified(true);
		sensor.nominal_reading_rawvalue(50); // 1 Volts
		sensor.threshold_unr_rawvalue(55); // 1.1 Volts
		sensor.threshold_ucr_rawvalue(53); // 1.075 Volts
		sensor.threshold_unc_rawvalue(52); // 1.05 Volts
		sensor.threshold_lnc_rawvalue(47); // 0.95 Volts
		sensor.threshold_lcr_rawvalue(46); // 0.925 Volts
		sensor.threshold_lnr_rawvalue(45); // 0.9 Volts
		sensor.hysteresis_high(1); // +0.02 Volts
		sensor.hysteresis_low(1); // -0.02 Volts
		ADD_TO_REPO(sensor);
		if (!ipmc_sensors.get(sensor.sensor_number()))
			ipmc_sensors.add(std::make_shared<ThresholdSensor>(sensor.recordKey(), LOG["sensors"]["+1.0VETH"]));
	}

	{
		SensorDataRecord01 sensor;
		// SDR Calculator.html#precision=4&s-na=%2B2.5VETH&s-no=7&s-t=0x02&s-u-p=4&lnrf=2.25&lcrf=2.3125&lncf=2.375&uncf=2.625&ucrf=2.6875&unrf=2.75&nominalf=2.5&hystpi=1&hystni=1&minf=0&granularity=0.0113
		sensor.initializeBlank("+2.5VETH");
		sensor.sensor_owner_id(0); // Tag as "self". This will be auto-calculated in "Get SDR" commands.
		sensor.sensor_owner_channel(0); // See above.
		sensor.sensor_owner_lun(0); // Generally zero
		sensor.sensor_number(7);
		sensor.entity_id(0xA0);
		sensor.entity_instance(0x60);
		//sensor.sensor_setable(false); // Default, Unsupported
		//sensor.initialize_scanning_enabled(false); // Default (An Init Agent is not required.)
		//sensor.initialize_events_enabled(false); // Default (An Init Agent is not required.)
		//sensor.initialize_thresholds(false); // Default (An Init Agent is not required.)
		//sensor.initialize_hysteresis(false); // Default (An Init Agent is not required.)
		//sensor.initialize_sensor_type(false); // Default (An Init Agent is not required.)
		sensor.ignore_if_entity_absent(true);
		sensor.events_enabled_default(true);
		sensor.scanning_enabled_default(true);
		sensor.sensor_auto_rearm(true);
		sensor.sensor_hysteresis_support(SensorDataRecordReadableSensor::ACCESS_READWRITE);
		sensor.sensor_threshold_access_support(SensorDataRecordReadableSensor::ACCESS_READWRITE);
		sensor.sensor_event_message_control_support(SensorDataRecordReadableSensor::EVTCTRL_GRANULAR);
		sensor.sensor_type_code(0x02); // Voltage
		sensor.event_type_reading_code(SensorDataRecordReadableSensor::EVENT_TYPE_THRESHOLD_SENSOR);
		sensor.threshold_comparisons_returned(0x3f); // All comparisons returned
		sensor.assertion_lower_threshold_reading_mask(0x0fff); // LNR, LCR, LNC, UNC, UCR, UNR assertions supported.
		sensor.deassertion_upper_threshold_reading_mask(0x0fff); // LNR, LCR, LNC, UNC, UCR, UNR deassertions supported.
		sensor.discrete_reading_setable_threshold_reading_mask(0x3f3f); // All thresholds are configurable.
		sensor.units_numeric_format(SensorDataRecord01::UNITS_UNSIGNED);
		sensor.units_rate_unit(SensorDataRecordReadableSensor::RATE_UNIT_NONE);
		sensor.units_base_unit(4); // Volts
		sensor.units_modifier_unit(0); // unspecified
		sensor.units_modifier_unit_method(SensorDataRecordReadableSensor::MODIFIER_UNIT_NONE);
		sensor.linearization(SensorDataRecord01::LIN_LINEAR);
		// IPMI Specifies a linearization function of: y = L[(Mx + (B * 10^(Bexp) ) ) * 10^(Rexp) ]
		// Our settings produce a valid range of 0 (Volts) to 2.8815 (Volts) with 0.0113 Volts granularity.
		sensor.conversion_m(113);
		sensor.conversion_b(0);
		sensor.conversion_b_exp(0);
		sensor.conversion_r_exp(-4);
		sensor.sensor_direction(SensorDataRecordReadableSensor::DIR_UNSPECIFIED);
		//sensor.normal_min_specified(false); // Default
		//sensor.normal_min_rawvalue(0); // Unspecified
		//sensor.normal_max_specified(false); // Default
		//sensor.normal_max_rawvalue(0); // Unspecified
		sensor.nominal_reading_specified(true);
		sensor.nominal_reading_rawvalue(221); // 2.5 Volts
		sensor.threshold_unr_rawvalue(243); // 2.75 Volts
		sensor.threshold_ucr_rawvalue(237); // 2.6875 Volts
		sensor.threshold_unc_rawvalue(232); // 2.625 Volts
		sensor.threshold_lnc_rawvalue(210); // 2.375 Volts
		sensor.threshold_lcr_rawvalue(204); // 2.3125 Volts
		sensor.threshold_lnr_rawvalue(199); // 2.25 Volts
		sensor.hysteresis_high(1); // +0.0113 Volts
		sensor.hysteresis_low(1); // -0.0113 Volts
		ADD_TO_REPO(sensor);
		if (!ipmc_sensors.get(sensor.sensor_number()))
			ipmc_sensors.add(std::make_shared<ThresholdSensor>(sensor.recordKey(), LOG["sensors"]["+2.5VETH"]));
	}

	{
		SensorDataRecord01 sensor;
		// SDR Calculator.html#precision=4&s-na=%2B1.2VPHY&s-no=8&s-t=0x02&s-u-p=4&lnrf=1.08&lcrf=1.11&lncf=1.14&uncf=1.26&ucrf=1.29&unrf=1.32&nominalf=1.2&hystpi=1&hystni=1&minf=0&granularity=0.0055
		sensor.initializeBlank("+1.2VPHY");
		sensor.sensor_owner_id(0); // Tag as "self". This will be auto-calculated in "Get SDR" commands.
		sensor.sensor_owner_channel(0); // See above.
		sensor.sensor_owner_lun(0); // Generally zero
		sensor.sensor_number(8);
		sensor.entity_id(0xA0);
		sensor.entity_instance(0x60);
		//sensor.sensor_setable(false); // Default, Unsupported
		//sensor.initialize_scanning_enabled(false); // Default (An Init Agent is not required.)
		//sensor.initialize_events_enabled(false); // Default (An Init Agent is not required.)
		//sensor.initialize_thresholds(false); // Default (An Init Agent is not required.)
		//sensor.initialize_hysteresis(false); // Default (An Init Agent is not required.)
		//sensor.initialize_sensor_type(false); // Default (An Init Agent is not required.)
		sensor.ignore_if_entity_absent(true);
		sensor.events_enabled_default(true);
		sensor.scanning_enabled_default(true);
		sensor.sensor_auto_rearm(true);
		sensor.sensor_hysteresis_support(SensorDataRecordReadableSensor::ACCESS_READWRITE);
		sensor.sensor_threshold_access_support(SensorDataRecordReadableSensor::ACCESS_READWRITE);
		sensor.sensor_event_message_control_support(SensorDataRecordReadableSensor::EVTCTRL_GRANULAR);
		sensor.sensor_type_code(0x02); // Voltage
		sensor.event_type_reading_code(SensorDataRecordReadableSensor::EVENT_TYPE_THRESHOLD_SENSOR);
		sensor.threshold_comparisons_returned(0x3f); // All comparisons returned
		sensor.assertion_lower_threshold_reading_mask(0x0fff); // LNR, LCR, LNC, UNC, UCR, UNR assertions supported.
		sensor.deassertion_upper_threshold_reading_mask(0x0fff); // LNR, LCR, LNC, UNC, UCR, UNR deassertions supported.
		sensor.discrete_reading_setable_threshold_reading_mask(0x3f3f); // All thresholds are configurable.
		sensor.units_numeric_format(SensorDataRecord01::UNITS_UNSIGNED);
		sensor.units_rate_unit(SensorDataRecordReadableSensor::RATE_UNIT_NONE);
		sensor.units_base_unit(4); // Volts
		sensor.units_modifier_unit(0); // unspecified
		sensor.units_modifier_unit_method(SensorDataRecordReadableSensor::MODIFIER_UNIT_NONE);
		sensor.linearization(SensorDataRecord01::LIN_LINEAR);
		// IPMI Specifies a linearization function of: y = L[(Mx + (B * 10^(Bexp) ) ) * 10^(Rexp) ]
		// Our settings produce a valid range of 0 (Volts) to 1.4025 (Volts) with 0.0055 Volts granularity.
		sensor.conversion_m(55);
		sensor.conversion_b(0);
		sensor.conversion_b_exp(0);
		sensor.conversion_r_exp(-4);
		sensor.sensor_direction(SensorDataRecordReadableSensor::DIR_UNSPECIFIED);
		//sensor.normal_min_specified(false); // Default
		//sensor.normal_min_rawvalue(0); // Unspecified
		//sensor.normal_max_specified(false); // Default
		//sensor.normal_max_rawvalue(0); // Unspecified
		sensor.nominal_reading_specified(true);
		sensor.nominal_reading_rawvalue(218); // 1.2 Volts
		sensor.threshold_unr_rawvalue(240); // 1.32 Volts
		sensor.threshold_ucr_rawvalue(234); // 1.29 Volts
		sensor.threshold_unc_rawvalue(229); // 1.26 Volts
		sensor.threshold_lnc_rawvalue(207); // 1.14 Volts
		sensor.threshold_lcr_rawvalue(201); // 1.11 Volts
		sensor.threshold_lnr_rawvalue(196); // 1.08 Volts
		sensor.hysteresis_high(1); // +0.0055 Volts
		sensor.hysteresis_low(1); // -0.0055 Volts
		ADD_TO_REPO(sensor);
		if (!ipmc_sensors.get(sensor.sensor_number()))
			ipmc_sensors.add(std::make_shared<ThresholdSensor>(sensor.recordKey(), LOG["sensors"]["+1.2VPHY"]));
	}

	{
		SensorDataRecord01 sensor;
		// SDR Calculator.html#precision=4&s-na=T_TOP&s-no=9&s-t=0x01&s-u-p=1&uncf=40&ucrf=45&unrf=50&nominalf=30&hystpi=1&hystni=1&minf=0&granularity=0.5
		sensor.initializeBlank("T_TOP");
		sensor.sensor_owner_id(0); // Tag as "self". This will be auto-calculated in "Get SDR" commands.
		sensor.sensor_owner_channel(0); // See above.
		sensor.sensor_owner_lun(0); // Generally zero
		sensor.sensor_number(9);
		sensor.entity_id(0xA0);
		sensor.entity_instance(0x60);
		//sensor.sensor_setable(false); // Default, Unsupported
		//sensor.initialize_scanning_enabled(false); // Default (An Init Agent is not required.)
		//sensor.initialize_events_enabled(false); // Default (An Init Agent is not required.)
		//sensor.initialize_thresholds(false); // Default (An Init Agent is not required.)
		//sensor.initialize_hysteresis(false); // Default (An Init Agent is not required.)
		//sensor.initialize_sensor_type(false); // Default (An Init Agent is not required.)
		sensor.ignore_if_entity_absent(true);
		sensor.events_enabled_default(true);
		sensor.scanning_enabled_default(true);
		sensor.sensor_auto_rearm(true);
		sensor.sensor_hysteresis_support(SensorDataRecordReadableSensor::ACCESS_READWRITE);
		sensor.sensor_threshold_access_support(SensorDataRecordReadableSensor::ACCESS_READWRITE);
		sensor.sensor_event_message_control_support(SensorDataRecordReadableSensor::EVTCTRL_GRANULAR);
		sensor.sensor_type_code(0x01); // Temperature
		sensor.event_type_reading_code(SensorDataRecordReadableSensor::EVENT_TYPE_THRESHOLD_SENSOR);
		sensor.threshold_comparisons_returned(0x3f); // All comparisons returned
		sensor.assertion_lower_threshold_reading_mask(0x0fc0); // UNC, UCR, UNR assertions supported.
		sensor.deassertion_upper_threshold_reading_mask(0x0fc0); // UNC, UCR, UNR deassertions supported.
		sensor.discrete_reading_setable_threshold_reading_mask(0x3f3f); // All thresholds are configurable.
		sensor.units_numeric_format(SensorDataRecord01::UNITS_UNSIGNED);
		sensor.units_rate_unit(SensorDataRecordReadableSensor::RATE_UNIT_NONE);
		sensor.units_base_unit(1); // degrees C
		sensor.units_modifier_unit(0); // unspecified
		sensor.units_modifier_unit_method(SensorDataRecordReadableSensor::MODIFIER_UNIT_NONE);
		sensor.linearization(SensorDataRecord01::LIN_LINEAR);
		// IPMI Specifies a linearization function of: y = L[(Mx + (B * 10^(Bexp) ) ) * 10^(Rexp) ]
		// Our settings produce a valid range of 0 (degrees C) to 127.5 (degrees C) with 0.5 degrees C granularity.
		sensor.conversion_m(5);
		sensor.conversion_b(0);
		sensor.conversion_b_exp(0);
		sensor.conversion_r_exp(-1);
		sensor.sensor_direction(SensorDataRecordReadableSensor::DIR_UNSPECIFIED);
		//sensor.normal_min_specified(false); // Default
		//sensor.normal_min_rawvalue(0); // Unspecified
		//sensor.normal_max_specified(false); // Default
		//sensor.normal_max_rawvalue(0); // Unspecified
		sensor.nominal_reading_specified(true);
		sensor.nominal_reading_rawvalue(60); // 30 degrees C
		sensor.threshold_unr_rawvalue(100); // 50 degrees C
		sensor.threshold_ucr_rawvalue(90); // 45 degrees C
		sensor.threshold_unc_rawvalue(80); // 40 degrees C
		sensor.threshold_lnc_rawvalue(0); // null degrees C
		sensor.threshold_lcr_rawvalue(0); // null degrees C
		sensor.threshold_lnr_rawvalue(0); // null degrees C
		sensor.hysteresis_high(1); // +0.5 degrees C
		sensor.hysteresis_low(1); // -0.5 degrees C
		ADD_TO_REPO(sensor);
		if (!ipmc_sensors.get(sensor.sensor_number()))
			ipmc_sensors.add(std::make_shared<ThresholdSensor>(sensor.recordKey(), LOG["sensors"]["T_TOP"]));
	}

	{
		SensorDataRecord01 sensor;
		// SDR Calculator.html#precision=4&s-na=T_BOT&s-no=10&s-t=0x01&s-u-p=1&uncf=40&ucrf=45&unrf=50&nominalf=30&hystpi=1&hystni=1&minf=0&granularity=0.5
		sensor.initializeBlank("T_BOT");
		sensor.sensor_owner_id(0); // Tag as "self". This will be auto-calculated in "Get SDR" commands.
		sensor.sensor_owner_channel(0); // See above.
		sensor.sensor_owner_lun(0); // Generally zero
		sensor.sensor_number(10);
		sensor.entity_id(0xA0);
		sensor.entity_instance(0x60);
		//sensor.sensor_setable(false); // Default, Unsupported
		//sensor.initialize_scanning_enabled(false); // Default (An Init Agent is not required.)
		//sensor.initialize_events_enabled(false); // Default (An Init Agent is not required.)
		//sensor.initialize_thresholds(false); // Default (An Init Agent is not required.)
		//sensor.initialize_hysteresis(false); // Default (An Init Agent is not required.)
		//sensor.initialize_sensor_type(false); // Default (An Init Agent is not required.)
		sensor.ignore_if_entity_absent(true);
		sensor.events_enabled_default(true);
		sensor.scanning_enabled_default(true);
		sensor.sensor_auto_rearm(true);
		sensor.sensor_hysteresis_support(SensorDataRecordReadableSensor::ACCESS_READWRITE);
		sensor.sensor_threshold_access_support(SensorDataRecordReadableSensor::ACCESS_READWRITE);
		sensor.sensor_event_message_control_support(SensorDataRecordReadableSensor::EVTCTRL_GRANULAR);
		sensor.sensor_type_code(0x01); // Temperature
		sensor.event_type_reading_code(SensorDataRecordReadableSensor::EVENT_TYPE_THRESHOLD_SENSOR);
		sensor.threshold_comparisons_returned(0x3f); // All comparisons returned
		sensor.assertion_lower_threshold_reading_mask(0x0fc0); // UNC, UCR, UNR assertions supported.
		sensor.deassertion_upper_threshold_reading_mask(0x0fc0); // UNC, UCR, UNR deassertions supported.
		sensor.discrete_reading_setable_threshold_reading_mask(0x3f3f); // All thresholds are configurable.
		sensor.units_numeric_format(SensorDataRecord01::UNITS_UNSIGNED);
		sensor.units_rate_unit(SensorDataRecordReadableSensor::RATE_UNIT_NONE);
		sensor.units_base_unit(1); // degrees C
		sensor.units_modifier_unit(0); // unspecified
		sensor.units_modifier_unit_method(SensorDataRecordReadableSensor::MODIFIER_UNIT_NONE);
		sensor.linearization(SensorDataRecord01::LIN_LINEAR);
		// IPMI Specifies a linearization function of: y = L[(Mx + (B * 10^(Bexp) ) ) * 10^(Rexp) ]
		// Our settings produce a valid range of 0 (degrees C) to 127.5 (degrees C) with 0.5 degrees C granularity.
		sensor.conversion_m(5);
		sensor.conversion_b(0);
		sensor.conversion_b_exp(0);
		sensor.conversion_r_exp(-1);
		sensor.sensor_direction(SensorDataRecordReadableSensor::DIR_UNSPECIFIED);
		//sensor.normal_min_specified(false); // Default
		//sensor.normal_min_rawvalue(0); // Unspecified
		//sensor.normal_max_specified(false); // Default
		//sensor.normal_max_rawvalue(0); // Unspecified
		sensor.nominal_reading_specified(true);
		sensor.nominal_reading_rawvalue(60); // 30 degrees C
		sensor.threshold_unr_rawvalue(100); // 50 degrees C
		sensor.threshold_ucr_rawvalue(90); // 45 degrees C
		sensor.threshold_unc_rawvalue(80); // 40 degrees C
		sensor.threshold_lnc_rawvalue(0); // null degrees C
		sensor.threshold_lcr_rawvalue(0); // null degrees C
		sensor.threshold_lnr_rawvalue(0); // null degrees C
		sensor.hysteresis_high(1); // +0.5 degrees C
		sensor.hysteresis_low(1); // -0.5 degrees C
		ADD_TO_REPO(sensor);
		if (!ipmc_sensors.get(sensor.sensor_number()))
			ipmc_sensors.add(std::make_shared<ThresholdSensor>(sensor.recordKey(), LOG["sensors"]["T_BOT"]));
	}

#undef ADD_TO_REPO

	runTask("persist_sdr", TASK_PRIORITY_SERVICE, [reinit]() -> void {
		VariablePersistentAllocation sdr_persist(*persistent_storage, PersistentStorageAllocations::WISC_SDR_REPOSITORY);
		// If not reinitializing, merge in saved configuration, overwriting matching records.
		if (!reinit) {
			device_sdr_repo.u8import(sdr_persist.getData());
			// Now that we've merged in our stored settings, we'll have to update the linkages (and sensor processor settings) again.
			if (payload_manager)
				payload_manager->refreshSensorLinkage();
			// else: It'll get run after payload_manager is initialized anyway.
		}

		// Store the newly initialized Device SDRs
		sdr_persist.setData(device_sdr_repo.u8export());
	});
}


