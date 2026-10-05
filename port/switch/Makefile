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
HOST_EC_PATCH_FILE := $(abspath platform/msys-runtime-text.patch)

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
GAME_OFILES := $(BUILD)/CrcUnit.o $(BUILD)/System.o $(BUILD)/SystemImports.o $(BUILD)/EC_File.o $(BUILD)/EC_Struct.o $(BUILD)/EC_Buf.o $(BUILD)/EC_BlockPar.o $(BUILD)/EC_Str.o $(BUILD)/EC_Mem.o $(BUILD)/aPacket.o $(BUILD)/MessageText.o $(BUILD)/GR_Main.o $(BUILD)/runtime.o

.PHONY: all clean host-package-test host-package-cycle-test host-ec-file-test host-ec-file-synthetic-test host-blockpar-synthetic-test host-configured-package-test host-gr-main-no-mod-test host-gr-main-config-test host-gr-main-release-config-test host-platform-startup-test host-milestone4-regression host-milestone5-regression host-milestone6-regression host-milestone7-regression
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

$(BUILD)/EC_File.o: $(GAME_CPP_ROOT)/src/EC_File.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/EC_Struct.o: $(GAME_CPP_ROOT)/src/EC_Struct.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/EC_Buf.o: $(GAME_CPP_ROOT)/src/EC_Buf.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/EC_BlockPar.o: $(GAME_CPP_ROOT)/src/EC_BlockPar.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/EC_Str.o: $(GAME_CPP_ROOT)/src/EC_Str.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/EC_Mem.o: $(GAME_CPP_ROOT)/src/EC_Mem.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/aPacket.o: $(GAME_CPP_ROOT)/src/aPacket.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/MessageText.o: $(GAME_CPP_ROOT)/src/MessageText.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/GR_Main.o: $(GAME_CPP_ROOT)/src/GR_Main.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/runtime.o: $(GAME_CPP_ROOT)/runtime/runtime.cpp | $(BUILD)
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
	g++ -std=c++20 -Wall -Wextra -Iplatform ../../tests/test_package_tree.cpp platform/package.cpp -lz -o $(BUILD)/test_package_tree
	$(BUILD)/test_package_tree "../../windows/Space Rangers HD A War Apart"

host-package-payload-test:
	python ../../tests/test_package_payload.py "../../windows/Space Rangers HD A War Apart"

host-package-payload-cpp-test: | $(BUILD)
	g++ -std=c++20 -Wall -Wextra -Iplatform ../../tests/test_package_payload_cpp.cpp platform/package.cpp -lz -o $(BUILD)/test_package_payload_cpp
	$(BUILD)/test_package_payload_cpp "../../windows/Space Rangers HD A War Apart"

host-zl02-synthetic-test: | $(BUILD)
	g++ -std=c++20 -Wall -Wextra -Iplatform ../../tests/test_zl02_synthetic.cpp platform/package.cpp -lz -o $(BUILD)/test_zl02_synthetic
	$(BUILD)/test_zl02_synthetic

host-release-regression: host-package-test host-package-tree-test host-package-payload-cpp-test host-package-payload-test host-corrupt-package-test host-package-cycle-test host-zl02-synthetic-test
	@echo RELEASE REGRESSION PASS

host-milestone4-regression: host-release-regression host-ec-file-synthetic-test host-ec-file-test
	@echo MILESTONE 4 HOST REGRESSION PASS

host-milestone5-regression: host-milestone4-regression host-blockpar-synthetic-test host-configured-package-test
	@echo MILESTONE 5 HOST REGRESSION PASS

host-milestone6-regression: host-milestone5-regression host-gr-main-no-mod-test host-gr-main-config-test host-gr-main-release-config-test
	@echo MILESTONE 6 HOST REGRESSION PASS

host-milestone7-regression: host-milestone6-regression host-platform-startup-test
	@echo MILESTONE 7 HOST REGRESSION PASS

