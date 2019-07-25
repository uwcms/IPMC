/*
 * RemoteFRUStorage.cpp
 *
 *  Created on: Jul 18, 2018
 *      Author: jtikalsky
 */

#include <services/ipmi/RemoteFRUStorage.h>
#include <services/ipmi/IPMIFormats.h>

/**
 * Read out a FRU Storage Area header via the IPMB and return the parsed record.
 *
 * \param ipmb The IPMB to send the request on
 * \param target The IPMB address to request FRU Storage Area data from
 * \param dev The FRU Storage Device to query on the specified target
 * \param retry_delay This code will retry several times in the case of failure. vTaskDelay(retry_delay) between them.
 * \return A shared_ptr to a RemoteFRUStorage object, or a NULL shared_ptr on failure
 */
std::shared_ptr<RemoteFRUStorage> RemoteFRUStorage::Probe(IPMBSvc *ipmb, uint8_t target, uint8_t dev, BaseType_t retry_delay) {
	std::shared_ptr<IPMI_MSG> probersp;
	for (int retry = 0; retry < 3; ++retry) {
		probersp = ipmb->sendSync(std::make_shared<IPMI_MSG>(
				0, ipmb->getIPMBAddress(),
				0, target,
				(IPMI::Get_FRU_Inventory_Area_Info >> 8) & 0xff, IPMI::Get_FRU_Inventory_Area_Info & 0xff,
				std::vector<uint8_t> {dev}));
		if (!probersp || probersp->data_len != 4 || probersp->data[0] != IPMI::Completion::Success) {
			vTaskDelay(retry_delay);
			continue;
		}
		break;
	}
	if (!probersp || probersp->data_len != 4 || probersp->data[0] != IPMI::Completion::Success)
		return NULL;
	std::shared_ptr<RemoteFRUStorage> storage = std::make_shared<RemoteFRUStorage>(ipmb, target, dev,
			((probersp->data[2]<<8) | probersp->data[1]),
			!(probersp->data[3]&1)
			);
	storage->ReadHeader();
	return storage;
}

/**
 * Read the header from the FRU storage and populate this object's data.
 *
 * @param retry_delay This code will retry several times in the case of failure. vTaskDelay(retry_delay) between them.
 * @return true on success, else false (however check RemoteFRUStorage.header_checksum_valid!)
 */
bool RemoteFRUStorage::ReadHeader(BaseType_t retry_delay) {
	std::vector<uint8_t> header = this->ReadData(0, 8, NULL, retry_delay);
	this->header_version           = 0;
	this->internal_use_area_offset = 0;
	this->chassis_info_area_offset = 0;
	this->board_area_offset        = 0;
	this->product_info_area_offset = 0;
	this->multirecord_area_offset  = 0;

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
	return header.size() == 8;
}

/**
 * Read the specified FRU Storage Area contents.
 *
 * @param offset The offset to start reading from
 * @param size The number of bytes to read
 * @param progress_callback A function called every read to report progress, or NULL
 * @param retry_delay This code will retry several times in the case of failure. vTaskDelay(retry_delay) between them.
 * @return
 */
std::vector<uint8_t> RemoteFRUStorage::ReadData(uint16_t offset, uint16_t size, std::function<void(uint16_t offset, uint16_t size)> progress_callback, BaseType_t retry_delay) const {
	std::vector<uint8_t> outbuf;
	outbuf.reserve(size);
	while (size) {
		if (progress_callback)
			progress_callback(offset, size);
		uint8_t rd_size = 0x10;
		if (size < rd_size)
			rd_size = size;

		std::shared_ptr<IPMI_MSG> rd_rsp;
		for (int i = 0; i < 3; ++i) {
			rd_rsp = this->ipmb->sendSync(
					std::make_shared<IPMI_MSG>(0, this->ipmb->getIPMBAddress(), 0, this->ipmb_target,
							(IPMI::Read_FRU_Data >> 8) & 0xff,
							IPMI::Read_FRU_Data & 0xff,
							std::vector<uint8_t> {
									this->fru_device_id,
									static_cast<uint8_t>(offset & 0xff),
									static_cast<uint8_t>((offset >> 8) & 0xff),
									rd_size
							}
					)
			);
			if (!rd_rsp || (rd_rsp->data_len && rd_rsp->data[0] == IPMI::Completion::FRU_Device_Busy)) {
				vTaskDelay(retry_delay);
				continue;
			}
			break;
		}
		if (!rd_rsp || rd_rsp->data_len < 2 || rd_rsp->data[0] != IPMI::Completion::Success || rd_rsp->data_len != rd_rsp->data[1]+2) {
			outbuf.clear();
			return outbuf;
		}
		outbuf.insert(outbuf.end(), rd_rsp->data + 2, rd_rsp->data + rd_rsp->data_len);
		offset += rd_rsp->data[1];
		size -= rd_rsp->data[1];
	}
	return outbuf;
}

