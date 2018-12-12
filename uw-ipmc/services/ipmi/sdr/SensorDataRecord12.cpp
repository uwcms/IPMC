#include <IPMC.h>
#include <services/ipmi/IPMIFormats.h>
#include <services/ipmi/sdr/SensorDataRecord12.h>
#include <iterator>

std::vector<uint8_t> SensorDataRecord12::record_key() const {
	this->validate();
	return std::vector<uint8_t>(std::next(this->sdr_data.begin(), 5), std::next(this->sdr_data.begin(), 8));
}

void SensorDataRecord12::validate() const {
	SensorDataRecord::validate();
	if (this->record_type() != 0x12)
		throw invalid_sdr_error("SensorDataRecord12 supports only type 12h SDRs.");
	// Check that the ID field's Type/Length specification is valid.
	if (this->sdr_data.size() < 16 +1U /* One byte of ID content */ +1U /* Change offset of last, to size of container */)
		throw invalid_sdr_error("Truncated SDR");
	unsigned idlen = ipmi_type_length_field_get_length(std::vector<uint8_t>(std::next(this->sdr_data.begin(), 16), this->sdr_data.end()));
	if (idlen > 17) // IDString TypeLengthCode (= 1) + IDString Bytes (<= 16)
		throw invalid_sdr_error("Invalid ID string");
	if (16+idlen < this->sdr_data.size())
		throw invalid_sdr_error("Truncated ID string");
}

void SensorDataRecord12::initialize_blank(std::string name) {
	if (name.size() > 16) // Specification limit
		throw std::domain_error("Sensor names must be <= 16 characters.");
	this->sdr_data.resize(0); // Clear
	this->sdr_data.resize(16, 0); // Initialize Blank Fields
	this->sdr_data.push_back(0xC0 | name.size()); // Type/Length code: Raw ASCII/Unicode, with length.
	for (auto it = name.begin(), eit = name.end(); it != eit; ++it)
		this->sdr_data.push_back(static_cast<uint8_t>(*it));
	this->sdr_data[2] = 0x51; // Set SDR version
	this->sdr_data[3] = this->parsed_record_type(); // Set record type
	this->sdr_data[4] = this->sdr_data.size() - 5; // Number of remaining bytes.
}

/// Create a bitmask with the lower `nbits` bits set.
#define LOWBITS(nbits) (0xff >> (8-(nbits)))

/// Define a `type` type SDR_FIELD from byte `byte`[a:b].
#define SDR_FIELD(name, type, byte, a, b, attributes) \
	type SensorDataRecord12::name() const attributes { \
		this->validate(); \
		return static_cast<type>((this->sdr_data[byte] >> (b)) & LOWBITS((a)-(b)+1)); \
	} \
	void SensorDataRecord12::name(type val) attributes { \
		if ((static_cast<uint8_t>(val) & LOWBITS((a)-(b)+1)) != static_cast<uint8_t>(val)) \
			throw std::domain_error("The supplied value does not fit correctly in the field."); \
		this->validate(); \
		this->sdr_data[byte] &= ~(LOWBITS((a)-(b)+1)<<(b)); /* Erase old value */ \
		this->sdr_data[byte] |= static_cast<uint8_t>(val)<<(b); /* Set new value */ \
	}

SDR_FIELD(device_slave_address, uint8_t, 5, 7, 0, )
SDR_FIELD(channel, uint8_t, 6, 3, 0, )

SDR_FIELD(acpi_system_power_state_notification_required, bool, 7, 7, 7, )
SDR_FIELD(acpi_device_power_state_notification_required, bool, 7, 6, 6, )
SDR_FIELD(is_static, bool, 7, 5, 5, )
SDR_FIELD(init_agent_logs_errors, bool, 7, 3, 3, )
SDR_FIELD(init_agent_log_errors_accessing_this_controller, bool, 7, 2, 2, )
SDR_FIELD(init_agent_init_type, enum SensorDataRecord12::InitializationType, 7, 1, 0, )

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

std::string SensorDataRecord12::id_string() const {
	this->validate();
	return render_ipmi_type_length_field(std::vector<uint8_t>(std::next(this->sdr_data.begin(), 16), this->sdr_data.end()));
}
void SensorDataRecord12::id_string(std::string val) {
	this->validate();
	if (val.size() > 16) // Specified.
		throw std::domain_error("Sensor names must be <= 16 characters.");
	this->sdr_data.resize(16); // Downsize.
	this->sdr_data.push_back(0xC0 | val.size()); // Type/Length code: Raw ASCII/Unicode, with length.
	for (auto it = val.begin(), eit = val.end(); it != eit; ++it)
		this->sdr_data.push_back(static_cast<uint8_t>(*it));
}

std::vector<uint8_t> SensorDataRecord12::u8export(uint8_t self_ipmb_addr, uint8_t self_ipmb_channel) const {
	std::vector<uint8_t> out = SensorDataRecord::u8export(self_ipmb_addr, self_ipmb_channel);
	if (out[5] == 0)
		out[5] = self_ipmb_addr;
	if ((out[6] & 0x0f) == 0)
		out[6] |= self_ipmb_channel;
	return out;
}