host-corrupt-package-test: | $(BUILD)
	g++ -std=c++20 -Wall -Wextra -Iplatform ../../tests/test_corrupt_package.cpp platform/package.cpp -lz -o $(BUILD)/test_corrupt_package
	$(BUILD)/test_corrupt_package

host-ec-file-test: | $(BUILD)
	@patched=0; if test "$$(uname -o)" = Msys; then git -C $(GAME_CPP_ROOT) apply $(HOST_EC_PATCH_FILE) || exit $$?; patched=1; fi; g++ -std=c++20 -Wall -Wextra -Wpedantic -Iplatform -I$(GAME_CPP_ROOT)/src -I$(GAME_CPP_ROOT)/runtime ../../tests/test_ec_file.cpp platform/package.cpp platform/game_path.cpp platform/ec_file_adapter.cpp platform/windows_imports_portable.cpp $(GAME_CPP_ROOT)/src/EC_File.cpp $(GAME_CPP_ROOT)/src/EC_Struct.cpp $(GAME_CPP_ROOT)/runtime/runtime.cpp -lz -o $(BUILD)/test_ec_file; result=$$?; if test $$result -eq 0; then $(BUILD)/test_ec_file "../../windows/Space Rangers HD A War Apart"; result=$$?; fi; if test $$patched -eq 1; then git -C $(GAME_CPP_ROOT) apply -R $(HOST_EC_PATCH_FILE) || exit $$?; fi; exit $$result

host-ec-file-synthetic-test: | $(BUILD)
	@patched=0; if test "$$(uname -o)" = Msys; then git -C $(GAME_CPP_ROOT) apply $(HOST_EC_PATCH_FILE) || exit $$?; patched=1; fi; g++ -std=c++20 -Wall -Wextra -Wpedantic -Iplatform -I$(GAME_CPP_ROOT)/src -I$(GAME_CPP_ROOT)/runtime ../../tests/test_ec_file_synthetic.cpp platform/package.cpp platform/game_path.cpp platform/ec_file_adapter.cpp platform/windows_imports_portable.cpp $(GAME_CPP_ROOT)/src/EC_File.cpp $(GAME_CPP_ROOT)/src/EC_Struct.cpp $(GAME_CPP_ROOT)/runtime/runtime.cpp -lz -o $(BUILD)/test_ec_file_synthetic; result=$$?; if test $$result -eq 0; then $(BUILD)/test_ec_file_synthetic; result=$$?; fi; if test $$patched -eq 1; then git -C $(GAME_CPP_ROOT) apply -R $(HOST_EC_PATCH_FILE) || exit $$?; fi; exit $$result

host-blockpar-synthetic-test: | $(BUILD)
	@g++ -std=c++20 -Wall -Wextra -Wpedantic -ffunction-sections -fdata-sections -Iplatform -I$(GAME_CPP_ROOT)/src -I$(GAME_CPP_ROOT)/runtime ../../tests/test_blockpar_synthetic.cpp platform/package.cpp platform/game_path.cpp platform/ec_file_adapter.cpp platform/windows_imports_portable.cpp platform/sysutils_imports_portable.cpp platform/runtime_failure_hooks_portable.cpp $(GAME_CPP_ROOT)/src/CrcUnit.cpp $(GAME_CPP_ROOT)/src/BlockParException.cpp $(GAME_CPP_ROOT)/src/EC_File.cpp $(GAME_CPP_ROOT)/src/EC_Struct.cpp $(GAME_CPP_ROOT)/src/EC_Buf.cpp $(GAME_CPP_ROOT)/src/EC_BlockPar.cpp $(GAME_CPP_ROOT)/src/EC_Str.cpp $(GAME_CPP_ROOT)/src/EC_Mem.cpp $(GAME_CPP_ROOT)/src/aPacket.cpp $(GAME_CPP_ROOT)/src/GR_Main.cpp $(GAME_CPP_ROOT)/runtime/runtime.cpp -Wl,--gc-sections -lz -o $(BUILD)/test_blockpar_synthetic && $(BUILD)/test_blockpar_synthetic