/**
 * Read and parse the Chassis Info Area of this FRU Data Area
 *
 * @param retry_delay This code will retry several times in the case of failure. vTaskDelay(retry_delay) between them.
 * @return A shared_ptr to a ChassisInfo record, or a null shared_ptr on failure
 */
std::shared_ptr<RemoteFRUStorage::ChassisInfo> RemoteFRUStorage::ReadChassisInfoArea(BaseType_t retry_delay) {
	std::vector<uint8_t> data = this->ReadData(this->chassis_info_area_offset, 2);
	if (data.size() < 2)
		return NULL; // Unable to read value.
	if (data[0] != 1)
		return NULL; // Unsupported format.
	uint16_t info_area_length = data[1]*8;
	data = this->ReadData(this->chassis_info_area_offset, info_area_length);
	if (data.size() != info_area_length)
		return NULL; // Unable to read data area.
	if (ipmi_checksum(data) != 0)
		return NULL; // Corrupt data area.

	std::shared_ptr<RemoteFRUStorage::ChassisInfo> chassis = std::make_shared<RemoteFRUStorage::ChassisInfo>();
	uint16_t parse_offset = 0;

	chassis->info_area_version = data[parse_offset++];

	if (data[parse_offset++] * 8 != info_area_length)
		return NULL; // Data changed during processing.

#define CHECK_OFFSET(len) \
	do { \
		if (parse_offset + (len) > info_area_length) \
			return NULL; /* Invalid Type/Length field. */ \
	} while (0)

#define PARSE_TLFIELD(field) \
	do { \
		uint8_t len = data[parse_offset] & 0x3f; \
		CHECK_OFFSET(len); \
		std::vector<uint8_t> subvec(data.begin()+parse_offset, data.begin()+parse_offset+1+len); \
		field = render_ipmi_type_length_field(subvec); \
		parse_offset += 1+len; \
	} while (0);

	chassis->type = data[parse_offset++];

	PARSE_TLFIELD(chassis->part_number);

	PARSE_TLFIELD(chassis->serial_number);

	while (data[parse_offset] != 0xC1 /* End Marker */) {
		uint8_t len = data[parse_offset] & 0x3f;
		CHECK_OFFSET(len);
		std::vector<uint8_t> subvec(data.begin()+parse_offset, data.begin()+parse_offset+1+len);
		chassis->custom_info.push_back(render_ipmi_type_length_field(subvec));
		parse_offset += 1+len;
	}
#undef PARSE_TLFIELD
#undef CHECK_OFFSET
	return chassis;
}

/**
 * Read and parse the Board Area of this FRU Data Area
 *
 * @param retry_delay This code will retry several times in the case of failure. vTaskDelay(retry_delay) between them.
 * @return A shared_ptr to a BoardArea record, or a null shared_ptr on failure
 */
std::shared_ptr<RemoteFRUStorage::BoardArea> RemoteFRUStorage::ReadBoardArea(BaseType_t retry_delay) {
	std::vector<uint8_t> data = this->ReadData(this->board_area_offset, 2);
	if (data.size() < 2)
		return NULL; // Unable to read value.
	if (data[0] != 1)
		return NULL; // Unsupported format.
	uint16_t info_area_length = data[1]*8;
	data = this->ReadData(this->board_area_offset, info_area_length);
	if (data.size() != info_area_length)
		return NULL; // Unable to read data area.
	if (ipmi_checksum(data) != 0)
		return NULL; // Corrupt data area.

	std::shared_ptr<RemoteFRUStorage::BoardArea> board = std::make_shared<RemoteFRUStorage::BoardArea>();
	uint16_t parse_offset = 0;

	board->board_area_version = data[parse_offset++];

	if (data[parse_offset++] * 8 != info_area_length)
		return NULL; // Data changed during processing.

#define CHECK_OFFSET(len) \
	do { \
		if (parse_offset + (len) > info_area_length) \
			return NULL; /* Invalid Type/Length field. */ \
	} while (0)

#define PARSE_TLFIELD(field) \
	do { \
		uint8_t len = data[parse_offset] & 0x3f; \
		CHECK_OFFSET(len); \
		std::vector<uint8_t> subvec(data.begin()+parse_offset, data.begin()+parse_offset+1+len); \
		field = render_ipmi_type_length_field(subvec); \
		parse_offset += 1+len; \
	} while (0);

	board->language_code = data[parse_offset++];

	board->mfg_timestamp = 0;
	for (int i = 0; i < 3; ++i)
		board->mfg_timestamp = data[parse_offset++] << 8*i;
	if (board->mfg_timestamp)
		board->mfg_timestamp = (board->mfg_timestamp*60) + 820454400; // (Minutes->Seconds) + 1996-01-01T00:00:00Z

	PARSE_TLFIELD(board->manufacturer);

	PARSE_TLFIELD(board->product_name);

	PARSE_TLFIELD(board->serial_number);

	PARSE_TLFIELD(board->part_number);

	PARSE_TLFIELD(board->fru_file_id);

	while (data[parse_offset] != 0xC1 /* End Marker */) {
		uint8_t len = data[parse_offset] & 0x3f;
		CHECK_OFFSET(len);
		std::vector<uint8_t> subvec(data.begin()+parse_offset, data.begin()+parse_offset+1+len);
		board->custom_info.push_back(render_ipmi_type_length_field(subvec));
		parse_offset += 1+len;
	}
#undef PARSE_TLFIELD
#undef CHECK_OFFSET
	return board;
}

