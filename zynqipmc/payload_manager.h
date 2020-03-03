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

#ifndef SRC_COMMON_ZYNQIPMC_PAYLOAD_MANAGER_H_
#define SRC_COMMON_ZYNQIPMC_PAYLOAD_MANAGER_H_

#include <drivers/ad7689/ad7689.h>
#include <drivers/generics/adc.h>
#include <drivers/management_zone/management_zone.h>
#include <drivers/sensorprocessor/sensorprocessor.h>
#include <services/console/consolesvc.h>
#include <services/faultlog/faultlog.h>
#include <services/ipmi/m_state_machine.h>
#include <services/ipmi/sensor/severity_sensor.h>
#include <services/ipmi/sensor/threshold_sensor.h>
#include <services/timer/timer.h>
#include <vector>
#include <map>
#include <memory>

class PowerProperties {
public:
	/// The number of ATCA slots spanned by this board (PICMG 3.0 Table 3-82)
	uint8_t spanned_slots;
	/// The slot on the board 0=left where the IPM connector is (PICMG 3.0 Table 3-82)
	uint8_t controller_location;

	/// True if payload service is uninterrupted when reconfiguring power levels.
	bool dynamic_reconfiguration;

	/**
	 * The current or desired power levels, as a 1-based index into the
	 * power_levels array.  The value 0 indicates no payload power.
	 */
	///@{
	uint8_t current_power_level;
	uint8_t desired_power_level;
	///@}

	/// The delay (tenths of a second) for which the specified early power levels are desired.
	///@{
	uint8_t delay_to_stable_power;
	uint8_t remaining_delay_to_stable_power;
	///@}

	/**
	 * A multiplier for the values in the power levels array.
	 * (If 5, then power levels are specified in units of 5W.)
	 */
	uint8_t power_multiplier;

	/**
	 * An array of up to 20 selectable power levels, in units of
	 * `power_multipler` Watts.  This array must be monotonically increasing.
	 *
	 * These levels represent modes of operation.  The shelf manager will
	 * attempt to authorize power up to the requested mode of operation, but
	 * may select a lower power mode of operation if that is not possible.
	 *
	 * These values should NOT include the first 10W of management power.
	 */
	///@{
	std::vector<uint8_t> power_levels;
	std::vector<uint8_t> early_power_levels;
	///@}

	PowerProperties() :
		spanned_slots(1), controller_location(0),
		dynamic_reconfiguration(false),
		current_power_level(0),
		desired_power_level(0),
		delay_to_stable_power(0),
		remaining_delay_to_stable_power(0),
		power_multiplier(1) { };
};

/**
 * A link descriptor, following the structure of PICMG 3.0 Table 3-50.
 */
class LinkDescriptor {
public:
	bool enabled; ///< Indicates the enabled status of this link.

	uint8_t LinkGroupingID;
	uint8_t LinkTypeExtension;
	uint8_t LinkType;
	/// Bit 0 = Port 0, etc.
	uint8_t IncludedPorts;
	enum Interfaces {
		IF_BASE           = 0,
		IF_FABRIC         = 1,
		IF_UPDATE_CHANNEL = 2,
		IF_RESERVED       = 3
	};
	enum Interfaces Interface;
	uint8_t ChannelNumber;

	LinkDescriptor();
	LinkDescriptor(
			uint8_t LinkGroupingID,
			uint8_t LinkTypeExtension,
			uint8_t LinkType,
			uint8_t IncludedPorts,
			enum Interfaces Interface,
			uint8_t ChannelNumber);
	LinkDescriptor(const std::vector<uint8_t> &bytes, bool enabled=false);
	operator std::vector<uint8_t>() const;

	/// Determines whether these link descriptors are the same link
	bool operator==(const LinkDescriptor &b) const {
		return std::vector<uint8_t>(*this) == std::vector<uint8_t>(b);
	}
	/// Determines whether these link descriptors are not the same link
	bool operator!=(const LinkDescriptor &b) const {
		return std::vector<uint8_t>(*this) != std::vector<uint8_t>(b);
	}

	/**
	 * Register or look up an OEM LinkType GUID, and return the LinkType index
	 * associated with it.
	 *
	 * @param oem_guid The GUID (16 bytes) to register.
	 * @return The LinkType index associated with this GUID.
	 * @throws std::domain_error if an invalid GUID is supplied
	 * @throws std::out_of_range if there is no more space in the table
	 */
	static uint8_t map_oem_LinkType_guid(const std::vector<uint8_t> &oem_guid);

	/**
	 * Looks up an OEM LinkType index and converts it to the appropriate OEM GUID.
	 * @param LinkType The LinkType index
	 * @return The OEM GUID
	 * @throws std::out_of_range if the LinkType is not registered.
	 */
	static std::vector<uint8_t> lookup_oem_LinkType_guid(uint8_t LinkType);

protected:
	static SemaphoreHandle_t oem_guid_mutex; ///< A mutex protecting the registered OEM GUID mapping.
	static std::map< uint8_t, std::vector<uint8_t> > oem_guids; ///< A mapping of registered OEM GUIDs.
};