host-configured-package-test: | $(BUILD)
	@g++ -std=c++20 -Wall -Wextra -Wpedantic -ffunction-sections -fdata-sections -Iplatform -I$(GAME_CPP_ROOT)/src -I$(GAME_CPP_ROOT)/runtime ../../tests/test_configured_packages.cpp platform/package.cpp platform/game_path.cpp platform/ec_file_adapter.cpp platform/windows_imports_portable.cpp platform/sysutils_imports_portable.cpp platform/runtime_failure_hooks_portable.cpp $(GAME_CPP_ROOT)/src/CrcUnit.cpp $(GAME_CPP_ROOT)/src/BlockParException.cpp $(GAME_CPP_ROOT)/src/EC_File.cpp $(GAME_CPP_ROOT)/src/EC_Struct.cpp $(GAME_CPP_ROOT)/src/EC_Buf.cpp $(GAME_CPP_ROOT)/src/EC_BlockPar.cpp $(GAME_CPP_ROOT)/src/EC_Str.cpp $(GAME_CPP_ROOT)/src/EC_Mem.cpp $(GAME_CPP_ROOT)/src/aPacket.cpp $(GAME_CPP_ROOT)/src/GR_Main.cpp $(GAME_CPP_ROOT)/runtime/runtime.cpp -Wl,--gc-sections -lz -o $(BUILD)/test_configured_packages && $(BUILD)/test_configured_packages "../../windows/Space Rangers HD A War Apart"

host-gr-main-no-mod-test: | $(BUILD)
	@g++ -std=c++20 -Wall -Wextra -Wpedantic -ffunction-sections -fdata-sections -Iplatform -I$(GAME_CPP_ROOT)/src -I$(GAME_CPP_ROOT)/runtime ../../tests/test_gr_main_no_mod_startup.cpp platform/package.cpp platform/game_path.cpp platform/ec_file_adapter.cpp platform/windows_imports_portable.cpp platform/sysutils_imports_portable.cpp platform/runtime_failure_hooks_portable.cpp $(GAME_CPP_ROOT)/src/CrcUnit.cpp $(GAME_CPP_ROOT)/src/BlockParException.cpp $(GAME_CPP_ROOT)/src/EC_File.cpp $(GAME_CPP_ROOT)/src/EC_Struct.cpp $(GAME_CPP_ROOT)/src/EC_Buf.cpp $(GAME_CPP_ROOT)/src/EC_BlockPar.cpp $(GAME_CPP_ROOT)/src/EC_Str.cpp $(GAME_CPP_ROOT)/src/EC_Mem.cpp $(GAME_CPP_ROOT)/src/aPacket.cpp $(GAME_CPP_ROOT)/src/GR_Main.cpp $(GAME_CPP_ROOT)/runtime/runtime.cpp -Wl,--gc-sections -lz -o $(BUILD)/test_gr_main_no_mod_startup && $(BUILD)/test_gr_main_no_mod_startup

host-gr-main-config-test: | $(BUILD)
	@g++ -std=c++20 -Wall -Wextra -Wpedantic -ffunction-sections -fdata-sections -Iplatform -I$(GAME_CPP_ROOT)/src -I$(GAME_CPP_ROOT)/runtime ../../tests/test_gr_main_config_startup.cpp platform/package.cpp platform/game_path.cpp platform/ec_file_adapter.cpp platform/windows_imports_portable.cpp platform/sysutils_imports_portable.cpp platform/runtime_failure_hooks_portable.cpp $(GAME_CPP_ROOT)/src/CrcUnit.cpp $(GAME_CPP_ROOT)/src/BlockParException.cpp $(GAME_CPP_ROOT)/src/EC_File.cpp $(GAME_CPP_ROOT)/src/EC_Struct.cpp $(GAME_CPP_ROOT)/src/EC_Buf.cpp $(GAME_CPP_ROOT)/src/EC_BlockPar.cpp $(GAME_CPP_ROOT)/src/EC_Str.cpp $(GAME_CPP_ROOT)/src/EC_Mem.cpp $(GAME_CPP_ROOT)/src/aPacket.cpp $(GAME_CPP_ROOT)/src/GR_Main.cpp $(GAME_CPP_ROOT)/runtime/runtime.cpp -Wl,--gc-sections -lz -o $(BUILD)/test_gr_main_config_startup && $(BUILD)/test_gr_main_config_startup

