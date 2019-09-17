.PHONY: check-env

all: debug

debug:
	mkdir -p bootimages
	rm -f version.txt
	arm-none-eabi-objcopy -O binary --dump-section=.versiondata=/dev/stdout IPMC/src/bin/Debug/IPMC.elf /dev/null | tee version.txt
	bootgen -w -image debug.bif -arch zynq -o bootimages/BOOT.bin 
	rm -f version.txt
	@echo Use flash_upload.sh or regular FTP to upload the generated .bin to target IPMC.

clean:
	rm -r bootimages

check-env:
ifndef XILINX_VIVADO
   $(error Vivado is not sourced or undefined)
endif

