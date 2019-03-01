/*
 * PayloadManager.h
 *
 *  Created on: Dec 4, 2018
 *      Author: jtikalsky
 */

#ifndef SRC_COMMON_UW_IPMC_PAYLOADMANAGER_H_
#define SRC_COMMON_UW_IPMC_PAYLOADMANAGER_H_

#include <services/ipmi/MStateMachine.h>
#include <services/console/ConsoleSvc.h>
#include <drivers/mgmt_zone/MGMTZone.h>
#include <vector>

class PayloadManager final {
public:
	PayloadManager(MStateMachine *mstate_machine, LogTree &log);
	virtual ~PayloadManager();

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
	PowerProperties get_power_properties(uint8_t fru, bool recompute=false);
	void set_power_level(uint8_t fru, uint8_t level);

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
		static uint8_t map_oem_LinkType_guid(const std::vector<uint8_t> &oem_guid);
		static std::vector<uint8_t> lookup_oem_LinkType_guid(uint8_t LinkType);

	protected:
		static SemaphoreHandle_t oem_guid_mutex; ///< A mutex protecting the registered OEM GUID mapping.
		static std::map< uint8_t, std::vector<uint8_t> > oem_guids; ///< A mapping of registered OEM GUIDs.
	};
	void update_link_enable(const LinkDescriptor &descriptor);
	std::vector<LinkDescriptor> get_links() const;

	void register_console_commands(CommandParser &parser, const std::string &prefix);
	void deregister_console_commands(CommandParser &parser, const std::string &prefix);

protected:
	SemaphoreHandle_t mutex; ///< A mutex protecting internal data.
	MStateMachine *mstate_machine; ///< The MStateMachine to notify of changes.
	MGMT_Zone *mgmt_zones[XPAR_MGMT_ZONE_CTRL_0_MZ_CNT];
	PowerProperties power_properties; ///< The current power properties.
	LogTree &log; ///< The LogTree for this object's messages.
	std::vector<LinkDescriptor> links; ///< All supported E-Keying links.

	void implement_power_level(uint8_t level);

	friend class ConsoleCommand_PayloadManager_power_level;
	friend class ConsoleCommand_PayloadManager_mz_control;
	friend void payload_manager_apd_bringup_poweroff_hack(); // XXX
};

#endif /* SRC_COMMON_UW_IPMC_PAYLOADMANAGER_H_ */
