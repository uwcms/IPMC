# ZYNQ-IPMC - Open-source IPMC hardware and software framework
University of Wisconsin IPMC

## Instructions for a clean checkout and SDK:
1. git clone git@github.com:uwcms/IPMC.git
2. Open the Vivado 2017.2 project (vivado Vivado/ipmc_zynq_vivado.xpr)
3. (optional) Generate Bitstream
4. (if bitstream generated) File -> Export -> Export Hardware (include bitstream checked)
5. File -> Launch SDK (accept defaults, click OK)
6. In SDK, File -> Import -> General -> Existing Projects into Workspace -> Browse (should be the IPMC .sdk folder by default). Click Finish.

Compilation of all projects should start automatically.
In case of issue, regenerate BSP files by right click on ipmc_standalone_bsp -> Re-generate BSP Sources.

## Flash boot image and programming
If the IPMC and FSBL projects compiled successfully in XSDK then the boot image is created by doing:
```bash
cd Vivado/ipmc_zynq_vivado.sdk
make
```
Upload the image to target IPMC using flash_upload.sh bash script, e.g.:
```bash
./flash_upload.sh 192.168.250.243 ipmc ipmc
```
The first argument is the IPMC address and then the FTP username and password.
Filezilla or any other FTP client can also be used. The BOOT.bin needs to be copied to virtual/flash.bin in the IPMC and, if valid, the flash update will start automatically.
The IPMC can then be restarted by using the ```restart``` command.
