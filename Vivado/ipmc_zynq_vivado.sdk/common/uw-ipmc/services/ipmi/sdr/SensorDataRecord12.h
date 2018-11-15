#ifndef SRC_COMMON_UW_IPMC_SERVICES_IPMI_SDR_SENSORDATARECORD12_H_
#define SRC_COMMON_UW_IPMC_SERVICES_IPMI_SDR_SENSORDATARECORD12_H_

#include <services/ipmi/sdr/SensorDataRecord.h>
#include <map>
#include <string>

/**
 * A generic intermediary class providing the common interface between type 01,
 * type 02, type 03 SDRs.
 */
class SensorDataRecord12 : public SensorDataRecord {
public:
	SensorDataRecord12(const std::vector<uint8_t> &sdr_data = std::vector<uint8_t>()) : SensorDataRecord(sdr_data) { };
	virtual ~SensorDataRecord12() { };

	virtual std::vector<uint8_t> record_key() const;
	virtual void validate() const;
	virtual uint8_t parsed_record_type() const { return 0x12; };
	virtual void initialize_blank(std::string name);

	/**
	 * SDR Data Accessors
	 *
	 * \warning Do not call any accessors on a record that does not validate().
	 */
	///@{
#define SDR_FIELD(name, type, byte, a, b, attributes) \
	virtual type name() const attributes; \
	virtual void name(type val) attributes;

	SDR_FIELD(device_slave_address, uint8_t, 5, 7, 0, )
	SDR_FIELD(channel, uint8_t, 6, 3, 0, )

	SDR_FIELD(acpi_system_power_state_notification_required, bool, 7, 7, 7, )
	SDR_FIELD(acpi_device_power_state_notification_required, bool, 7, 6, 6, )
	SDR_FIELD(is_static, bool, 7, 5, 5, )
	SDR_FIELD(init_agent_logs_errors, bool, 7, 3, 3, )
	SDR_FIELD(init_agent_log_errors_accessing_this_controller, bool, 7, 2, 2, )
	enum InitializationType {
		INIT_ENABLE_EVENTS  = 0,
		INIT_DISABLE_EVENTS = 1,
		INIT_DO_NOT_INIT    = 2,
		INIT_RESERVED       = 3,
	};
	SDR_FIELD(init_agent_init_type, enum InitializationType, 7, 1, 0, )

	SDR_FIELD(cap_chassis_device, bool, 8, 7, 7, )
	SDR_FIELD(cap_bridge, bool, 8, 6, 6, )
	SDR_FIELD(cap_ipmb_event_generator, bool, 8, 5, 5, )
	SDR_FIELD(cap_ipmb_event_receiver, bool, 8, 4, 4, )
	SDR_FIELD(cap_fru_inventory_device, bool, 8, 3, 3, )
	SDR_FIELD(cap_sel_device, bool, 8, 2, 2, )
	SDR_FIELD(cap_sdr_repository_device, bool, 8, 1, 1, )
	SDR_FIELD(cap_sensor_device, bool, 8, 0, 0, )

	SDR_FIELD(entity_id, uint8_t, 12, 7, 0, )
	SDR_FIELD(entity_instance, uint8_t, 13, 7, 0, )

	SDR_FIELD(oem, uint8_t, 14, 7, 0, )

	SDR_FIELD(id_string, std::string, VARIABLE, 7, 0, )

#undef SDR_FIELD
	///@}
};

#endif /* SRC_COMMON_UW_IPMC_SERVICES_IPMI_SDR_SENSORDATARECORDSENSOR_H_ */
