DEVKITPRO ?= C:/devkitPro
DEVKITA64 ?= $(DEVKITPRO)/devkitA64
LIBNX ?= $(DEVKITPRO)/libnx
PORTLIBS ?= $(DEVKITPRO)/portlibs/switch

TARGET := SpaceRangersHDAWarApart
BUILD := build
SOURCES := source
INCLUDES := include
GAME_CPP_ROOT := ../../upstream/cpp

CC := $(DEVKITA64)/bin/aarch64-none-elf-gcc
CXX := $(DEVKITA64)/bin/aarch64-none-elf-g++
ELF2NRO := $(DEVKITPRO)/tools/bin/elf2nro

INCLUDE := -I$(LIBNX)/include -I$(PORTLIBS)/include -I$(INCLUDES) -Iplatform -I$(GAME_CPP_ROOT)/src -I$(GAME_CPP_ROOT)/runtime
LIBDIRS := -L$(LIBNX)/lib -L$(PORTLIBS)/lib
ARCH := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE
CFLAGS := -g -O2 -Wall -Wextra $(ARCH) -ffunction-sections -fdata-sections $(INCLUDE) -D__SWITCH__
CXXFLAGS := $(CFLAGS) -std=gnu++20
LDFLAGS := -specs=$(LIBNX)/switch.specs -g $(ARCH) -Wl,-Map,$(BUILD)/$(TARGET).map -Wl,--gc-sections
LIBS := -lSDL2 -lEGL -lglapi -ldrm_nouveau -lnx -lpthread

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

$(BUILD)/$(TARGET).elf: $(OFILES) $(PLATFORM_OFILES) $(GAME_OFILES)
	$(CXX) $(OFILES) $(PLATFORM_OFILES) $(GAME_OFILES) $(LIBDIRS) $(LDFLAGS) $(LIBS) -o $@

$(TARGET).nro: $(BUILD)/$(TARGET).elf
	$(ELF2NRO) $< $@ --nacp=SpaceRangersHDAWarApart.nacp

clean:
	rm -rf $(BUILD) $(TARGET).nro

host-package-test:
	g++ -std=c++20 -Wall -Wextra -Iplatform ../../tests/test_package_probe.cpp platform/filesystem.cpp -o $(BUILD)/test_package_probe
	$(BUILD)/test_package_probe "../../windows/Space Rangers HD A War Apart"
