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

## FTP image upload and programming
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

## Programming via Serial Console
Firmware images can be uploaded to the IPMC via Serial Console as when FTP is not available. This can be done using the ```upload``` command.

Make sure all the necessary tools are installed on Linux:
```bash
sudo yum install screen base64 sha256sum tr cut
```
Assume that ```file.bin``` is the binary file that will get uploaded and ```virtual/file.bin``` is the target virtual file on the IPMC. As a side example, the IPMC flash resides in ```virtual/flash.bin```. Start by exporting both variables by doing:
```bash
export BINFILE=file.bin
export IPMCTARGET=virtual/file.bin
```
Then generate the necessary items to execute the upload:
```bash
base64 $BINFILE | tr -d '\n' > /tmp/screen-exchange
export FILEHASH=`sha256sum /tmp/screen-exchange | cut -d " " -f 1`
export FILESIZE=`du -b /tmp/screen-exchange | cut -f 1`
echo "upload $IPMCTARGET $FILESIZE $FILEHASH"
```
The last printed line will be the command that will need to be run on the Serial Console using ```screen```.

Assume that ```/dev/ttyUSB0``` is the USB interface where the IPMC connection resides. Start the ```screen``` session by doing:
```bash
screen /dev/ttyUSB0
```
With screen connected to the serial interface do the following: ```Ctrl+A``` followed by ```:readbuf```. This will read the file to screen's memory buffer.

Still on the ```screen``` session, copy the upload line printed by ```echo```. If this was successful you should see a prompt indicating that the IPMC is waiting for a file to be uploaded:
```
Reading incoming serial stream for 10 seconds..
```
The time that the serial console will be waiting for data will depend on the total file size. There is a default 5 second grace period to allow the user to start the upload.

The upload is started by doing ```Ctrl+A``` followed by ```]``` on the ```screen``` session. No feedback will be provided but the IPMC will automatically reply after all bytes are read or when the total time allowed is reached.

The upload status will be reported accordingly.
