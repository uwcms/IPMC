ifeq (x$(BUILD_CONFIGURATION),x)
$(error Please specify a BUILD_CONFIGURATION)
endif

export GIT_DESCRIBE := $(shell git describe --tags 2>/dev/null || git rev-parse --short HEAD)
export GIT_BRANCH   := $(shell git rev-parse --abbrev-ref HEAD)
export GIT_DIRTY    := $(shell git diff-index --quiet HEAD || echo 'dirty')
export GIT_STATUS   := "$(shell git status --porcelain | perl -pe 's/\n/\\n/g;s/"/\\"/g;s/'\''/'\'\\\'\''/g')"
export PROJECT_ROOT := $(realpath ..)
export PROJECT_NAME := $(notdir $(PROJECT_ROOT))

export CC := arm-none-eabi-gcc
export CXX := arm-none-eabi-g++ 

export WORKSPACE ?= ../..

export INCLUDE_PATHS = \
	-I"$(WORKSPACE)/freertos_cli_example_cpp/src/common/uw-ipmc" \
	-I"$(WORKSPACE)/freertos_cli_example_cpp/src/config" \
	-I"$(WORKSPACE)/freertos_cli_example_cpp/src/components" \
	-I"$(WORKSPACE)/freertos_cli_example_cpp/src/common/lwIP/src/contrib/ports/xilinx/include" \
	-I"$(WORKSPACE)/freertos_cli_example_cpp/src/common/lwIP/src/lwip-1.4.1/src/include/ipv4" \
	-I"$(WORKSPACE)/freertos_cli_example_cpp/src/common/lwIP/src/lwip-1.4.1/src/include" \
	-I"$(WORKSPACE)/freertos_cli_example_cpp/src/common/FreeRTOS/portable/GCC/ARM_CA9" \
	-I"$(WORKSPACE)/freertos_cli_example_cpp/src/common/FreeRTOS/include" \
	-I"$(WORKSPACE)/freertos_cli_example_cpp/src/common/xilisf/include" \
	-I"$(WORKSPACE)/ipmc_standalone_bsp/ps7_cortexa9_0/include" \

export FUNCTION_WRAPPERS = \
	-Wl,-wrap=malloc \
	-Wl,-wrap=free \
	-Wl,-wrap=calloc \
	-Wl,-wrap=realloc \
	-Wl,-wrap=printf \
	-Wl,-wrap=sprintf \
	-Wl,-wrap=snprintf \
	-Wl,-wrap=vprintf \
	-Wl,-wrap=vsprintf \
	-Wl,-wrap=vsnprintf \
	-Wl,-wrap=xil_printf \
	-Wl,-wrap=print \
	-Wl,-wrap=sleep \
	-Wl,-wrap=sha_256 \

export LIBRARY_PATHS = \
	-L"$(WORKSPACE)/ipmc_standalone_bsp/ps7_cortexa9_0/lib" \

export LIBS := -lrsa -Wl,--start-group,-lxil,-lgcc,-lc,-lstdc++,--end-group

ifeq ($(BUILD_CONFIGURATION),Debug)
# "-Og Optimize debugging experience.  -Og enables optimizations that do not interfere with debugging." from man g++
OPTIMIZE_OPTIONS := -O0 -ggdb3
#-flto -fuse-linker-plugin -fno-fat-lto-objects
else
# "-Os Optimize for size.  -Os enables all -O2 optimizations that do not typically increase code size." from man g++
OPTIMIZE_OPTIONS := -Os -g0
#-flto -fuse-linker-plugin -fno-fat-lto-objects
endif

export CFLAGS = \
	$(OPTIMIZE_OPTIONS) \
	-fmessage-length=0 \
	-mcpu=cortex-a9 \
	-mfpu=vfpv3 \
	-mfloat-abi=hard \
	-MMD -MP -MF"$(patsubst .obj/$(BUILD_CONFIGURATION)/%,.dep/$(BUILD_CONFIGURATION)/%,$@)" \
	-D XPAR_XISF_FLASH_FAMILY=5 -D XPAR_XISF_INTERFACE_PSQSPI \

