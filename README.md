# ZYNQ-IPMC

An open-source IPMC hardware and software framework developed by the University of Wisconsin-Madison intended to be used as an IPMI controller in ATCA blades. More detailed information and documentation at the [WIKI](https://github.com/uwcms/IPMC/wiki).

For information about the hardware check its [dedicated repository here](https://github.com/uwcms/ZYNQ-IPMC-HW).

# Getting started

## Prerequirements

- Linux (likely to work on Windows with Cygwin).
- Xilinx Vivado 2018.2 with XSDK installed.

## Clean checkout
1. ```git clone --recursive git@github.com:uwcms/IPMC.git```
2. Open the project: ```vivado Vivado/ipmc_zynq_vivado.xpr```.
3. (optional) Generate Bitstream if it doesn't exist.
4. (if bitstream generated) ```File``` -> ```Export``` -> ```Export Hardware``` (include bitstream checked).
5. Lunch XSDK from Vivado: ```File``` -> ```Launch SDK``` (accept defaults, click OK).
6. In XSDK, go to ```File``` -> ```Import``` -> ```General``` -> ```Existing Projects into Workspace``` -> ```Browse``` (should be the IPMC .sdk folder by default). Click ```Finish```.

Compilation of all projects should start automatically.
In case of issue, regenerate BSP files by right-clicking on ```ipmc_standalone_bsp``` -> ```Re-generate BSP Sources```.

## Using Xilinx Virtual Cable (XVC)
An XVC end-point is always running on the IPMC and is accessible by Ethernet. An host computer will then need to host the Hardware Server which is used to bridge from XVC to Vivado. An Hardware can connect to several XVC end-point and dongles.

First make sure the IPMC is reachable from the machine that will host the Hardware Server, normally the same machine where Vivado is running for convenience. ```network.status``` can be used on the IPMC to check the current IP which can the be used with ```ping```.

Then, source Vivado and run ```hw_server -e 'set auto-open-servers xilinx-xvc:<IPMC-IP-ADDRESS>:2542'``` where ```<IPMC-IP-ADDRESS>``` is the target IPMC address. This will start the Hardware Server on port 3121 (default) and, when connected with Vivado, it will detect the XVC chain transparently as if it was a local dongle.

A different port can be used if multiple Hardware Servers need to run in parallel by adding the argument ```-s TCP::<PORT>``` where ```<PORT>``` is the desired TCP/IP port.

## Programming
### Generating BOOT.bin files
If the IPMC and FSBL projects compiled successfully in XSDK then the boot image is created by doing:
```bash
cd Vivado/ipmc_zynq_vivado.sdk
make
```

### Using FTP
Upload the image to target IPMC using flash_upload.py script, found at `Vivado/ipmc_zynq_vivado.sdk/flash_upload.py` (symlink to common).
```bash
./flash_upload.py -h 192.168.250.243 bootimages/BOOT.bin A
```
The `-h` argument is the IPMC ip address.  The first argument is the file to upload, the second argument is which target image to upload it to.  The default username & password are 'ipmc', 'ipmc'.  See `--help` for additional details.

Filezilla or any other FTP client can also be used. The BOOT.bin needs to be copied to virtual/A.bin (or B.bin, etc) in the IPMC and, if valid, the flash update will start automatically.

The IPMC can then be restarted by using the ```restart``` command. There are mechanisms in place to prevent restarts if there are signs of improper image flashing.

### Using Serial Console
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

# Contributing

No information added.

# Credits

- Marcelo Vicente (marcelo.vicente __AT__ cern.ch) ```[Maintainer]```
- Jesra Tikalsky (jtikalsky __AT__ hep.wisc.edu) ```[Maintainer]```
- Ales Svetek (ales.svetek __AT__ cern.ch)
- Tom Gorski (tgorski __AT__ hep.wisc.edu)

# License
The ZYNQ-IPMC Framework is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

The ZYNQ-IPMC Framework is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
