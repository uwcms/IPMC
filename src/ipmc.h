/*
 * ipmc.h
 *
 *  Created on: Apr 4, 2019
 *      Author: mpv
 */

#ifndef SRC_IPMC_H_
#define SRC_IPMC_H_

#include <xil_types.h>

u8 get_ipmc_hw_rev();

u8 get_ipmc_target_image();

void tag_image(u8 image);


#endif /* SRC_IPMC_H_ */