/**
 * Read and parse the Product Info Area of this FRU Data Area
 *
 * @param retry_delay This code will retry several times in the case of failure. vTaskDelay(retry_delay) between them.
 * @return A shared_ptr to a ProductInfoArea record, or a null shared_ptr on failure
 */
std::shared_ptr<RemoteFRUStorage::ProductInfoArea> RemoteFRUStorage::ReadProductInfoArea(BaseType_t retry_delay) {
	std::vector<uint8_t> data = this->ReadData(this->product_info_area_offset, 2);
	if (data.size() < 2)
		return NULL; // Unable to read value.
	if (data[0] != 1)
		return NULL; // Unsupported format.
	uint16_t info_area_length = data[1]*8;
	data = this->ReadData(this->product_info_area_offset, info_area_length);
	if (data.size() != info_area_length)
		return NULL; // Unable to read data area.
	if (ipmi_checksum(data) != 0)
		return NULL; // Corrupt data area.

	std::shared_ptr<RemoteFRUStorage::ProductInfoArea> product = std::make_shared<RemoteFRUStorage::ProductInfoArea>();
	uint16_t parse_offset = 0;

	product->info_area_version = data[parse_offset++];

	if (data[parse_offset++] * 8 != info_area_length)
		return NULL; // Data changed during processing.

#define CHECK_OFFSET(len) \
	do { \
		if (parse_offset + (len) > info_area_length) \
			return NULL; /* Invalid Type/Length field. */ \
	} while (0)

#define PARSE_TLFIELD(field) \
	do { \
		uint8_t len = data[parse_offset] & 0x3f; \
		CHECK_OFFSET(len); \
		std::vector<uint8_t> subvec(data.begin()+parse_offset, data.begin()+parse_offset+1+len); \
		field = render_ipmi_type_length_field(subvec); \
		parse_offset += 1+len; \
	} while (0);

	product->language_code = data[parse_offset++];

	PARSE_TLFIELD(product->manufacturer);

	PARSE_TLFIELD(product->product_name);

	PARSE_TLFIELD(product->product_partmodel_number);

	PARSE_TLFIELD(product->product_version);

	PARSE_TLFIELD(product->serial_number);

	PARSE_TLFIELD(product->asset_tag);

	PARSE_TLFIELD(product->fru_file_id);

	while (data[parse_offset] != 0xC1 /* End Marker */) {
		uint8_t len = data[parse_offset] & 0x3f;
		CHECK_OFFSET(len);
		std::vector<uint8_t> subvec(data.begin()+parse_offset, data.begin()+parse_offset+1+len);
		product->custom_info.push_back(render_ipmi_type_length_field(subvec));
		parse_offset += 1+len;
	}
#undef PARSE_TLFIELD
#undef CHECK_OFFSET
	return product;
}

