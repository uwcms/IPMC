#ifndef SRC_COMMON_UW_IPMC_LIBS_EXCEPT_H_
#define SRC_COMMON_UW_IPMC_LIBS_EXCEPT_H_

#include <exception>
#include <stdexcept>
#include <string>

// Global custom exceptions
#define DEFINE_GENERIC_EXCEPTION(name, base) \
	namespace except { \
	class name : public base { \
	public: \
		explicit name(const std::string& what_arg) : base(what_arg) { }; \
		explicit name(const char* what_arg) : base(what_arg) { }; \
	}; \
	}

DEFINE_GENERIC_EXCEPTION(hardware_error, std::runtime_error)

#endif /* SRC_COMMON_UW_IPMC_LIBS_EXCEPT_H_ */
