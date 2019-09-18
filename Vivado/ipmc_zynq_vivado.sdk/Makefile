ifndef XILINX_VIVADO
$(error Vivado is not sourced or undefined)
endif

all: debug

debug:
	mkdir -p bootimages
	bootgen -w -image debug.bif -arch zynq -o bootimages/BOOT.bin 
	@echo Use flash_upload.sh or regular FTP to upload the generated .bin to target IPMC.

package: debug
	tar -cjvf ZYNQ-IPMC-$(shell date +%Y%m%dT%H%M%S)-$(shell git describe --tags 2>/dev/null || git rev-parse --short HEAD).tbz2 bootimages/BOOT.bin IPMC/src/bin

clean:
	rm -r bootimages
	rm -f ZYNQ-IPMC-*.tbz2
