#!/bin/sh
## Written by mpv

## Usage: ./<script_name>.sh [IPMC IP address] [Username] [Password] <BOOT.BIN location>
##    e.g.: ./flash_upload.sh 192.168.250.243 ipmc ipmc

if ! [ -x "$(command -v ftp)" ]; then
	echo 'Error: ftp is not installed.' >&2
	exit 1
fi

if [ $# -lt 3 ]; then
	echo "Error: Not enough arguments, usage: ./$0 address username password" >&2
	exit 1
fi

## Default configuration that can be overwritten by arguments
HOST=$1
USER=$2
PASSWD=$3
FILE='bootimages/BOOT.bin'

if [ "$4" != "" ]; then
	FILE=$4
fi 

## Check if file exists
if [ ! -f $FILE ]; then
	echo "Error: $FILE does not exist." >&2
	exit 1
fi

## Do the actual programming via FTP
ftp -n $HOST <<END_SCRIPT
quote USER $USER
quote PASS $PASSWD
bin
send $FILE virtual/flash.bin
quit
END_SCRIPT

exit 0