# -MT"$@" 

export CXXFLAGS = \
	-std=c++11 \

export LDFLAGS := \
	-Wl,-build-id=none \
	-specs=Xilinx.spec \
	-Wl,-T -Wl,lscript.ld \
	$(FUNCTION_WRAPPERS) \

export WARNING_FLAGS = -Wall

OBJS := \
	$(patsubst ./%.c,.obj/$(BUILD_CONFIGURATION)/%.o,$(shell find -L . -type f -name '*.c')) \
	$(patsubst ./%.c,.obj/$(BUILD_CONFIGURATION)/%.o,$(shell find -L . -type f -name '*.c')) \
	$(patsubst ./%.cc,.obj/$(BUILD_CONFIGURATION)/%.o,$(shell find -L . -type f -name '*.cc')) \
	$(patsubst ./%.cpp,.obj/$(BUILD_CONFIGURATION)/%.o,$(shell find -L . -type f -name '*.cpp')) \
	$(patsubst ./%.S,.obj/$(BUILD_CONFIGURATION)/%.o,$(shell find -L . -type f -name '*.S')) \

all: doxygen build

pre-build: .dep/.lwip-patch
	-a9-linaro-pre-build-step

.dep/.lwip-patch:
	@mkdir -p .dep/
	touch "$@"
	find common/lwIP/src/contrib/ports/xilinx -type f -exec sed -i -re 's/#include .xil_printf\.h.//;s/xil_printf/ipmc_lwip_printf/g;s/\<printf/ipmc_lwip_printf/g;s/\<print\(/ipmc_lwip_printf("%s",/g' '{}' +
	sed -i -r -e '/^#undef /d;s/(#define\s+)(E\w+)(\s)/#undef \2\n\1\2\3/' common/lwIP/src/lwip-1.4.1/src/include/lwip/arch.h
	find common/lwIP/src -type f -name '*.h' -exec sed -i -e 's!"U16_F!" U16_F!g;s!U16_F"!U16_F "!g' '{}' +

build: pre-build bin/$(BUILD_CONFIGURATION)/freertos_cli_example_cpp.elf

bin/$(BUILD_CONFIGURATION)/freertos_cli_example_cpp.elf: $(OBJS) | pre-build
	@mkdir -p "$(dir $@)"
	$(CXX) $(CFLAGS) $(LDFLAGS) $(INCLUDE_PATHS) $(LIBRARY_PATHS) -o "$@" $^ $(LIBS)
	arm-none-eabi-size "$@" | tee "$@.size"

.obj/$(BUILD_CONFIGURATION)/%.o: %.c | pre-build
	@mkdir -p "$(dir $@)" "$(dir $(patsubst .obj/$(BUILD_CONFIGURATION)/%,.dep/$(BUILD_CONFIGURATION)/%,$@))"
ifeq "common/lwIP/src/" "$(patsubst $@,common/lwIP/src/%,common/lwIP/src/)"
	$(CC) -c $(CFLAGS) $(LDFLAGS) $(INCLUDE_PATHS) $(LIBRARY_PATHS) -o "$@" "$<" $(LIBS)
else
	$(CC) -c $(CFLAGS) $(WARNING_FLAGS) $(LDFLAGS) $(INCLUDE_PATHS) $(LIBRARY_PATHS) -o "$@" "$<" $(LIBS)
endif

.obj/$(BUILD_CONFIGURATION)/%.o: %.cc | pre-build
	@mkdir -p "$(dir $@)" "$(dir $(patsubst .obj/$(BUILD_CONFIGURATION)/%,.dep/$(BUILD_CONFIGURATION)/%,$@))"
