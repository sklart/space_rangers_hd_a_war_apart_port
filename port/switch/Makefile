DEVKITPRO ?= C:/devkitPro
DEVKITA64 ?= $(DEVKITPRO)/devkitA64
LIBNX ?= $(DEVKITPRO)/libnx
PORTLIBS ?= $(DEVKITPRO)/portlibs/switch

TARGET := SpaceRangersHDAWarApart
BUILD := build
SOURCES := source
INCLUDES := include

CC := $(DEVKITA64)/bin/aarch64-none-elf-gcc
CXX := $(DEVKITA64)/bin/aarch64-none-elf-g++
ELF2NRO := $(DEVKITPRO)/tools/bin/elf2nro

INCLUDE := -I$(LIBNX)/include -I$(PORTLIBS)/include -I$(INCLUDES)
LIBDIRS := -L$(LIBNX)/lib -L$(PORTLIBS)/lib
ARCH := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE
CFLAGS := -g -O2 -Wall -Wextra $(ARCH) -ffunction-sections -fdata-sections $(INCLUDE) -D__SWITCH__
CXXFLAGS := $(CFLAGS) -std=gnu++20
LDFLAGS := -specs=$(LIBNX)/switch.specs -g $(ARCH) -Wl,-Map,$(BUILD)/$(TARGET).map -Wl,--gc-sections
LIBS := -lSDL2 -lEGL -lglapi -ldrm_nouveau -lnx -lpthread

CPPFILES := $(wildcard $(SOURCES)/*.cpp)
OFILES := $(CPPFILES:$(SOURCES)/%.cpp=$(BUILD)/%.o)

.PHONY: all clean
all: $(TARGET).nro

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: $(SOURCES)/%.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/$(TARGET).elf: $(OFILES)
	$(CXX) $(OFILES) $(LIBDIRS) $(LDFLAGS) $(LIBS) -o $@

$(TARGET).nro: $(BUILD)/$(TARGET).elf
	$(ELF2NRO) $< $@ --nacp=SpaceRangersHDAWarApart.nacp

clean:
	rm -rf $(BUILD) $(TARGET).nro
