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

#include <libs/printf.h>
#include <services/ipmi/ipmi_formats.h>
#include <iterator>

unsigned getIpmiTypeLengthFieldLength(const std::vector<uint8_t> &data) {
	if (data.empty()) return 0;
	//const uint8_t field_type = data[0] >> 6;
	const unsigned field_length = data[0] & 0x3f;

	if (1+field_length > data.size()) {
		return 0;
	}

	return 1+field_length;
}

std::string renderIpmiTypeLengthField(const std::vector<uint8_t> &data) {
	if (data.empty()) {
		return "<Invalid (Short) Type/Length Field>";
	}

	const uint8_t field_type = data[0] >> 6;
	const unsigned field_length = data[0] & 0x3f;

	if (field_length == 0) {
		return "<nullptr>";
	}

	if (1+field_length > data.size()) {
		return "<Invalid (Short) Type/Length Field>";
	}

	if (field_type == 0) {
		// Binary or Unspecified.
		std::string out;
		for (std::vector<uint8_t>::size_type i = 0; i < field_length; ++i) {
			if (out.size())
				out += " ";
			out += stdsprintf("0x%02hhx", data[1+i]);
		}
		return out;

	} else if (field_type == 1) {
		// BCD
		std::string out;
		for (std::vector<uint8_t>::size_type i = 0; i < field_length; ++i) {
			static const char *bcd_table = "01234567889 -.???";
			out += bcd_table[data[1+i] >> 4];
			out += bcd_table[data[1+i] & 0x0f];
		}
		return out;

	} else if (field_type == 2) {
		// Packed 6-bit ASCII

		std::string out;
		// The length of the field, in bits, divided by 6, to get the number of complete 6bit chars.
		std::vector<uint8_t>::size_type count6b = (field_length*8)/6;
		for (std::vector<uint8_t>::size_type i = 0; i*6 < count6b; ++i) {
			char c;

			// Calculate the byte offset in which this char starts.
			std::vector<uint8_t>::size_type offset = ((i*6)/8);
			switch (i % 4) {
				case 0:
					/* The first char in a cycle is zero aligned.
					 *
					 * Nothing fancy here.
					 */
					c = data[1+offset] & 0x3f;
					break;
				case 1:
					/* The second char in a cycle is offset by 6.
					 *
					 * Take the last two of that, and the first two of the next.
					 */
					c = (data[1+offset+1] & 0x0f) | (data[1+offset] >> 6);
					break;
				case 2:
					/* The third char in a cycle is offset by 4.
					 *
					 * Take the last four of that, and the first two of the next.
					 */
					c = (data[1+offset+1] & 0x03) | (data[1+offset] >> 4);
					break;
				case 3:
					/* The fourth char in a cycle is offset by 2.
					 *
					 * Take the last 6 of that.
					 */
					c = data[1+offset] >> 2;
					break;
			}
			// Now convert 6b to 8b ASCII.
			out += (' ' +c);
		}
		return out;

	} else if (field_type == 3) {
		// Raw ASCII or Unicode.
		return std::string(std::next(data.begin()), std::next(data.begin(),1+field_length));

	} else {
		// We have already iterated over values [0,1,2,3] of a 2 bit field.
		return "";
	}
}

std::vector<uint8_t> encodeIpmiTypeLengthField(const std::string &data, bool prevent_c1) {
	std::vector<uint8_t> out(data.begin(), data.end());
	if (out.size() > 63) {
		out.resize(63); // Trim, we can only encode this much.
	}

	if (out.size() == 1 && prevent_c1) {
		out.push_back(' '); // 0xC1 tends to be 'end of record list', so we'll add a space to the end of any one-letter field.
	}

	out.insert(out.begin(), 0xc0 /* Raw ASCII or Unicode */ | static_cast<uint8_t>(out.size()));
	return out;
}
