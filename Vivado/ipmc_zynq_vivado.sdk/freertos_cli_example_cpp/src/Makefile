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
	-I"$(WORKSPACE)/ipmc_standalone_bsp/ps7_cortexa9_0/include" \

export LIBRARY_PATHS = \
	-L"$(WORKSPACE)/ipmc_standalone_bsp/ps7_cortexa9_0/lib" \

export LIBS := -Wl,--start-group,-lxil,-lgcc,-lc,-lstdc++,--end-group

ifneq (x$(GIT_DIRTY),x)
GIT_DEFINE_STATUS := -DGIT_STATUS="$$GIT_STATUS"
else
GIT_DEFINE_STATUS := 
endif

ifeq ($(BUILD_CONFIGURATION),Debug)
# "-Og Optimize debugging experience.  -Og enables optimizations that do not interfere with debugging." from man g++
OPTIMIZE_OPTIONS := -Og -ggdb3
else
# "-Os Optimize for size.  -Os enables all -O2 optimizations that do not typically increase code size." from man g++
OPTIMIZE_OPTIONS := -Os -g0
endif

export CFLAGS = \
	-Wall \
	$(OPTIMIZE_OPTIONS) \
	-fmessage-length=0 \
	-mcpu=cortex-a9 \
	-mfpu=vfpv3 \
	-mfloat-abi=hard \
	-MMD -MP -MF"$(patsubst .obj/$(BUILD_CONFIGURATION)/%,.dep/$(BUILD_CONFIGURATION)/%,$@)" \
	-DGIT_DESCRIBE="\"$(GIT_DESCRIBE)\"" \
	-DGIT_BRANCH="\"$(GIT_BRANCH)\"" \
	$(GIT_DEFINE_STATUS) \

# -MT"$@" 

export CXXFLAGS = \
	-std=c++11 \

export LDFLAGS := \
	-Wl,-build-id=none \
	-specs=Xilinx.spec \
	-Wl,-T -Wl,lscript.ld \

OBJS := \
	$(patsubst ./%.c,.obj/$(BUILD_CONFIGURATION)/%.o,$(shell find -L . -type f -name '*.c')) \
	$(patsubst ./%.cc,.obj/$(BUILD_CONFIGURATION)/%.o,$(shell find -L . -type f -name '*.cc')) \
	$(patsubst ./%.cpp,.obj/$(BUILD_CONFIGURATION)/%.o,$(shell find -L . -type f -name '*.cpp')) \
	$(patsubst ./%.S,.obj/$(BUILD_CONFIGURATION)/%.o,$(shell find -L . -type f -name '*.S')) \

all: doxygen pre-build build

pre-build:
	@echo
	-a9-linaro-pre-build-step

build: bin/$(BUILD_CONFIGURATION)/freertos_cli_example_cpp.elf
   
bin/$(BUILD_CONFIGURATION)/freertos_cli_example_cpp.elf: $(OBJS)
	@echo
	@mkdir -p "$(dir $@)"
	$(CXX) $(CFLAGS) $(LDFLAGS) $(INCLUDE_PATHS) $(LIBRARY_PATHS) -o "$@" $^ $(LIBS)
	arm-none-eabi-size "$@" | tee "$@.size"

.obj/$(BUILD_CONFIGURATION)/%.o: %.c
	@echo
	@mkdir -p "$(dir $@)" "$(dir $(patsubst .obj/$(BUILD_CONFIGURATION)/%,.dep/$(BUILD_CONFIGURATION)/%,$@))"
	$(CC) -c $(CFLAGS) $(LDFLAGS) $(INCLUDE_PATHS) $(LIBRARY_PATHS) -o "$@" "$<" $(LIBS)

.obj/$(BUILD_CONFIGURATION)/%.o: %.cc
	@echo
	@mkdir -p "$(dir $@)" "$(dir $(patsubst .obj/$(BUILD_CONFIGURATION)/%,.dep/$(BUILD_CONFIGURATION)/%,$@))"
	$(CXX) -c $(CFLAGS) $(CXXFLAGS) $(LDFLAGS) $(INCLUDE_PATHS) $(LIBRARY_PATHS) -o "$@" "$<" $(LIBS)

.obj/$(BUILD_CONFIGURATION)/%.o: %.cpp
	@echo
	@mkdir -p "$(dir $@)" "$(dir $(patsubst .obj/$(BUILD_CONFIGURATION)/%,.dep/$(BUILD_CONFIGURATION)/%,$@))"
	$(CXX) -c $(CFLAGS) $(CXXFLAGS) $(LDFLAGS) $(INCLUDE_PATHS) $(LIBRARY_PATHS) -o "$@" "$<" $(LIBS)

.obj/$(BUILD_CONFIGURATION)/%.o: %.S
	@echo
	@mkdir -p "$(dir $@)" "$(dir $(patsubst .obj/$(BUILD_CONFIGURATION)/%,.dep/$(BUILD_CONFIGURATION)/%,$@))"
	$(CXX) -c $(CFLAGS) $(CXXFLAGS) $(LDFLAGS) $(INCLUDE_PATHS) $(LIBRARY_PATHS) -o "$@" "$<" $(LIBS)

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