class PayloadManager : public ConsoleCommandSupport {
public:
	/**
	 * Instantiate the PayloadManager and perform all required initialization.
	 *
	 * @param mstate_machine The MStateMachine to register
	 * @param faultlog The fault log (or NULL) to record major events to.
	 * @param log The LogTree to use
	 */
	PayloadManager(MStateMachine *mstate_machine, FaultLog *faultlog, LogTree &log);
	virtual ~PayloadManager();

	virtual void config() = 0;

	virtual PowerProperties getPowerProperties(uint8_t fru, bool recompute = false) = 0;
	virtual void setPowerLevel(uint8_t fru, uint8_t level) = 0;

	void updateLinkEnable(const LinkDescriptor &descriptor);
	std::vector<LinkDescriptor> getLinks() const;

	virtual std::string getMZName(size_t mz) const;

	// From base class ConsoleCommandSupport:
	virtual void registerConsoleCommands(CommandParser &parser, const std::string &prefix);
	virtual void deregisterConsoleCommands(CommandParser &parser, const std::string &prefix);

	class ADCSensor {
	public:
		std::string name;
		ADC::Channel adc;
		int sensor_processor_id;
		int mz_context;
		std::weak_ptr<ThresholdSensor> ipmi_sensor;
		ADCSensor(std::string name, ADC::Channel adc, int sensor_processor_id, int mz_context) :
			name(name), adc(adc), sensor_processor_id(sensor_processor_id), mz_context(mz_context) { };
	};

protected:
	// Console commands:
	class PowerLevelCommand;
	class MzControlCommand;
	class ReadIPMISensorsCommand;
	class GetSensorEventEnablesCommand;
	class SetSensorEventEnablesCommand;

	SemaphoreHandle_t mutex; ///< A mutex protecting internal data.
	MStateMachine *mstate_machine; ///< The MStateMachine to notify of changes.
	FaultLog *faultlog; ///< The fault log to record major events in.
#ifdef MANAGEMENT_ZONE_PRESENT_IN_BSP
	uint64_t mz_hf_vectors[XPAR_MGMT_ZONE_CTRL_0_MZ_CNT];
	ZoneController::Zone *mgmt_zones[XPAR_MGMT_ZONE_CTRL_0_MZ_CNT];
#endif
#ifdef SENSORPROCESSOR_PRESENT_IN_BSP
	SensorProcessor *sensor_processor; ///< The sensor processor instance to configure and use.
#endif
	PowerProperties power_properties; ///< The current power properties.
	LogTree &log; ///< The LogTree for this object's messages.
	LogRepeatSuppressor logrepeat; ///< A LogRepeatSuppressor for this instance.
	std::vector<LinkDescriptor> links; ///< All supported E-Keying links.

	void finishConfig();

	virtual void implementPowerLevel(uint8_t level) = 0;
	virtual void processNonManagedADCSensor(std::pair<std::string, ADCSensor> sensor);

	static std::map<std::string, ADCSensor> adc_sensors;
	std::shared_ptr<SeveritySensor> alarmlevel_sensor;
	void runSensorThread();

	/**
	 * Returns true if the specified MZ is in context, or false otherwise.
	 * @param mz The MZ to check
	 * @return true if the MZ is in context or the supplied MZ id is <0, else false
	 */
	virtual bool isMZInContext(int mz) const;
	void updateSensorProcessorContexts();
	std::shared_ptr<TimerService::Timer> sp_context_update_timer; ///< A timer used to re-enable sensor processor contexts.

	/**
	 * Allow user configuration of which IPMI events are fault-loged.
	 * @param ipmi_payload The payload of the IPMI Platform Event message.
	 * @return true if this message should be logged to EEPROM, else false
	 */
	virtual bool filterFaultLogIPMIEvent(const std::vector<uint8_t> &ipmi_payload);

	/**
	 * Allow users to include handlers for MZ fault events.
	 * @note This fault detection is slow-polling after the firmware response.
	 * @note The default handler writes to the EEPROM fault log.
	 */
	virtual void mzFaultDetected(size_t mzno);

public:
	/**
	 * This refreshes the ADC Sensor to IPMI Sensor linkage by doing a name-based
	 * lookup for each ADC Sensor in the global ipmc_sensors SensorSet.
	 */
	void refreshSensorLinkage();

	static void addADCSensor(ADCSensor sensor) {
		PayloadManager::adc_sensors.insert(std::pair<std::string, ADCSensor>(sensor.name, sensor));
	}

	static inline const std::map<std::string, ADCSensor>& getADCSensors() { return PayloadManager::adc_sensors; };
};

#endif /* SRC_COMMON_ZYNQIPMC_PAYLOAD_MANAGER_H_ */
