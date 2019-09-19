.PHONY: check-env

all: debug

debug:
	mkdir -p bootimages
	rm -f version.txt
	arm-none-eabi-objcopy -O binary --dump-section=.versiondata=/dev/stdout IPMC/src/bin/Debug/IPMC.elf /dev/null | tee version.txt
	bootgen -w -image debug.bif -arch zynq -o bootimages/BOOT.bin 
	rm -f version.txt
	@echo Use flash_upload.sh or regular FTP to upload the generated .bin to target IPMC.

package: debug
	tar -cjvf ZYNQ-IPMC-$(shell date +%Y%m%dT%H%M%S)-$(shell git describe --always --match='*-v*.*.*' 2>/dev/null).tbz2 bootimages/BOOT.bin IPMC/src/bin

clean:
	rm -r bootimages
	rm -f ZYNQ-IPMC-*.tbz2                                                                                                                                


check-env:
ifndef XILINX_VIVADO
   $(error Vivado is not sourced or undefined)
endif

