/*
 * IPMIFormats.cpp
 *
 *  Created on: Jul 27, 2018
 *      Author: jtikalsky
 */

#include <services/ipmi/IPMIFormats.h>
#include <IPMC.h> // stdsprintf()

/**
 * Unpack and render a field specified in Type/Length byte format, as
 * specified by the Platform Management FRU Information Storage Definition v1.0.
 *
 * @param data The field data including the type/length code byte.
 * @return The rendered data in human readable format.
 */
std::string render_ipmi_type_length_field(std::vector<uint8_t> &data) {
	if (data.empty())
		return "<Invalid (Short) Type/Length Field>";
	const uint8_t field_type = data[0] >> 6;
	const uint8_t field_length = data[0] & 0x3f;
	data.erase(data.begin());
	if (field_length == 0)
		return "<NULL>";
	if (field_length < data.size())
		return "<Invalid (Short) Type/Length Field>";
	if (field_type == 0) {
		// Binary or Unspecified.
		std::string out;
		for (std::vector<uint8_t>::size_type i = 0; i < field_length; ++i) {
			if (out.size())
				out += " ";
			out += stdsprintf("0x%02hhx", data[i]);
		}
		return out;
	}
	else if (field_type == 1) {
		// BCD
		std::string out;
		for (std::vector<uint8_t>::size_type i = 0; i < field_length; ++i) {
			static const char *bcd_table = "01234567889 -.???";
			out += bcd_table[data[i] >> 4];
			out += bcd_table[data[i] & 0x0f];
		}
		return out;
	}
	else if (field_type == 2) {
		// Packed 6-bit ASCII

		std::string out;
		// The length of the field, in bits, divided by 6, to get the number of complete 6bit chars.
		std::vector<uint8_t>::size_type count6b = (field_length*8)/6;
		for (std::vector<uint8_t>::size_type i = 0; i*6 < count6b; ++i) {
			char c;
			// Calculate the byte offset in which this char starts.
			std::vector<uint8_t>::size_type offset = (i*6)/8;
			switch (i % 4) {
			case 0:
				/* The first char in a cycle is zero aligned.
				 *
				 * Nothing fancy here.
				 */
				c = data[offset] & 0x3f;
				break;
			case 1:
				/* The second char in a cycle is offset by 6.
				 *
				 * Take the last two of that, and the first two of the next.
				 */
				c = (data[offset+1] & 0x0f) | (data[offset] >> 6);
				break;
			case 2:
				/* The third char in a cycle is offset by 4.
				 *
				 * Take the last four of that, and the first two of the next.
				 */
				c = (data[offset+1] & 0x03) | (data[offset] >> 4);
				break;
			case 3:
				/* The fourth char in a cycle is offset by 2.
				 *
				 * Take the last 6 of that.
				 */
				c = data[offset] >> 2;
				break;
			}
			// Now convert 6b to 8b ASCII.
			out += (' ' +c);
		}
		return out;
	}
	else if (field_type == 3) {
		// Raw ASCII or Unicode.
		return std::string(data.begin(), data.begin()+field_length);
	}
	else {
		// We have already iterated over values [0,1,2,3] of a 2 bit field.
		configASSERT(0);
		__builtin_unreachable();
	}
}
