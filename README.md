# Custom FSBL for the ZYNQ-IPMC

This is a standard Xilinx Zynq-7000 FSBL modified to support multiple images on
the ZYNQ-IPMC platform.

On a ZYNQ-IPMC revB the QSPI flash has 4 partiations of 16MB each as follows:


| Low Address | High Address | Image |
| ----------- | ------------ | ----- |
| 0 MB | 16 MB | Fallback |
| 16 MB | 32 MB | Image A |
| 32 MB | 48 MB | Image B |
| 48 MB | 64 MB | Test |

The target image to boot can be set in two different places:
 - In byte 2 of the MAC EEPROM.
 - In bits [27:24] of the REBOOT register of the Zynq (persistent through resets).

By default, the image defined in the MAC EEPROM is the one that will get booted.
If bit 27 of the REBOOT register is set to ```1``` then this is considered as
'forced boot' and the image defined in the REBOOT register has priority.

The target image is set as the 3 lowest bits of the MAC EEPROM or
bits [26:24] of the reboot register with the following format:

 - Bits [1:0]: Determines the boot image.  It can take values 0 (fallback), 1 (A) or 2 (B).
 - Bits [2]: If set to ```1``` then the test image is the one that will boot.
 - Bits [3]: If set to ```1``` then the software 'primary' image is B instead of A. (ignored by FSBL)

If the test image fails to boot (e.g. image is corrupted), then it will be the
'regular' defined image that will get boot.

The allowed booting order is:
 - test -> A/B -> fallback
 - A/B -> fallback

## Known issues

1. If a reboot takes place and the current QSPI flash page corrupt then the IPMC
will fail to boot. There is no solution to this besides resetting the page back
to zero after programming or booting.
