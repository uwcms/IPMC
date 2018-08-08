.PHONY: check-env

all: debug

debug:
	mkdir -p bootimages
	bootgen -w -image debug.bif -arch zynq -o bootimages/BOOT.bin 
	@echo Use flash_upload.sh or regular FTP to upload the generated .bin to target IPMC.

clean:
	rm -r bootimages

check-env:
ifndef XILINX_VIVADO
   $(error Vivado is not sourced or undefined)
endif

