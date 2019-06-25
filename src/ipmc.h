/*
 * ipmc.h
 *
 *  Created on: Apr 4, 2019
 *      Author: mpv
 */

#ifndef SRC_IPMC_H_
#define SRC_IPMC_H_

#include <xil_types.h>

typedef u8 TargetRecord;

/**
 * Get the IPMC hardware revision. revA returns 0, revB returns 1, etc.
 */
u8 get_ipmc_hw_rev();

/**
 * Returns the EEPROM boot record. Used with functions bellow.
 */
TargetRecord get_eeprom_boot_record();

/**
 * Returns the boot register tag. Used with functions bellow.
 */
u8 get_bootreg_tag();

/**
 * Set the boot tag which doesn't get cleared during resets.
 *  force_boot: Indicates if the goal is to boot the image defined in this tag (and not EEPROM tag).
 *  force_test_image: Indicates the image to boot is 'test', only valid if force_boot is true.
 *  target_image: Indicates the image to boot (0='fallback', 1='A', 2='B'), only valid if force_boot is true.
 */
void set_bootreg_tag(u8 force_boot, u8 force_test_image, u8 target_image);

/**
 * Get the boot info from the bootreg. Return value used in the fuctions below.
 */
TargetRecord get_bootreg_record();

/**
 * Return 1 if the boot record is valid.
 */
u8 is_record_valid(TargetRecord record);

/**
 * Return 0 if the boot tag indicates to force boot.
 */
u8 is_forced_boot();

/**
 * Return non-zero if the test image flag is asserted.
 */
u8 is_test_image(TargetRecord record);

/**
 * Get the boot tag image to boot, valid if is_record_valid() is non-zero.
 * Output will be either be 0 ('fallback'), 1 ('A'), 2 ('B'). Test image isn't taken into account.
 * This is used to decide what to boot if the test image is invalid.
 */
u8 get_regular_boot_target(TargetRecord record);

/**
 * Unlike get_regular_boot_target, this one can also return 3 ('test').
 */
u8 get_final_boot_target(TargetRecord record);

/**
 * Verify an image based on it's image number.
 *  image: Can be 0 ('fallback'), 1 ('A'), 2 ('B') or 3 ('test').
 * Returns XST_SUCCESS if image is valid.
 */
u32 verify_image(u8 image);


#endif /* SRC_IPMC_H_ */
