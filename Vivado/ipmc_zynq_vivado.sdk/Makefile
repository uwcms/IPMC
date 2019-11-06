.PHONY: check-env

all: debug

debug:
	mkdir -p bootimages
	rm -f version.txt
	arm-none-eabi-objcopy -O binary --dump-section=.versiondata=/dev/stdout IPMC/src/bin/Debug/IPMC.elf /dev/null | tee version.txt
	bootgen -w -image debug.bif -arch zynq -o bootimages/BOOT.bin 
	rm -f version.txt
	@echo Use flash_upload.sh or regular FTP to upload the generated .bin to target IPMC.

package: IMAGE_ID := ZYNQ-IPMC-$(shell date +%Y%m%dT%H%M%S)-$(shell git describe --always --match='*-v*.*.*' 2>/dev/null)
package: debug
	rm -rf ZYNQ-IPMC-*-*-v*.*.*
	mkdir $(IMAGE_ID)
	cp bootimages/BOOT.bin $(IMAGE_ID)/
	cp IPMC/src/bin/Debug/IPMC.elf $(IMAGE_ID)/
	tar -cjvf $(IMAGE_ID).tbz2 $(IMAGE_ID)

clean:
	rm -rf bootimages
	rm -rf ZYNQ-IPMC-*-*-v*.*.*
