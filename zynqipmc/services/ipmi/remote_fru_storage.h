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

#ifndef SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_REMOTE_FRU_STORAGE_H_
#define SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_REMOTE_FRU_STORAGE_H_

#include <services/ipmi/ipmbsvc/ipmbsvc.h>
#include <stddef.h>
#include <memory>
#include <vector>

/**
 * A class representing a remote FRU Storage Area, with methods for accessing it.
 */
class RemoteFRUStorage final {
public:
	/**
	 * Instantiate a RemoteFRUStorage.
	 *
	 * @param ipmb The IPMBSvc used to communicate with the FRU Storage Area.
	 * @param ipmb_target The IPMI address of the device controlling the FRU Storage Area.
	 * @param fru_device_id The ID of the FRU Storage Area.
	 * @param size The size in bytes of the FRU Storage Area.
	 * @param byte_addressed True if the storage area is byte addressed, else false if it is word addressed.
	 * @param header The raw 8 byte header of the FRU Storage Area.
	 */
	RemoteFRUStorage(IPMBSvc *ipmb, uint8_t ipmb_target, uint8_t fru_device_id, uint16_t size, bool byte_addressed, std::vector<uint8_t> header = std::vector<uint8_t>())
		: ipmb(ipmb), ipmb_target(ipmb_target), fru_device_id(fru_device_id), size(size), byte_addressed(byte_addressed), header_valid(false) {
		this->header_valid = (header.size() >= 8 && IPMIMessage::checksum(header) == 0 && header[0] == 1 /* recognized version */);
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

	/**
	 * Read out a FRU Storage Area header via the IPMB and return the parsed record.
	 *
	 * @param ipmb The IPMB to send the request on.
	 * @param target The IPMB address to request FRU Storage Area data from.
	 * @param dev The FRU Storage Device to query on the specified target.
	 * @param retry_delay This code will retry several times in the case of failure. vTaskDelay(retry_delay) between them.
	 * @return A shared_ptr to a RemoteFRUStorage object, or a nullptr shared_ptr on failure.
	 */
	static std::shared_ptr<RemoteFRUStorage> probe(IPMBSvc *ipmb, uint8_t target, uint8_t dev, BaseType_t retry_delay=333);

	/**
	 * Read the header from the FRU storage and populate this object's data.
	 *
	 * @param retry_delay This code will retry several times in the case of failure. vTaskDelay(retry_delay) between them.
	 * @return true on success, else false (however check RemoteFRUStorage.header_checksum_valid!).
	 */
	bool readHeader(BaseType_t retry_delay=333);

	/**
	 * Read the specified FRU Storage Area contents.
	 *
	 * @param offset The offset to start reading from.
	 * @param size The number of bytes to read.
	 * @param progress_callback A function called every read to report progress, or nullptr.
	 * @param retry_delay This code will retry several times in the case of failure. vTaskDelay(retry_delay) between them.
	 * @return
	 */
	std::vector<uint8_t> readData(uint16_t offset, uint16_t size, std::function<void(uint16_t offset, uint16_t size)> progress_callback=nullptr, BaseType_t retry_delay=333) const;

	/**
	 * A structure representing the Chassis Area Info data.
	 */
	class ChassisInfo {
	public:
		uint8_t info_area_version; ///< The version of the Chassis Info Area record.
		uint8_t type;              ///< The "Chassis Type" field.
		static const std::map<uint8_t, std::string> chassis_type_descriptions; ///< A table of chassis type descriptions.
		std::string part_number;   ///< The interpreted "Chassis Part Number" field.
		std::string serial_number; ///< The interpreted "Chassis Serial Number" field.
		std::vector<std::string> custom_info; ///< The interpreted "Custom Chassis Info" fields.
	};

	/**
	 * Read and parse the Chassis Info Area of this FRU Data Area.
	 *
	 * @param retry_delay This code will retry several times in the case of failure. vTaskDelay(retry_delay) between them.
	 * @return A shared_ptr to a ChassisInfo record, or a null shared_ptr on failure.
	 */
	std::shared_ptr<ChassisInfo> readChassisInfoArea(BaseType_t retry_delay=333);

	/**
	 * A structure representing the Board Area Info data.
	 */
	class BoardArea {
	public:
		uint8_t board_area_version;           ///< The version of the Board Area record.
		uint8_t language_code;                ///< The "Language Code" field.
		uint32_t mfg_timestamp;               ///< The MfgDate/Time field, parsed into a standard Epoch Timestamp.
		std::string manufacturer;             ///< The interpreted "Board Manufacturer" field.
		std::string product_name;             ///< The interpreted "Board Product Name" field.
		std::string serial_number;            ///< The interpreted "Board Serial Number" field.
		std::string part_number;              ///< The interpreted "Board Part Number" field.
		std::string fru_file_id;              ///< The interpreted "FRU File ID" field.
		std::vector<std::string> custom_info; ///< The interpreted "Custom Mfg Info" fields.
	};

	static const std::map<uint8_t, std::string> language_codes; ///< Language Codes

	/**
	 * Read and parse the Board Area of this FRU Data Area.
	 *
	 * @param retry_delay This code will retry several times in the case of failure. vTaskDelay(retry_delay) between them.
	 * @return A shared_ptr to a BoardArea record, or a null shared_ptr on failure.
	 */
	std::shared_ptr<BoardArea> readBoardArea(BaseType_t retry_delay=333);

	/**
	 * A structure representing the Board Area Info data.
	 */
	class ProductInfoArea {
	public:
		uint8_t info_area_version;            ///< The version of the Product Info Area record.
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

	/**
	 * Read and parse the Product Info Area of this FRU Data Area.
	 *
	 * @param retry_delay This code will retry several times in the case of failure. vTaskDelay(retry_delay) between them.
	 * @return A shared_ptr to a ProductInfoArea record, or a null shared_ptr on failure.
	 */
	std::shared_ptr<ProductInfoArea> readProductInfoArea(BaseType_t retry_delay=333);

	std::vector< std::vector<uint8_t> > readMultiRecordArea(BaseType_t retry_delay=333);

	/// Getters
	/// @{
	inline uint8_t getFruDeviceId() const { return this->fru_device_id; };
	inline uint16_t getStorageAreaSize() const { return this->size; };
	inline bool isByteAddressed() const { return this->byte_addressed; };
	inline uint8_t getHeaderVersion() const { return this->header_version; };
	inline uint16_t getInternalUseAreaOffset() const { return this->internal_use_area_offset; };
	inline uint16_t getChassisInfoAreaOffset() const { return this->chassis_info_area_offset; };
	inline uint16_t getBoardAreaOffset() const { return this->board_area_offset; };
	inline uint16_t getProductInfoAreaOffset() const { return this->product_info_area_offset; };
	inline uint16_t getMultirecordAreaOffset() const { return this->multirecord_area_offset; };
	inline bool isHeaderValid() const { return this->header_valid; };
	/// @}

private:
	IPMBSvc *ipmb;                       ///< The IPMBSvc used to communicate with the FRU Storage Area.
	uint8_t ipmb_target;                 ///< The IPMI address of the device controlling the FRU Storage Area.
	uint8_t fru_device_id;               ///< The ID of the FRU Storage Area.

	uint16_t size;                       ///< The size in bytes of the FRU Storage Area.
	bool byte_addressed;                 ///< True if the storage area is byte addressed, else false if it is word addressed.

	uint8_t header_version;              ///< The version of the Storage Area header, if it has been loaded.
	uint16_t internal_use_area_offset;   ///< The Internal Use Area Offset, if the header has been loaded.
	uint16_t chassis_info_area_offset;   ///< The Chassis Info Area Offset, if the header has been loaded.
	uint16_t board_area_offset;          ///< The Board Area Offset, if the header has been loaded.
	uint16_t product_info_area_offset;   ///< The Product Info Area Offset, if the header has been loaded.
	uint16_t multirecord_area_offset;    ///< The MultiRecord Area Offset, if the header has been loaded.
	bool header_valid;                   ///< Reports whether a valid header has been loaded.
};

#endif /* SRC_COMMON_ZYNQIPMC_SERVICES_IPMI_REMOTE_FRU_STORAGE_H_ */