ifeq "common/lwIP/src/" "$(patsubst $@,common/lwIP/src/%,common/lwIP/src/)"
	$(CXX) -c $(CFLAGS) $(CXXFLAGS) $(LDFLAGS) $(INCLUDE_PATHS) $(LIBRARY_PATHS) -o "$@" "$<" $(LIBS)
else
	$(CXX) -c $(CFLAGS) $(WARNING_FLAGS) $(CXXFLAGS) $(LDFLAGS) $(INCLUDE_PATHS) $(LIBRARY_PATHS) -o "$@" "$<" $(LIBS)
endif

.obj/$(BUILD_CONFIGURATION)/common/uw-ipmc/version.o: common/uw-ipmc/version.cc | pre-build
	@mkdir -p "$(dir $@)" "$(dir $(patsubst .obj/$(BUILD_CONFIGURATION)/%,.dep/$(BUILD_CONFIGURATION)/%,$@))"
	$(CXX) -c $(CFLAGS) $(WARNING_FLAGS) $(CXXFLAGS) $(LDFLAGS) $(INCLUDE_PATHS) $(LIBRARY_PATHS) \
		-DD_GIT_SHORT_INT="0x$(shell git rev-parse --short HEAD)" \
		-DD_GIT_SHORT="\"$(shell git rev-parse --short HEAD)\"" \
		-DD_GIT_LONG="\"$(shell git rev-parse HEAD)\"" \
		-DD_GIT_DESCRIBE="\"$$GIT_DESCRIBE\"" \
		-DD_GIT_BRANCH="\"$$GIT_BRANCH\"" \
		-DD_GIT_STATUS="$$GIT_STATUS" \
		-DD_COMPILE_DATE="\"$(shell date)\"" \
		-DD_COMPILE_HOST="\"$(shell getent passwd `id -u` | cut -d: -f1)@$(shell hostname --fqdn)\"" \
		-o "$@" "$<" $(LIBS)
.PHONY: .obj/$(BUILD_CONFIGURATION)/common/uw-ipmc/version.o

.obj/$(BUILD_CONFIGURATION)/%.o: %.cpp | pre-build
	@mkdir -p "$(dir $@)" "$(dir $(patsubst .obj/$(BUILD_CONFIGURATION)/%,.dep/$(BUILD_CONFIGURATION)/%,$@))"
	$(CXX) -c $(CFLAGS) $(WARNING_FLAGS) $(CXXFLAGS) $(LDFLAGS) $(INCLUDE_PATHS) $(LIBRARY_PATHS) -o "$@" "$<" $(LIBS)

.obj/$(BUILD_CONFIGURATION)/%.o: %.S | pre-build
	@mkdir -p "$(dir $@)" "$(dir $(patsubst .obj/$(BUILD_CONFIGURATION)/%,.dep/$(BUILD_CONFIGURATION)/%,$@))"
	$(CXX) -c $(CFLAGS) $(WARNING_FLAGS) $(CXXFLAGS) $(LDFLAGS) $(INCLUDE_PATHS) $(LIBRARY_PATHS) -o "$@" "$<" $(LIBS)

ifeq (x$(GIT_DIRTY),x)
export DOXYGEN_GIT_DIRTY :=
else
export DOXYGEN_GIT_DIRTY := (dirty)
endif
doxygen: doxygen.conf $(shell find -L . -type f \( -name '*.c' -o -name '*.cc' -o -name '*.cpp' -o -name '*.S' -o -name '*.h' \) )
	doxygen doxygen.conf

clean:
	rm -rf .dep/$(BUILD_CONFIGURATION)/ .obj/$(BUILD_CONFIGURATION)/ bin/$(BUILD_CONFIGURATION) doxygen/ Xilinx.spec

.PHONY: build pre-build all clean

.SUFFIXES:

-include $(shell find .dep/$(BUILD_CONFIGURATION)/ -type f 2>/dev/null)
