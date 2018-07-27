#ifndef SRC_COMMON_UW_IPMC_SERVICES_IPMI_REMOTEFRUSTORAGE_H_
#define SRC_COMMON_UW_IPMC_SERVICES_IPMI_REMOTEFRUSTORAGE_H_

#include <stddef.h>
#include <services/ipmi/ipmbsvc/IPMBSvc.h>
#include <memory>
#include <vector>

/**
 * A class representing a remote FRU Storage Area, with methods for accessing it.
 */
class RemoteFRUStorage {
public:
	IPMBSvc *ipmb;                       ///< The IPMBSvc used to communicate with the FRU Storage Area
	uint8_t ipmb_target;                 ///< The IPMI address of the device controlling the FRU Storage Area
	uint8_t fru_device_id;               ///< The ID of the FRU Storage Area

	uint16_t size;                       ///< The size in bytes of the FRU Storage Area
	bool byte_addressed;                 ///< True if the storage area is byte addressed, else false if it is word addressed

	uint8_t header_version;              ///< The version of the Storage Area header, if it has been loaded
	uint16_t internal_use_area_offset;   ///< The Internal Use Area Offset, if the header has been loaded
	uint16_t chassis_info_area_offset;   ///< The Chassis Info Area Offset, if the header has been loaded
	uint16_t board_area_offset;          ///< The Board Area Offset, if the header has been loaded
	uint16_t product_info_area_offset;   ///< The Product Info Area Offset, if the header has been loaded
	uint16_t multirecord_area_offset;    ///< The MultiRecord Area Offset, if the header has been loaded
	bool header_valid;                   ///< Reports whether a valid header has been loaded.

	/**
	 * Instantiate a RemoteFRUStorage
	 *
	 * @param ipmb The IPMBSvc used to communicate with the FRU Storage Area
	 * @param ipmb_target The IPMI address of the device controlling the FRU Storage Area
	 * @param fru_device_id The ID of the FRU Storage Area
	 * @param size The size in bytes of the FRU Storage Area
	 * @param byte_addressed True if the storage area is byte addressed, else false if it is word addressed
	 * @param header The raw 8 byte header of the FRU Storage Area
	 */
	RemoteFRUStorage(IPMBSvc *ipmb, uint8_t ipmb_target, uint8_t fru_device_id, uint16_t size, bool byte_addressed, std::vector<uint8_t> header = std::vector<uint8_t>())
		: ipmb(ipmb), ipmb_target(ipmb_target), fru_device_id(fru_device_id), size(size), byte_addressed(byte_addressed), header_valid(false) {
		this->header_valid = (header.size() >= 8 && ipmi_checksum(header) == 0 && header[0] == 1 /* recognized version */);
		if (header.size() >= 1)
			this->header_version = header[0];
		if (this->header_valid) {
			this->internal_use_area_offset = header[1]*8;
			this->chassis_info_area_offset = header[2]*8;
			this->board_area_offset        = header[3]*8;
			this->product_info_area_offset = header[4]*8;
			this->multirecord_area_offset  = header[5]*8;
		}
	};

	static std::shared_ptr<RemoteFRUStorage> Probe(IPMBSvc *ipmb, uint8_t target, uint8_t dev, BaseType_t retry_delay=333);

	bool ReadHeader(BaseType_t retry_delay=333);

	std::vector<uint8_t> ReadData(uint16_t offset, uint16_t size, std::function<void(uint16_t offset, uint16_t size)> progress_callback=NULL, BaseType_t retry_delay=333) const;

	/**
	 * A structure representing the Chassis Area Info data.
	 */
	class ChassisInfo {
	public:
		uint8_t info_area_version; ///< The version of the Chassis Info Area record
		uint8_t type;              ///< The "Chassis Type" field.
		static const std::map<uint8_t, std::string> chassis_type_descriptions; ///< A table of chassis type descriptions.
		std::string part_number;   ///< The interpreted "Chassis Part Number" field.
		std::string serial_number; ///< The interpreted "Chassis Serial Number" field.
		std::vector<std::string> custom_info; ///< The interpreted "Custom Chassis Info" fields.
	};

	std::shared_ptr<ChassisInfo> ReadChassisInfoArea(BaseType_t retry_delay=333);

	/**
	 * A structure representing the Board Area Info data.
	 */
	class BoardArea {
	public:
		uint8_t board_area_version;           ///< The version of the Board Area record
		uint8_t language_code;                ///< The "Language Code" field.
		uint32_t mfg_timestamp;               ///< The MfgDate/Time field, parsed into a standard Epoch Timestamp
		std::string manufacturer;             ///< The interpreted "Board Manufacturer" field.
		std::string product_name;             ///< The interpreted "Board Product Name" field.
		std::string serial_number;            ///< The interpreted "Board Serial Number" field.
		std::string part_number;              ///< The interpreted "Board Part Number" field.
		std::string fru_file_id;              ///< The interpreted "FRU File ID" field.
		std::vector<std::string> custom_info; ///< The interpreted "Custom Mfg Info" fields.
	};

	static const std::map<uint8_t, std::string> language_codes; ///< Language Codes

	std::shared_ptr<BoardArea> ReadBoardArea(BaseType_t retry_delay=333);

	/**
	 * A structure representing the Board Area Info data.
	 */
	class ProductInfoArea {
	public:
		uint8_t info_area_version;            ///< The version of the Product Info Area record
		uint8_t language_code;                ///< The "Language Code" field.
		std::string manufacturer;             ///< The interpreted "Manufacturer Name" field.
		std::string product_name;             ///< The interpreted "Product Name" field.
		std::string product_partmodel_number; ///< The interpreted "Product Part/Model Number" field.
		std::string product_version;          ///< The interpreted "Product Version" field.
		std::string serial_number;            ///< The interpreted "Product Serial Number" field.
		std::string asset_tag;                ///< The interpreted "Asset Tag" field.
		std::string fru_file_id;              ///< The interpreted "FRU File ID" field.
		std::vector<std::string> custom_info; ///< The interpreted "Custom Product Area Info" fields.
	};

	std::shared_ptr<ProductInfoArea> ReadProductInfoArea(BaseType_t retry_delay=333);
};

#endif /* SRC_COMMON_UW_IPMC_SERVICES_IPMI_REMOTEFRUSTORAGE_H_ */
