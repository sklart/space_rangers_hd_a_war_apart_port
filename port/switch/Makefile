DEVKITPRO ?= C:/devkitPro
DEVKITA64 ?= $(DEVKITPRO)/devkitA64
LIBNX ?= $(DEVKITPRO)/libnx
PORTLIBS ?= $(DEVKITPRO)/portlibs/switch
# CMake is started from devkitPro MSYS and therefore needs POSIX paths.
DEVKITA64_MSYS := $(shell cygpath -u "$(DEVKITA64)")
PORTLIBS_MSYS := $(shell cygpath -u "$(PORTLIBS)")

TARGET := SpaceRangersHDAWarApart
BUILD := build
SOURCES := source
INCLUDES := include
GAME_CPP_ROOT := ../../upstream/cpp
BUILD_GIT_COMMIT := $(shell git -C ../.. rev-parse --short HEAD)

CC := $(DEVKITA64)/bin/aarch64-none-elf-gcc
CXX := $(DEVKITA64)/bin/aarch64-none-elf-g++
ELF2NRO := $(DEVKITPRO)/tools/bin/elf2nro

INCLUDE := -I$(LIBNX)/include -I$(PORTLIBS)/include -I$(INCLUDES) -Iplatform -I$(GAME_CPP_ROOT)/src -I$(GAME_CPP_ROOT)/runtime -I../../upstream/okgf/include
LIBDIRS := -L$(LIBNX)/lib -L$(PORTLIBS)/lib
ARCH := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE
CFLAGS := -g -O2 -Wall -Wextra $(ARCH) -ffunction-sections -fdata-sections $(INCLUDE) -D__SWITCH__
CXXFLAGS := $(CFLAGS) -std=gnu++20 -DBUILD_GIT_COMMIT=\"$(BUILD_GIT_COMMIT)\"
LDFLAGS := -specs=$(LIBNX)/switch.specs -g $(ARCH) -Wl,-Map,$(BUILD)/$(TARGET).map -Wl,--gc-sections
OKGF_LIB := $(BUILD)/okgf/libokgf.a
OKGF_SOFTFLOAT_LIB := $(BUILD)/okgf/vendor/softfloat/libokgf_softfloat.a
LIBS := $(OKGF_LIB) $(OKGF_SOFTFLOAT_LIB) -lpng -lz -ljpeg -lm -lSDL2 -lEGL -lglapi -ldrm_nouveau -lnx -lpthread

CPPFILES := $(wildcard $(SOURCES)/*.cpp)
OFILES := $(CPPFILES:$(SOURCES)/%.cpp=$(BUILD)/%.o)
PLATFORM_CPPFILES := $(wildcard platform/*.cpp)
PLATFORM_OFILES := $(PLATFORM_CPPFILES:platform/%.cpp=$(BUILD)/platform_%.o)
GAME_OFILES := $(BUILD)/CrcUnit.o $(BUILD)/System.o $(BUILD)/SystemImports.o

.PHONY: all clean host-package-test
all: $(TARGET).nro

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: $(SOURCES)/%.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/platform_%.o: platform/%.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/CrcUnit.o: $(GAME_CPP_ROOT)/src/CrcUnit.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/System.o: $(GAME_CPP_ROOT)/src/System.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/SystemImports.o: $(GAME_CPP_ROOT)/src/SystemImports.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OKGF_LIB):
	cmake -S ../../upstream/okgf -B $(BUILD)/okgf -G "Unix Makefiles" -DCMAKE_SYSTEM_NAME=Generic -DCMAKE_C_COMPILER=$(DEVKITA64_MSYS)/bin/aarch64-none-elf-gcc -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY -DCMAKE_FIND_ROOT_PATH=$(PORTLIBS_MSYS) -DCMAKE_PREFIX_PATH=$(PORTLIBS_MSYS) -DOKGF_MATH_BACKEND=COMPATIBLE -DOKGF_PORTABLE_MATH=ON
	cmake --build $(BUILD)/okgf --parallel 2

$(BUILD)/$(TARGET).elf: $(OFILES) $(PLATFORM_OFILES) $(GAME_OFILES) $(OKGF_LIB)
	$(CXX) $(OFILES) $(PLATFORM_OFILES) $(GAME_OFILES) $(LIBDIRS) $(LDFLAGS) $(LIBS) -o $@

$(TARGET).nro: $(BUILD)/$(TARGET).elf
	$(ELF2NRO) $< $@ --nacp=SpaceRangersHDAWarApart.nacp

clean:
	rm -rf $(BUILD) $(TARGET).nro

host-package-test: | $(BUILD)
	g++ -std=c++20 -Wall -Wextra -Iplatform ../../tests/test_package_probe.cpp platform/filesystem.cpp -o $(BUILD)/test_package_probe
	$(BUILD)/test_package_probe "../../windows/Space Rangers HD A War Apart"

host-package-tree-test: | $(BUILD)
	g++ -std=c++20 -Wall -Wextra -Iplatform ../../tests/test_package_tree.cpp platform/package.cpp -o $(BUILD)/test_package_tree
	$(BUILD)/test_package_tree "../../windows/Space Rangers HD A War Apart"

host-package-payload-test:
	python ../../tests/test_package_payload.py "../../windows/Space Rangers HD A War Apart"

host-corrupt-package-test: | $(BUILD)
	g++ -std=c++20 -Wall -Wextra -Iplatform ../../tests/test_corrupt_package.cpp platform/package.cpp -o $(BUILD)/test_corrupt_package
	$(BUILD)/test_corrupt_package
