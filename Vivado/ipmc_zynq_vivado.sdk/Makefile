ifndef XILINX_VIVADO
$(error Vivado is not sourced or undefined)
endif

all: debug

debug:
	mkdir -p bootimages
	rm -f version.json
	arm-none-eabi-objcopy -O binary --dump-section=.versiondata=/dev/stdout IPMC/src/bin/Debug/IPMC.elf /dev/null | tee version.json | jq -r '.summary'
	bootgen -w -image debug.bif -arch zynq -o bootimages/BOOT.bin
	rm -f version.json
	@echo Use flash_upload.sh or regular FTP to upload the generated .bin to target IPMC.

MACHINE_DATE := $(shell arm-none-eabi-objcopy -O binary --dump-section=.versiondata=/dev/stdout IPMC/src/bin/Debug/IPMC.elf /dev/null | jq -r '.build.machine_date')
GIT_DESCRIBE := $(shell arm-none-eabi-objcopy -O binary --dump-section=.versiondata=/dev/stdout IPMC/src/bin/Debug/IPMC.elf /dev/null | jq -r '.git.describe')
package: IMAGE_ID := ZYNQ-IPMC-$(shell date -d '$(MACHINE_DATE)' +%Y%m%dT%H%M%S)-$(GIT_DESCRIBE)
package: debug
	rm -rf ZYNQ-IPMC-*-*-v*.*.*
	mkdir $(IMAGE_ID)
	cp bootimages/BOOT.bin $(IMAGE_ID)/
	cp IPMC/src/bin/Debug/IPMC.elf $(IMAGE_ID)/
	tar -cjvf $(IMAGE_ID).tbz2 $(IMAGE_ID)

clean:
	rm -rf bootimages
	rm -rf ZYNQ-IPMC-*-*-v*.*.*