std::vector< std::vector<uint8_t> > RemoteFRUStorage::ReadMultiRecordArea(BaseType_t retry_delay) {
	std::vector< std::vector<uint8_t> > records;
	uint16_t offset = this->multirecord_area_offset;
	while (true) {
		std::vector<uint8_t> data = this->ReadData(offset, 5);
		if (data.size() < 5)
			return std::vector< std::vector<uint8_t> >(); // Unable to read value.
		if (ipmi_checksum(data) != 0)
			return std::vector< std::vector<uint8_t> >(); // Header checksum failure.
		uint16_t size = 5+data[2];
		data = this->ReadData(offset, size);
		if (data.size() != size)
			return std::vector< std::vector<uint8_t> >(); // Read failure.
		if (ipmi_checksum(std::vector<uint8_t>(data.begin(), std::next(data.begin(), 5))) != 0)
			return std::vector< std::vector<uint8_t> >(); // Header checksum failure on full read.
		if (ipmi_checksum(std::vector<uint8_t>(std::next(data.begin(), 5), data.end())) != data[3])
			return std::vector< std::vector<uint8_t> >(); // Record checksum failure.
		records.emplace_back(data);
		if (data[1] & 0x80 /* EOL */)
			break; // Last entry.
		offset += size;
	}
	return records;
}

const std::map<uint8_t, std::string> RemoteFRUStorage::ChassisInfo::chassis_type_descriptions{
	{ 0x01, "Other"                 },
	{ 0x02, "Unknown"               },
	{ 0x03, "Desktop"               },
	{ 0x04, "Low Profile Desktop"   },
	{ 0x05, "Pizza Box"             },
	{ 0x06, "Mini Tower"            },
	{ 0x07, "Tower"                 },
	{ 0x08, "Portable"              },
	{ 0x09, "Laptop"                },
	{ 0x0A, "Notebook"              },
	{ 0x0B, "Hand Held"             },
	{ 0x0C, "Docking Station"       },
	{ 0x0D, "All in One"            },
	{ 0x0E, "Sub Notebook"          },
	{ 0x0F, "Space-saving"          },
	{ 0x10, "Lunch Box"             },
	{ 0x11, "Main Server Chassis"   },
	{ 0x12, "Expansion Chassis"     },
	{ 0x13, "SubChassis"            },
	{ 0x14, "Bus Expansion Chassis" },
	{ 0x15, "Peripheral Chassis"    },
	{ 0x16, "RAID Chassis"          },
	{ 0x17, "Rack Mount Chassis"    },
	{ 0x18, "Sealed-case PC"        },
	{ 0x19, "Multi-system chassis"  },
	{ 0x1A, "Compact PCI"           },
	{ 0x1B, "Advanced TCA"          },
	{ 0x1C, "Blade"                 },
	{ 0x1D, "Blade Enclosure"       },
	{ 0x1E, "Tablet"                },
	{ 0x1F, "Convertible"           },
	{ 0x20, "Detachable"            },
	{ 0x21, "IoT Gateway"           },
	{ 0x22, "Embedded PC"           },
	{ 0x23, "Mini PC"               },
	{ 0x24, "Stick PC"              },
};