host-gr-main-release-config-test: | $(BUILD)
	@g++ -std=c++20 -Wall -Wextra -Wpedantic -ffunction-sections -fdata-sections -Iplatform -I$(GAME_CPP_ROOT)/src -I$(GAME_CPP_ROOT)/runtime ../../tests/test_gr_main_release_config.cpp platform/package.cpp platform/game_path.cpp platform/ec_file_adapter.cpp platform/windows_imports_portable.cpp platform/sysutils_imports_portable.cpp platform/runtime_failure_hooks_portable.cpp $(GAME_CPP_ROOT)/src/CrcUnit.cpp $(GAME_CPP_ROOT)/src/BlockParException.cpp $(GAME_CPP_ROOT)/src/EC_File.cpp $(GAME_CPP_ROOT)/src/EC_Struct.cpp $(GAME_CPP_ROOT)/src/EC_Buf.cpp $(GAME_CPP_ROOT)/src/EC_BlockPar.cpp $(GAME_CPP_ROOT)/src/EC_Str.cpp $(GAME_CPP_ROOT)/src/EC_Mem.cpp $(GAME_CPP_ROOT)/src/aPacket.cpp $(GAME_CPP_ROOT)/src/GR_Main.cpp $(GAME_CPP_ROOT)/runtime/runtime.cpp -Wl,--gc-sections -lz -o $(BUILD)/test_gr_main_release_config && $(BUILD)/test_gr_main_release_config "../../windows/Space Rangers HD A War Apart"

host-platform-startup-test: | $(BUILD)
	@g++ -std=c++20 -Wall -Wextra -Wpedantic -ffunction-sections -fdata-sections -Iplatform -I$(GAME_CPP_ROOT)/src -I$(GAME_CPP_ROOT)/runtime ../../tests/test_platform_startup.cpp platform/package.cpp platform/game_path.cpp platform/ec_file_adapter.cpp platform/windows_imports_portable.cpp platform/sysutils_imports_portable.cpp platform/runtime_failure_hooks_portable.cpp platform/runtime_platform.cpp platform/startup_slice.cpp $(GAME_CPP_ROOT)/src/CrcUnit.cpp $(GAME_CPP_ROOT)/src/BlockParException.cpp $(GAME_CPP_ROOT)/src/EC_File.cpp $(GAME_CPP_ROOT)/src/EC_Struct.cpp $(GAME_CPP_ROOT)/src/EC_Buf.cpp $(GAME_CPP_ROOT)/src/EC_BlockPar.cpp $(GAME_CPP_ROOT)/src/EC_Str.cpp $(GAME_CPP_ROOT)/src/EC_Mem.cpp $(GAME_CPP_ROOT)/src/aPacket.cpp $(GAME_CPP_ROOT)/src/MessageText.cpp $(GAME_CPP_ROOT)/src/GR_Main.cpp $(GAME_CPP_ROOT)/runtime/runtime.cpp -Wl,--gc-sections -lz -o $(BUILD)/test_platform_startup && $(BUILD)/test_platform_startup

host-package-cycle-test: | $(BUILD)
	g++ -std=c++20 -Wall -Wextra -Iplatform ../../tests/test_package_cycles.cpp platform/package.cpp -lz -o $(BUILD)/test_package_cycles
	$(BUILD)/test_package_cycles
