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

#ifndef SRC_COMMON_ZYNQIPMC_SERVICES_INFOLINK_INFOLINK_H_
#define SRC_COMMON_ZYNQIPMC_SERVICES_INFOLINK_INFOLINK_H_

#include <drivers/elm/elm.h>
#include <libs/logtree/logtree.h>
#include <libs/threading.h>
#include <map>
#include <services/console/command_parser.h>

/**
 * A class providing IPMC/ELM information exchange facilities.
 *
 * Local statstics or information can be stored with the class via static
 * methods, which will then be made available by instances to their attached
 * ELMs.
 */
class InfoLink : public ELM::Channel, public ConsoleCommandSupport {
public:
	/**
	 * Instantiate an InfoLink module.
	 *
	 * \param elm The ELM interface to provide ELMLink service to.
	 */
	InfoLink(ELM &elm);
	virtual ~InfoLink();

	class MultiTypeValue {
	public:
		enum {
			TYPE_NULL,
			TYPE_BOOL,
			TYPE_INT,
			TYPE_UINT32,
			TYPE_FLOAT,
			TYPE_STRING,
			TYPE_LAMBDA,
		} value_type;

		union {
			bool vbool;
			int vint;
			uint32_t vuint32;
			float vfloat;
		} vsimple;

		/** The value if it is a string.
		 */
		std::string vstring;

		/** A function to be called to determine the MTV value.
		 * This MUST NOT return a TYPE_LAMBDA MTV.
		 */
		std::function<MultiTypeValue(void)> vlambda = nullptr;

		MultiTypeValue() : value_type(TYPE_NULL), vsimple{.vbool = false} {};
		explicit MultiTypeValue(bool value) : value_type(TYPE_BOOL), vsimple{.vbool = value} {};
		explicit MultiTypeValue(int value) : value_type(TYPE_INT), vsimple{.vint = value} {};
		explicit MultiTypeValue(uint32_t value) : value_type(TYPE_UINT32), vsimple{.vuint32 = value} {};
		explicit MultiTypeValue(float value) : value_type(TYPE_FLOAT), vsimple{.vfloat = value} {};
		MultiTypeValue(const std::string &value) : value_type(TYPE_STRING), vsimple{.vbool = false}, vstring(value){};
		MultiTypeValue(const std::function<MultiTypeValue(void)> &value) : value_type(TYPE_LAMBDA), vsimple{.vbool = false}, vlambda(value){};

		std::string renderJSON();
	};

	static void setInfo(std::string key, MultiTypeValue value);
	static void delInfo(std::string key);
	static std::map<std::string, MultiTypeValue> getMyInfo();

	/**
	 * A function to fetch the ELM's info.
	 *
	 * \param timeout How long to wait. A value of 0 will return a pre-cached result.
	 *                Any other value will return a fresh result, or "".
	 *
	 * \return The ELM will send whatever it wants.  It SHOULD be a pretty
	 *         formatted JSON object. We will not parse it for you, as we don't
	 *         know what you're looking for.
	 */
	std::string getELMInfo(TickType_t timeout = 5000);

	// From base class ConsoleCommandSupport:
	virtual void registerConsoleCommands(CommandParser &parser, const std::string &prefix = "");
	virtual void deregisterConsoleCommands(CommandParser &parser, const std::string &prefix = "");

protected:
	SemaphoreHandle_t mutex; ///< A mutex to guard things.

	/**
	 * A mapping of various information that will be rendered to a JSON object
	 * to send to the ELM upon request.
	 */
	static std::map<std::string, MultiTypeValue> *info;

	virtual void recv(const uint8_t *content, size_t size);
	virtual void sendInfo();

	WaitList<true> waitlist;

	std::string last_elm_info;
};

#endif /* SRC_COMMON_ZYNQIPMC_SERVICES_INFOLINK_INFOLINK_H_ */