const std::map<uint8_t, std::string> RemoteFRUStorage::language_codes{
	{ 0,   "en English*"         },
	{ 1,   "aa Afar"             },
	{ 2,   "ab Abkhazian"        },
	{ 3,   "af Afrikaans"        },
	{ 4,   "am Amharic"          },
	{ 5,   "ar Arabic"           },
	{ 6,   "as Assamese"         },
	{ 7,   "ay Aymara"           },
	{ 8,   "az Azerbaijani"      },
	{ 9,   "ba Bashkir"          },
	{ 10,  "be Byelorussian"     },
	{ 11,  "bg Bulgarian"        },
	{ 12,  "bh Bihari"           },
	{ 13,  "bi Bislama"          },
	{ 14,  "bn Bengali; Bangla"  },
	{ 15,  "bo Tibetan"          },
	{ 16,  "br Breton"           },
	{ 17,  "ca Catalan"          },
	{ 18,  "co Corsican"         },
	{ 19,  "cs Czech"            },
	{ 20,  "cy Welsh"            },
	{ 21,  "da danish"           },
	{ 22,  "de german"           },
	{ 23,  "dz Bhutani"          },
	{ 24,  "el Greek"            },
	{ 25,  "en English"          },
	{ 26,  "eo Esperanto"        },
	{ 27,  "es Spanish"          },
	{ 28,  "et Estonian"         },
	{ 29,  "eu Basque"           },
	{ 30,  "fa Persian"          },
	{ 31,  "fi Finnish"          },
	{ 32,  "fj Fiji"             },
	{ 33,  "fo Faeroese"         },
	{ 34,  "fr French"           },
	{ 35,  "fy Frisian"          },
	{ 36,  "ga Irish"            },
	{ 37,  "gd Scots Gaelic"     },
	{ 38,  "gl Galician"         },
	{ 39,  "gn Guarani"          },
	{ 40,  "gu Gujarati"         },
	{ 41,  "ha Hausa"            },
	{ 42,  "hi Hindi"            },
	{ 43,  "hr Croatian"         },
	{ 44,  "hu Hungarian"        },
	{ 45,  "hy Armenian"         },
	{ 46,  "ia Interlingua"      },
	{ 47,  "ie Interlingue"      },
	{ 48,  "ik Inupiak"          },
	{ 49,  "in Indonesian"       },
	{ 50,  "is Icelandic"        },
	{ 51,  "it Italian"          },
	{ 52,  "iw Hebrew"           },
	{ 53,  "ja Japanese"         },
	{ 54,  "ji Yiddish"          },
	{ 55,  "jw Javanese"         },
	{ 56,  "ka Georgian"         },
	{ 57,  "kk Kazakh"           },
	{ 58,  "kl Greenlandic"      },
	{ 59,  "km Cambodian"        },
	{ 60,  "kn Kannada"          },
	{ 61,  "ko Korean"           },
	{ 62,  "ks Kashmiri"         },
	{ 63,  "ku Kurdish"          },
	{ 64,  "ky Kirghiz"          },
	{ 65,  "la Latin"            },
	{ 66,  "ln Lingala"          },
	{ 67,  "lo Laothian"         },
	{ 68,  "lt Lithuanian"       },
	{ 69,  "lv Latvian, Lettish" },
	{ 70,  "mg Malagasy"         },
	{ 71,  "mi Maori"            },
	{ 72,  "mk Macedonian"       },
	{ 73,  "ml Malayalam"        },
	{ 74,  "mn Mongolian"        },
	{ 75,  "mo Moldavian"        },
	{ 76,  "mr Marathi"          },
	{ 77,  "ms Malay"            },
	{ 78,  "mt Maltese"          },
	{ 79,  "my Burmese"          },
	{ 80,  "na Nauru"            },
	{ 81,  "ne Nepali"           },
	{ 82,  "nl Dutch"            },
	{ 83,  "no Norwegian"        },
	{ 84,  "oc Occitan"          },
	{ 85,  "om (Afan) Oromo"     },
	{ 86,  "or Oriya"            },
	{ 87,  "pa Punjabi"          },
	{ 88,  "pl Polish"           },
	{ 89,  "ps Pashto, Pushto"   },
	{ 90,  "pt Portuguese"       },
	{ 91,  "qu Quechua"          },
	{ 92,  "rm Rhaeto-Romance"   },
	{ 93,  "rn Kirundi"          },
	{ 94,  "ro Romanian"         },
	{ 95,  "ru Russian"          },
	{ 96,  "rw Kinyarwanda"      },
	{ 97,  "sa Sanskrit"         },
	{ 98,  "sd Sindhi"           },
	{ 99,  "sg Sangro"           },
	{ 100, "sh Serbo-Croatian"   },
	{ 101, "si Singhalese"       },
	{ 102, "sk Slovak"           },
	{ 103, "sl Slovenian"        },
	{ 104, "sm Samoan"           },
	{ 105, "sn Shona"            },
	{ 106, "so Somali"           },
	{ 107, "sq Albanian"         },
	{ 108, "sr Serbian"          },
	{ 109, "ss Siswati"          },
	{ 110, "st Sesotho"          },
	{ 111, "su Sudanese"         },
	{ 112, "sv Swedish"          },
	{ 113, "sw Swahili"          },
	{ 114, "ta Tamil"            },
	{ 115, "te Tegulu"           },
	{ 116, "tg Tajik"            },
	{ 117, "th Thai"             },
	{ 118, "ti Tigrinya"         },
	{ 119, "tk Turkmen"          },
	{ 120, "tl Tagalog"          },
	{ 121, "tn Setswana"         },
	{ 122, "to Tonga"            },
	{ 123, "tr Turkish"          },
	{ 124, "ts Tsonga"           },
	{ 125, "tt Tatar"            },
	{ 126, "tw Twi"              },
	{ 127, "uk Ukrainian"        },
	{ 128, "ur Urdu"             },
	{ 129, "uz Uzbek"            },
	{ 130, "vi Vietnamese"       },
	{ 131, "vo Volapuk"          },
	{ 132, "wo Wolof"            },
	{ 133, "xh Xhosa"            },
	{ 134, "yo Yoruba"           },
	{ 135, "zh Chinese"          },
	{ 136, "zu Zulu"             },
};
