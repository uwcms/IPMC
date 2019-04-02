# ZYNQ-IPMC - Open-source IPMC hardware and software framework
University of Wisconsin IPMC

# This branch is the fallback and test image!

For complete instructions check the main branch.

## Test setup procedure:
1. Use the IPMC testboard with two IPMC: (1) The test IPMC and (2) the control IPMC.
2. Make sure the fallback firmware is running on both cards.
3. Make sure that /dev/ttyUSB0 is the test IPMC and /dev/ttyUSB1 is the control IPMC.
4. Make sure there are jumpers wiring the test IPMC XVC to the control IPMC JTAG input port.

## Testing flow (for test IPMC):
1. Do an ohmmeter test.
2. Program the IPMC using the JTAG dongle and XSDK.
3. Wait for the network interface to come up and program the flash.
4. At the same time the flash is programming check if XVC can see the control IPMC.
5. If the card needs to be tagged then check the MAC address using network.status.
6. Run test-suite.py and check the final report for potential connectivity issues.
