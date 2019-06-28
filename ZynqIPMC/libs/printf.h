/*
 * printf.h
 *
 *  Created on: Nov 15, 2018
 *      Author: jtikalsky
 */

#ifndef SRC_COMMON_UW_IPMC_LIBS_PRINTF_H_
#define SRC_COMMON_UW_IPMC_LIBS_PRINTF_H_

#include <string>

std::string stdsprintf(const char *fmt, ...) __attribute__((format(printf,1,2)));
void windows_newline(std::string &input, char nlchar='\n');
std::string cxa_demangle(const char *name);

#endif /* SRC_COMMON_UW_IPMC_LIBS_PRINTF_H_ */
