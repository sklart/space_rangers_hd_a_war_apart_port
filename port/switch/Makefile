DEVKITPRO ?= C:/devkitPro

.PHONY: switch-e2e-game
switch-e2e-game:
	$(PYTHON) ../../tools/e2e_arm64_build.py

.PHONY: host-e2e-time-test
host-e2e-time-test:
	$(PYTHON) ../../tools/e2e_time_test.py

.PHONY: host-e2e-events-test
host-e2e-events-test:
	@mkdir -p build/e2e-events
	g++ -std=c++20 -O2 -pthread -Iplatform ../../tests/test_e2e_events.cpp platform/e2e_events.cpp -o build/e2e-events/test_e2e_events
	build/e2e-events/test_e2e_events

.PHONY: host-e2e-threads-test
host-e2e-threads-test:
	@mkdir -p build/e2e-threads
	g++ -std=c++20 -O2 -pthread -Iplatform ../../tests/test_e2e_threads.cpp platform/e2e_threads.cpp -o build/e2e-threads/test_e2e_threads
	build/e2e-threads/test_e2e_threads

.PHONY: host-e2e-command-line-test
host-e2e-command-line-test:
	$(PYTHON) ../../tools/e2e_command_line_test.py

.PHONY: host-e2e-file-search-test
host-e2e-file-search-test:
	$(PYTHON) ../../tools/e2e_file_search_test.py
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
BUILD_GIT_HEADER := $(BUILD)/build_git_commit.hpp
HOST_EC_PATCH_FILE := $(abspath platform/msys-runtime-text.patch)
GR_MAIN_PORTABLE_PATCH_FILE := $(abspath platform/gr_main_portable.patch)
GR_MAIN_ZLIB_PORTABLE_PATCH_FILE := $(abspath platform/gr_main_zlib_portable.patch)
DAT_ZLIB_GUARD_PATCH_FILE := $(abspath platform/dat_zlib_guard.patch)
ifeq ($(shell uname -o),Msys)
GIT_APPLY := /c/Program\ Files/Git/cmd/git.exe
else
GIT_APPLY := git
endif

CC := $(DEVKITA64)/bin/aarch64-none-elf-gcc
CXX := $(DEVKITA64)/bin/aarch64-none-elf-g++
ELF2NRO := $(DEVKITPRO)/tools/bin/elf2nro
NACPTOOL := $(DEVKITPRO)/tools/bin/nacptool
NRO_ICON := assets/icon.jpg
NACP := $(BUILD)/$(TARGET).nacp
NACP_NAME := Space Rangers HD: A War Apart
NACP_AUTHOR := sklart
NACP_VERSION := 0.0.1
NRO_OUTPUT := Space\ Rangers\ HD\ -\ A\ War\ Apart.nro
ICON_VALIDATOR := ../../tools/validate-switch-icon.py
PYTHON ?= python3

INCLUDE := -I$(LIBNX)/include -I$(PORTLIBS)/include -I$(BUILD) -I$(INCLUDES) -Iplatform -I$(GAME_CPP_ROOT)/src -I$(GAME_CPP_ROOT)/runtime -I../../upstream/okgf/include
LIBDIRS := -L$(LIBNX)/lib -L$(PORTLIBS)/lib
ARCH := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE
CFLAGS := -g -O2 -Wall -Wextra $(ARCH) -ffunction-sections -fdata-sections $(INCLUDE) -D__SWITCH__
CXXFLAGS := $(CFLAGS) -std=gnu++20 -DSRHD_PORTABLE_RUNTIME_SLICE
LDFLAGS := -specs=$(LIBNX)/switch.specs -g $(ARCH) -Wl,-Map,$(BUILD)/$(TARGET).map -Wl,--gc-sections
OKGF_SWITCH_BUILD := $(BUILD)/okgf-switch-no-tls
OKGF_LIB := $(OKGF_SWITCH_BUILD)/libokgf.a
OKGF_SOFTFLOAT_LIB := $(OKGF_SWITCH_BUILD)/vendor/softfloat/libokgf_softfloat.a
LIBS := $(OKGF_LIB) $(OKGF_SOFTFLOAT_LIB) -lpng -lz -ljpeg -lm -lSDL2 -lEGL -lglapi -ldrm_nouveau -lnx -lpthread

CPPFILES := $(wildcard $(SOURCES)/*.cpp)
OFILES := $(CPPFILES:$(SOURCES)/%.cpp=$(BUILD)/%.o)
PLATFORM_CPPFILES := $(wildcard platform/*.cpp)
PLATFORM_OFILES := $(PLATFORM_CPPFILES:platform/%.cpp=$(BUILD)/platform_%.o)
GAME_OFILES := $(BUILD)/CrcUnit.o $(BUILD)/System.o $(BUILD)/SystemImports.o $(BUILD)/BlockParException.o $(BUILD)/GI_Main.o $(BUILD)/GlobalsV.o $(BUILD)/aMyFunction.o $(BUILD)/EC_File.o $(BUILD)/EC_Struct.o $(BUILD)/EC_Buf.o $(BUILD)/EC_Cache.o $(BUILD)/EC_CacheBitmap.o $(BUILD)/EC_BlockPar.o $(BUILD)/EC_Data.o $(BUILD)/EC_Str.o $(BUILD)/EC_Mem.o $(BUILD)/aPacket.o $(BUILD)/MessageText.o $(BUILD)/GR_GraphBuf.o $(BUILD)/GR_Main.o $(BUILD)/runtime.o

.PHONY: all clean FORCE host-package-test host-package-cycle-test host-ec-file-test host-ec-file-synthetic-test host-blockpar-synthetic-test host-dat-config-test host-global-cache-synthetic-test host-configured-package-test host-gr-main-no-mod-test host-gr-main-config-test host-gr-main-release-config-test host-platform-startup-test host-renderer-software-test host-user-writable-test host-runtime-config-slice-test host-milestone4-regression host-milestone5-regression host-milestone6-regression host-milestone7-regression host-runtime-loop-test host-milestone12-regression host-bitmap-cache-test host-bitmap-cache-release-test host-milestone13-regression host-gi-format0-cpu-test host-gi-format0-release-test host-gai-cpu-test host-gai-sequence-cpu-test host-gi-format2-cpu-test host-gai-playback-cpu-test host-milestone17-regression host-software-compositor-test host-milestone18-regression host-scene-compositor-test host-milestone19-regression host-gi-object-test host-milestone20-regression host-image-cpu-test host-alpha-bitmap-cpu-test host-trans-bitmap-cpu-test host-simple-bitmap-cpu-test host-image-layout-test host-image-object-test host-m21-ui-image-regression host-ui-object-test host-ui-tree-render-test host-ui-config-test host-m22-ui-regression host-aft-font-test host-tagged-text-test host-font-renderer-test host-ui-label-test host-m23-ui-text-test host-m23-ui-regression host-m24-zone-test host-m24-ui-controls-test host-m24-ui-regression host-m25-real-ui-test host-ui-gai-test host-m25-ui-regression host-gi-image-test host-ui-gai-release-test host-m25-controls-release-test host-m26-graph-buffer-cpu-test host-m26-scroll-bar-test host-m26-panel-scroll-bar-test host-m26-real-scroll-controls-test host-m26-edit-test host-m27-input-core-test host-m27-real-input-test host-m27-input-platform-test host-m26-okgf
all: $(NRO_OUTPUT)

host-m26-okgf: | $(BUILD)
	@cmake -S ../../upstream/okgf -B $(BUILD)/host-okgf -DOKGF_MATH_BACKEND=NATIVE >/dev/null && cmake --build $(BUILD)/host-okgf --parallel 2 >/dev/null

host-m26-graph-buffer-cpu-test: host-m26-okgf | $(BUILD)
	@g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -ffunction-sections -fdata-sections -Iplatform -I$(GAME_CPP_ROOT)/src -I$(GAME_CPP_ROOT)/runtime -I../../upstream/okgf/include ../../tests/test_m26_graph_buffer_cpu.cpp platform/graph_buffer_cpu.cpp platform/gi_image_cpu.cpp platform/gi_format0_cpu.cpp platform/gi_format0_okgf_bridge.cpp platform/gi_format2_cpu.cpp platform/image_layout.cpp platform/software_compositor.cpp platform/image_cpu.cpp platform/image_okgf_bridge.cpp platform/okgf_rle_bridge.cpp platform/package.cpp platform/zlib_bridge.cpp -Wl,--gc-sections -L$(BUILD)/host-okgf -lokgf -lpng -ljpeg -lz -lm -o $(BUILD)/test_m26_graph_buffer_cpu && LD_LIBRARY_PATH=$(BUILD)/host-okgf $(BUILD)/test_m26_graph_buffer_cpu

host-m26-scroll-bar-test: | $(BUILD)
	@g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -ffunction-sections -fdata-sections -Iplatform -I../../upstream/okgf/include ../../tests/test_m26_scroll_bar.cpp platform/ui_scroll_bar.cpp platform/ui_object.cpp -Wl,--gc-sections -o $(BUILD)/test_m26_scroll_bar && $(BUILD)/test_m26_scroll_bar

host-m26-panel-scroll-bar-test: | $(BUILD)
	@g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -ffunction-sections -fdata-sections -Iplatform -I../../upstream/okgf/include ../../tests/test_m26_panel_scroll_bar.cpp platform/ui_panel_scroll_bar.cpp platform/ui_scroll_bar.cpp platform/ui_object.cpp -Wl,--gc-sections -o $(BUILD)/test_m26_panel_scroll_bar && $(BUILD)/test_m26_panel_scroll_bar

host-m25-real-ui-test: host-alpha-bitmap-cpu-test host-m26-okgf | $(BUILD)
	@patched=0; if test "$$(uname -o)" = Msys; then git -C $(GAME_CPP_ROOT) apply --ignore-space-change $(HOST_EC_PATCH_FILE) || exit $$?; patched=1; fi; g++ -std=c++20 -Wall -Wextra -Wpedantic -ffunction-sections -fdata-sections -Iplatform -I$(GAME_CPP_ROOT)/src -I$(GAME_CPP_ROOT)/runtime -I../../upstream/okgf/include ../../tests/test_m25_real_ui.cpp ../../tests/blockpar_test_support.cpp platform/ui_cache_resolver.cpp platform/ui_tree_fingerprint.cpp platform/ui_object.cpp platform/ui_zone.cpp platform/ui_graph_button.cpp platform/ui_window.cpp platform/ui_config.cpp platform/ui_graph_buffer.cpp platform/graph_buffer_cpu.cpp platform/ui_scroll_bar.cpp platform/ui_panel_scroll_bar.cpp platform/ui_edit.cpp platform/ui_tree_renderer.cpp platform/ui_gai.cpp platform/ui_label.cpp platform/tagged_text.cpp platform/font_renderer.cpp platform/aft_font.cpp platform/font_repository.cpp platform/image_object.cpp platform/gi_image_cpu.cpp platform/image_layout.cpp platform/simple_bitmap_cpu.cpp platform/trans_bitmap_cpu.cpp platform/alpha_bitmap_cpu.cpp platform/software_compositor.cpp platform/image_cpu.cpp platform/image_okgf_bridge.cpp platform/gi_object.cpp platform/gai_frame_sequence_cpu.cpp platform/package.cpp platform/gai_cpu.cpp platform/gai_playback_cpu.cpp platform/gi_format0_cpu.cpp platform/gi_format0_okgf_bridge.cpp platform/gi_format2_cpu.cpp platform/okgf_rle_bridge.cpp platform/scene_compositor.cpp platform/game_path.cpp platform/user_root.cpp platform/ec_file_adapter.cpp platform/windows_imports_portable.cpp platform/sysutils_imports_portable.cpp platform/runtime_time.cpp platform/runtime_failure_hooks_portable.cpp platform/zlib_bridge.cpp $(GAME_CPP_ROOT)/src/CrcUnit.cpp $(GAME_CPP_ROOT)/src/BlockParException.cpp $(GAME_CPP_ROOT)/src/EC_File.cpp $(GAME_CPP_ROOT)/src/EC_Struct.cpp $(GAME_CPP_ROOT)/src/EC_Buf.cpp $(GAME_CPP_ROOT)/src/EC_BlockPar.cpp $(GAME_CPP_ROOT)/src/EC_Data.cpp $(GAME_CPP_ROOT)/src/EC_Str.cpp $(GAME_CPP_ROOT)/src/EC_Mem.cpp $(GAME_CPP_ROOT)/src/aPacket.cpp $(GAME_CPP_ROOT)/runtime/runtime.cpp $(BUILD)/host-alpha-bitmap/bmp_read.o $(BUILD)/host-alpha-bitmap/psd_read.o $(BUILD)/host-alpha-bitmap/image_read.o $(BUILD)/host-alpha-bitmap/rle.o $(BUILD)/host-alpha-bitmap/pixels.o $(BUILD)/host-alpha-bitmap/alpha_buffers.o $(BUILD)/host-alpha-bitmap/stubs.o -Wl,--gc-sections -L$(BUILD)/host-okgf -lokgf -lpng -ljpeg -lz -lm -o $(BUILD)/test_m25_real_ui; result=$$?; if test $$result -eq 0; then LD_LIBRARY_PATH=$(BUILD)/host-okgf $(BUILD)/test_m25_real_ui "$(GAME_ROOT)"; result=$$?; fi; if test $$result -eq 0; then ! grep -n -E 'GI_MessageLoop|TObjectGI|TPanelGI|GR_DX|Direct3D|TGraphBufGR|Forms' platform/ui_object.* platform/ui_config.*; result=$$?; fi; if test $$patched -eq 1; then git -C $(GAME_CPP_ROOT) apply -R --ignore-space-change $(HOST_EC_PATCH_FILE) || exit $$?; fi; exit $$result

host-m26-real-scroll-controls-test: host-alpha-bitmap-cpu-test host-m26-okgf | $(BUILD)
	@patched=0; if test "$$(uname -o)" = Msys; then git -C $(GAME_CPP_ROOT) apply --ignore-space-change $(HOST_EC_PATCH_FILE) || exit $$?; patched=1; fi; g++ -std=c++20 -Wall -Wextra -Wpedantic -ffunction-sections -fdata-sections -Iplatform -I$(GAME_CPP_ROOT)/src -I$(GAME_CPP_ROOT)/runtime -I../../upstream/okgf/include ../../tests/test_m26_real_scroll_controls.cpp ../../tests/blockpar_test_support.cpp platform/ui_cache_resolver.cpp platform/ui_tree_fingerprint.cpp platform/ui_object.cpp platform/ui_zone.cpp platform/ui_graph_button.cpp platform/ui_window.cpp platform/ui_config.cpp platform/ui_graph_buffer.cpp platform/graph_buffer_cpu.cpp platform/ui_scroll_bar.cpp platform/ui_panel_scroll_bar.cpp platform/ui_edit.cpp platform/ui_tree_renderer.cpp platform/ui_gai.cpp platform/ui_label.cpp platform/tagged_text.cpp platform/font_renderer.cpp platform/aft_font.cpp platform/font_repository.cpp platform/image_object.cpp platform/gi_image_cpu.cpp platform/image_layout.cpp platform/simple_bitmap_cpu.cpp platform/trans_bitmap_cpu.cpp platform/alpha_bitmap_cpu.cpp platform/software_compositor.cpp platform/image_cpu.cpp platform/image_okgf_bridge.cpp platform/gi_object.cpp platform/gai_frame_sequence_cpu.cpp platform/package.cpp platform/gai_cpu.cpp platform/gai_playback_cpu.cpp platform/gi_format0_cpu.cpp platform/gi_format0_okgf_bridge.cpp platform/gi_format2_cpu.cpp platform/okgf_rle_bridge.cpp platform/scene_compositor.cpp platform/game_path.cpp platform/user_root.cpp platform/ec_file_adapter.cpp platform/windows_imports_portable.cpp platform/sysutils_imports_portable.cpp platform/runtime_time.cpp platform/runtime_failure_hooks_portable.cpp platform/zlib_bridge.cpp $(GAME_CPP_ROOT)/src/CrcUnit.cpp $(GAME_CPP_ROOT)/src/BlockParException.cpp $(GAME_CPP_ROOT)/src/EC_File.cpp $(GAME_CPP_ROOT)/src/EC_Struct.cpp $(GAME_CPP_ROOT)/src/EC_Buf.cpp $(GAME_CPP_ROOT)/src/EC_BlockPar.cpp $(GAME_CPP_ROOT)/src/EC_Data.cpp $(GAME_CPP_ROOT)/src/EC_Str.cpp $(GAME_CPP_ROOT)/src/EC_Mem.cpp $(GAME_CPP_ROOT)/src/aPacket.cpp $(GAME_CPP_ROOT)/runtime/runtime.cpp $(BUILD)/host-alpha-bitmap/bmp_read.o $(BUILD)/host-alpha-bitmap/psd_read.o $(BUILD)/host-alpha-bitmap/image_read.o $(BUILD)/host-alpha-bitmap/rle.o $(BUILD)/host-alpha-bitmap/pixels.o $(BUILD)/host-alpha-bitmap/alpha_buffers.o $(BUILD)/host-alpha-bitmap/stubs.o -Wl,--gc-sections -L$(BUILD)/host-okgf -lokgf -lpng -ljpeg -lz -lm -o $(BUILD)/test_m26_real_scroll_controls; result=$$?; if test $$result -eq 0; then LD_LIBRARY_PATH=$(BUILD)/host-okgf $(BUILD)/test_m26_real_scroll_controls "$(GAME_ROOT)"; result=$$?; fi; if test $$result -eq 0; then ! grep -n -E 'GI_MessageLoop|TObjectGI|TPanelGI|GR_DX|Direct3D|TGraphBufGR|Forms' platform/ui_object.* platform/ui_config.*; result=$$?; fi; if test $$patched -eq 1; then git -C $(GAME_CPP_ROOT) apply -R --ignore-space-change $(HOST_EC_PATCH_FILE) || exit $$?; fi; exit $$result

host-m27-input-core-test: host-alpha-bitmap-cpu-test host-m26-okgf | $(BUILD)
	@patched=0; if test "$$(uname -o)" = Msys; then git -C $(GAME_CPP_ROOT) apply --ignore-space-change $(HOST_EC_PATCH_FILE) || exit $$?; patched=1; fi; g++ -std=c++20 -Wall -Wextra -Wpedantic -ffunction-sections -fdata-sections -Iplatform -I$(GAME_CPP_ROOT)/src -I$(GAME_CPP_ROOT)/runtime -I../../upstream/okgf/include ../../tests/test_m27_input_core.cpp ../../tests/blockpar_test_support.cpp platform/ui_cache_resolver.cpp platform/ui_tree_fingerprint.cpp platform/ui_object.cpp platform/ui_zone.cpp platform/ui_graph_button.cpp platform/ui_window.cpp platform/ui_config.cpp platform/ui_graph_buffer.cpp platform/graph_buffer_cpu.cpp platform/ui_scroll_bar.cpp platform/ui_panel_scroll_bar.cpp platform/ui_edit.cpp platform/ui_input.cpp platform/ui_tree_renderer.cpp platform/ui_gai.cpp platform/ui_label.cpp platform/tagged_text.cpp platform/font_renderer.cpp platform/aft_font.cpp platform/font_repository.cpp platform/image_object.cpp platform/gi_image_cpu.cpp platform/image_layout.cpp platform/simple_bitmap_cpu.cpp platform/trans_bitmap_cpu.cpp platform/alpha_bitmap_cpu.cpp platform/software_compositor.cpp platform/image_cpu.cpp platform/image_okgf_bridge.cpp platform/gi_object.cpp platform/gai_frame_sequence_cpu.cpp platform/package.cpp platform/gai_cpu.cpp platform/gai_playback_cpu.cpp platform/gi_format0_cpu.cpp platform/gi_format0_okgf_bridge.cpp platform/gi_format2_cpu.cpp platform/okgf_rle_bridge.cpp platform/scene_compositor.cpp platform/game_path.cpp platform/user_root.cpp platform/ec_file_adapter.cpp platform/windows_imports_portable.cpp platform/sysutils_imports_portable.cpp platform/runtime_time.cpp platform/runtime_failure_hooks_portable.cpp platform/zlib_bridge.cpp $(GAME_CPP_ROOT)/src/CrcUnit.cpp $(GAME_CPP_ROOT)/src/BlockParException.cpp $(GAME_CPP_ROOT)/src/EC_File.cpp $(GAME_CPP_ROOT)/src/EC_Struct.cpp $(GAME_CPP_ROOT)/src/EC_Buf.cpp $(GAME_CPP_ROOT)/src/EC_BlockPar.cpp $(GAME_CPP_ROOT)/src/EC_Data.cpp $(GAME_CPP_ROOT)/src/EC_Str.cpp $(GAME_CPP_ROOT)/src/EC_Mem.cpp $(GAME_CPP_ROOT)/src/aPacket.cpp $(GAME_CPP_ROOT)/runtime/runtime.cpp $(BUILD)/host-alpha-bitmap/bmp_read.o $(BUILD)/host-alpha-bitmap/psd_read.o $(BUILD)/host-alpha-bitmap/image_read.o $(BUILD)/host-alpha-bitmap/rle.o $(BUILD)/host-alpha-bitmap/pixels.o $(BUILD)/host-alpha-bitmap/alpha_buffers.o $(BUILD)/host-alpha-bitmap/stubs.o -Wl,--gc-sections -L$(BUILD)/host-okgf -lokgf -lpng -ljpeg -lz -lm -o $(BUILD)/test_m27_input_core; result=$$?; if test $$result -eq 0; then LD_LIBRARY_PATH=$(BUILD)/host-okgf $(BUILD)/test_m27_input_core "$(GAME_ROOT)"; result=$$?; fi; if test $$result -eq 0; then ! grep -n -E 'GI_MessageLoop|TObjectGI|TPanelGI|GR_DX|Direct3D|TGraphBufGR|Forms' platform/ui_object.* platform/ui_config.*; result=$$?; fi; if test $$patched -eq 1; then git -C $(GAME_CPP_ROOT) apply -R --ignore-space-change $(HOST_EC_PATCH_FILE) || exit $$?; fi; exit $$result

host-m27-real-input-test: host-alpha-bitmap-cpu-test host-m26-okgf | $(BUILD)
	@patched=0; if test "$$(uname -o)" = Msys; then git -C $(GAME_CPP_ROOT) apply --ignore-space-change $(HOST_EC_PATCH_FILE) || exit $$?; patched=1; fi; g++ -std=c++20 -Wall -Wextra -Wpedantic -ffunction-sections -fdata-sections -Iplatform -I$(GAME_CPP_ROOT)/src -I$(GAME_CPP_ROOT)/runtime -I../../upstream/okgf/include ../../tests/test_m27_real_input.cpp ../../tests/blockpar_test_support.cpp platform/ui_cache_resolver.cpp platform/ui_tree_fingerprint.cpp platform/ui_object.cpp platform/ui_zone.cpp platform/ui_graph_button.cpp platform/ui_window.cpp platform/ui_config.cpp platform/ui_graph_buffer.cpp platform/graph_buffer_cpu.cpp platform/ui_scroll_bar.cpp platform/ui_panel_scroll_bar.cpp platform/ui_edit.cpp platform/ui_input.cpp platform/ui_tree_renderer.cpp platform/ui_gai.cpp platform/ui_label.cpp platform/tagged_text.cpp platform/font_renderer.cpp platform/aft_font.cpp platform/font_repository.cpp platform/image_object.cpp platform/gi_image_cpu.cpp platform/image_layout.cpp platform/simple_bitmap_cpu.cpp platform/trans_bitmap_cpu.cpp platform/alpha_bitmap_cpu.cpp platform/software_compositor.cpp platform/image_cpu.cpp platform/image_okgf_bridge.cpp platform/gi_object.cpp platform/gai_frame_sequence_cpu.cpp platform/package.cpp platform/gai_cpu.cpp platform/gai_playback_cpu.cpp platform/gi_format0_cpu.cpp platform/gi_format0_okgf_bridge.cpp platform/gi_format2_cpu.cpp platform/okgf_rle_bridge.cpp platform/scene_compositor.cpp platform/game_path.cpp platform/user_root.cpp platform/ec_file_adapter.cpp platform/windows_imports_portable.cpp platform/sysutils_imports_portable.cpp platform/runtime_time.cpp platform/runtime_failure_hooks_portable.cpp platform/zlib_bridge.cpp $(GAME_CPP_ROOT)/src/CrcUnit.cpp $(GAME_CPP_ROOT)/src/BlockParException.cpp $(GAME_CPP_ROOT)/src/EC_File.cpp $(GAME_CPP_ROOT)/src/EC_Struct.cpp $(GAME_CPP_ROOT)/src/EC_Buf.cpp $(GAME_CPP_ROOT)/src/EC_BlockPar.cpp $(GAME_CPP_ROOT)/src/EC_Data.cpp $(GAME_CPP_ROOT)/src/EC_Str.cpp $(GAME_CPP_ROOT)/src/EC_Mem.cpp $(GAME_CPP_ROOT)/src/aPacket.cpp $(GAME_CPP_ROOT)/runtime/runtime.cpp $(BUILD)/host-alpha-bitmap/bmp_read.o $(BUILD)/host-alpha-bitmap/psd_read.o $(BUILD)/host-alpha-bitmap/image_read.o $(BUILD)/host-alpha-bitmap/rle.o $(BUILD)/host-alpha-bitmap/pixels.o $(BUILD)/host-alpha-bitmap/alpha_buffers.o $(BUILD)/host-alpha-bitmap/stubs.o -Wl,--gc-sections -L$(BUILD)/host-okgf -lokgf -lpng -ljpeg -lz -lm -o $(BUILD)/test_m27_real_input; result=$$?; if test $$result -eq 0; then LD_LIBRARY_PATH=$(BUILD)/host-okgf $(BUILD)/test_m27_real_input "$(GAME_ROOT)"; result=$$?; fi; if test $$result -eq 0; then ! grep -n -E 'GI_MessageLoop|TObjectGI|TPanelGI|GR_DX|Direct3D|TGraphBufGR|Forms' platform/ui_object.* platform/ui_config.*; result=$$?; fi; if test $$patched -eq 1; then git -C $(GAME_CPP_ROOT) apply -R --ignore-space-change $(HOST_EC_PATCH_FILE) || exit $$?; fi; exit $$result

host-m27-input-platform-test: | $(BUILD)
	@g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -Iplatform ../../tests/test_m27_input_platform.cpp platform/input_platform.cpp -o $(BUILD)/test_m27_input_platform && $(BUILD)/test_m27_input_platform

host-gi-image-test: host-alpha-bitmap-cpu-test | $(BUILD)
	@g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -Iplatform -I$(GAME_CPP_ROOT)/src -I$(GAME_CPP_ROOT)/runtime -I../../upstream/okgf/include ../../tests/test_m25_gi_image.cpp platform/gi_image_cpu.cpp platform/gi_format0_cpu.cpp platform/gi_format0_okgf_bridge.cpp platform/gi_format2_cpu.cpp platform/image_layout.cpp platform/software_compositor.cpp platform/okgf_rle_bridge.cpp platform/package.cpp platform/zlib_bridge.cpp $(BUILD)/host-alpha-bitmap/bmp_read.o $(BUILD)/host-alpha-bitmap/psd_read.o $(BUILD)/host-alpha-bitmap/image_read.o $(BUILD)/host-alpha-bitmap/rle.o $(BUILD)/host-alpha-bitmap/pixels.o $(BUILD)/host-alpha-bitmap/alpha_buffers.o $(BUILD)/host-alpha-bitmap/stubs.o -lz -o $(BUILD)/test_m25_gi_image && $(BUILD)/test_m25_gi_image

$(BUILD):
	mkdir -p $(BUILD)

FORCE:

$(BUILD_GIT_HEADER): FORCE | $(BUILD)
	@commit="$$(git -C ../.. rev-parse --short HEAD)"; { printf '%s\n' '#pragma once'; printf '#define SRHD_BUILD_GIT_COMMIT "%s"\n' "$$commit"; } > $@.tmp; if test -f $@ && cmp -s $@.tmp $@; then rm -f $@.tmp; else mv $@.tmp $@; fi

$(BUILD)/main.o: $(BUILD_GIT_HEADER)

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

$(BUILD)/BlockParException.o: $(GAME_CPP_ROOT)/src/BlockParException.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/GI_Main.o: $(GAME_CPP_ROOT)/src/GI_Main.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/GlobalsV.o: $(GAME_CPP_ROOT)/src/GlobalsV.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/aMyFunction.o: $(GAME_CPP_ROOT)/src/aMyFunction.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/EC_File.o: $(GAME_CPP_ROOT)/src/EC_File.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/EC_Struct.o: $(GAME_CPP_ROOT)/src/EC_Struct.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/EC_Buf.o: $(GAME_CPP_ROOT)/src/EC_Buf.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/EC_Cache.o: $(GAME_CPP_ROOT)/src/EC_Cache.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/EC_CacheBitmap.o: $(GAME_CPP_ROOT)/src/EC_CacheBitmap.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/EC_BlockPar.o $(BUILD)/EC_Data.o &: $(GAME_CPP_ROOT)/src/EC_BlockPar.cpp $(GAME_CPP_ROOT)/src/EC_Data.cpp $(DAT_ZLIB_GUARD_PATCH_FILE) | $(BUILD)
	@patched=0; $(GIT_APPLY) -C $(GAME_CPP_ROOT) apply --ignore-space-change $(DAT_ZLIB_GUARD_PATCH_FILE) || exit $$?; patched=1; $(CXX) $(CXXFLAGS) -c $(GAME_CPP_ROOT)/src/EC_BlockPar.cpp -o $(BUILD)/EC_BlockPar.o; result=$$?; if test $$result -eq 0; then $(CXX) $(CXXFLAGS) -c $(GAME_CPP_ROOT)/src/EC_Data.cpp -o $(BUILD)/EC_Data.o; result=$$?; fi; if test $$patched -eq 1; then $(GIT_APPLY) -C $(GAME_CPP_ROOT) apply -R --ignore-space-change $(DAT_ZLIB_GUARD_PATCH_FILE) || exit $$?; fi; exit $$result

$(BUILD)/EC_Str.o: $(GAME_CPP_ROOT)/src/EC_Str.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/EC_Mem.o: $(GAME_CPP_ROOT)/src/EC_Mem.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/aPacket.o: $(GAME_CPP_ROOT)/src/aPacket.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/MessageText.o: $(GAME_CPP_ROOT)/src/MessageText.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/GR_Main.o: $(GAME_CPP_ROOT)/src/GR_Main.cpp $(GR_MAIN_PORTABLE_PATCH_FILE) $(GR_MAIN_ZLIB_PORTABLE_PATCH_FILE) | $(BUILD)
	@patched_gr=0; patched_zlib=0; $(GIT_APPLY) -C $(GAME_CPP_ROOT) apply --ignore-space-change $(GR_MAIN_PORTABLE_PATCH_FILE) || exit $$?; patched_gr=1; $(GIT_APPLY) -C $(GAME_CPP_ROOT) apply --ignore-space-change $(GR_MAIN_ZLIB_PORTABLE_PATCH_FILE) || exit $$?; patched_zlib=1; $(CXX) $(CXXFLAGS) -c $< -o $@; result=$$?; if test $$patched_zlib -eq 1; then $(GIT_APPLY) -C $(GAME_CPP_ROOT) apply -R --ignore-space-change $(GR_MAIN_ZLIB_PORTABLE_PATCH_FILE) || exit $$?; fi; if test $$patched_gr -eq 1; then $(GIT_APPLY) -C $(GAME_CPP_ROOT) apply -R --ignore-space-change $(GR_MAIN_PORTABLE_PATCH_FILE) || exit $$?; fi; exit $$result

$(BUILD)/GR_GraphBuf.o: $(GAME_CPP_ROOT)/src/GR_GraphBuf.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/runtime.o: $(GAME_CPP_ROOT)/runtime/runtime.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OKGF_LIB):
	# libnx's hbloader leaves the C11 TLS base unavailable here; OKGF is used
	# on the port's main thread, so keep its math state in ordinary storage.
	cmake -S ../../upstream/okgf -B $(OKGF_SWITCH_BUILD) -G "Unix Makefiles" -DCMAKE_SYSTEM_NAME=Generic -DCMAKE_C_COMPILER=$(DEVKITA64_MSYS)/bin/aarch64-none-elf-gcc -DCMAKE_C_FLAGS=-D_Thread_local= -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY -DCMAKE_FIND_ROOT_PATH=$(PORTLIBS_MSYS) -DCMAKE_PREFIX_PATH=$(PORTLIBS_MSYS) -DOKGF_MATH_BACKEND=COMPATIBLE -DOKGF_PORTABLE_MATH=ON
	cmake --build $(OKGF_SWITCH_BUILD) --parallel 2
	@if $(DEVKITA64)/bin/aarch64-none-elf-readelf -sW $(OKGF_LIB) $(OKGF_SOFTFLOAT_LIB) | grep -Eq '[[:space:]]TLS[[:space:]]'; then echo "Switch OKGF libraries must not contain TLS symbols" >&2; exit 1; fi

$(BUILD)/$(TARGET).elf: $(OFILES) $(PLATFORM_OFILES) $(GAME_OFILES) $(OKGF_LIB)
	$(CXX) $(OFILES) $(PLATFORM_OFILES) $(GAME_OFILES) $(LIBDIRS) $(LDFLAGS) $(LIBS) -o $@

$(NACP): | $(BUILD)
	$(NACPTOOL) --create "$(NACP_NAME)" "$(NACP_AUTHOR)" "$(NACP_VERSION)" $@

$(NRO_OUTPUT): $(BUILD)/$(TARGET).elf $(NRO_ICON) $(NACP)
	$(PYTHON) $(ICON_VALIDATOR) $(NRO_ICON)
	$(ELF2NRO) $< "$@" --nacp=$(NACP) --icon=$(NRO_ICON)

clean:
	rm -rf $(BUILD) $(NRO_OUTPUT)

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

host-milestone8-regression: host-milestone7-regression host-renderer-software-test
	@echo MILESTONE 8 HOST REGRESSION PASS

host-milestone9-regression: host-milestone8-regression host-dat-config-test host-user-writable-test host-runtime-config-slice-test
	@echo MILESTONE 9 HOST REGRESSION PASS

host-milestone12-regression: host-milestone9-regression host-runtime-loop-test
	@echo MILESTONE 12 HOST REGRESSION PASS

host-runtime-loop-test: host-renderer-software-test | $(BUILD)
	@patched_text=0; patched_gr=0; patched_zlib=0; if test "$$(uname -o)" = Msys; then git -C $(GAME_CPP_ROOT) apply --ignore-space-change $(HOST_EC_PATCH_FILE) || exit $$?; patched_text=1; fi; git -C $(GAME_CPP_ROOT) apply --ignore-space-change $(GR_MAIN_PORTABLE_PATCH_FILE) || exit $$?; patched_gr=1; git -C $(GAME_CPP_ROOT) apply --ignore-space-change $(GR_MAIN_ZLIB_PORTABLE_PATCH_FILE) || exit $$?; patched_zlib=1; g++ -std=c++20 -Wall -Wextra -Wpedantic -DSRHD_PORTABLE_RENDERER -ffunction-sections -fdata-sections -Iplatform -I$(GAME_CPP_ROOT)/src -I$(GAME_CPP_ROOT)/runtime -I../../upstream/okgf/include ../../tests/test_runtime_loop_slice.cpp platform/okgf_bridge.cpp platform/renderer_platform.cpp platform/runtime_loop_slice.cpp platform/input_platform.cpp platform/runtime_platform.cpp platform/package.cpp platform/game_path.cpp platform/user_root.cpp platform/ec_file_adapter.cpp platform/windows_imports_portable.cpp platform/sysutils_imports_portable.cpp platform/runtime_time.cpp platform/runtime_failure_hooks_portable.cpp platform/zlib_bridge.cpp $(GAME_CPP_ROOT)/src/CrcUnit.cpp $(GAME_CPP_ROOT)/src/System.cpp $(GAME_CPP_ROOT)/src/SystemImports.cpp $(GAME_CPP_ROOT)/src/BlockParException.cpp $(GAME_CPP_ROOT)/src/GI_Main.cpp $(GAME_CPP_ROOT)/src/GlobalsV.cpp $(GAME_CPP_ROOT)/src/aMyFunction.cpp $(GAME_CPP_ROOT)/src/EC_File.cpp $(GAME_CPP_ROOT)/src/EC_Struct.cpp $(GAME_CPP_ROOT)/src/EC_Buf.cpp $(GAME_CPP_ROOT)/src/EC_Cache.cpp $(GAME_CPP_ROOT)/src/EC_BlockPar.cpp $(GAME_CPP_ROOT)/src/EC_Data.cpp $(GAME_CPP_ROOT)/src/EC_Str.cpp $(GAME_CPP_ROOT)/src/EC_Mem.cpp $(GAME_CPP_ROOT)/src/aPacket.cpp $(GAME_CPP_ROOT)/src/MessageText.cpp $(GAME_CPP_ROOT)/src/GR_GraphBuf.cpp $(GAME_CPP_ROOT)/src/GR_Main.cpp $(GAME_CPP_ROOT)/runtime/runtime.cpp $(BUILD)/host-renderer/pixels.o $(BUILD)/host-renderer/copy.o $(BUILD)/host-renderer/alpha_buffers.o $(BUILD)/host-renderer/gradients.o $(BUILD)/host-renderer/precision.o -Wl,--gc-sections -lz -lm -o $(BUILD)/test_runtime_loop_slice; result=$$?; if test $$result -eq 0; then $(BUILD)/test_runtime_loop_slice; result=$$?; fi; if test $$patched_zlib -eq 1; then git -C $(GAME_CPP_ROOT) apply -R --ignore-space-change $(GR_MAIN_ZLIB_PORTABLE_PATCH_FILE) || exit $$?; fi; if test $$patched_gr -eq 1; then git -C $(GAME_CPP_ROOT) apply -R --ignore-space-change $(GR_MAIN_PORTABLE_PATCH_FILE) || exit $$?; fi; if test $$patched_text -eq 1; then git -C $(GAME_CPP_ROOT) apply -R --ignore-space-change $(HOST_EC_PATCH_FILE) || exit $$?; fi; exit $$result
host-renderer-software-test: | $(BUILD)
	@patched_text=0; patched=0; if test "$$(uname -o)" = Msys; then git -C $(GAME_CPP_ROOT) apply --ignore-space-change $(HOST_EC_PATCH_FILE) || exit $$?; patched_text=1; fi; git -C $(GAME_CPP_ROOT) apply --ignore-space-change $(GR_MAIN_PORTABLE_PATCH_FILE) || exit $$?; patched=1; mkdir -p $(BUILD)/host-renderer; gcc -std=c11 -I../../upstream/okgf/include -I../../upstream/okgf/src -c ../../upstream/okgf/src/pixels.c -o $(BUILD)/host-renderer/pixels.o; gcc -std=c11 -I../../upstream/okgf/include -I../../upstream/okgf/src -c ../../upstream/okgf/src/copy.c -o $(BUILD)/host-renderer/copy.o; gcc -std=c11 -I../../upstream/okgf/include -I../../upstream/okgf/src -c ../../upstream/okgf/src/alpha_buffers.c -o $(BUILD)/host-renderer/alpha_buffers.o; gcc -std=c11 -I../../upstream/okgf/include -I../../upstream/okgf/src -c ../../upstream/okgf/src/gradients.c -o $(BUILD)/host-renderer/gradients.o; gcc -std=c11 -I../../upstream/okgf/include -I../../upstream/okgf/src -c ../../upstream/okgf/src/math/precision.c -o $(BUILD)/host-renderer/precision.o; result=$$?; if test $$result -eq 0; then g++ -std=c++20 -Wall -Wextra -Wpedantic -DSRHD_PORTABLE_RENDERER -ffunction-sections -fdata-sections -Iplatform -I$(GAME_CPP_ROOT)/src -I$(GAME_CPP_ROOT)/runtime -I../../upstream/okgf/include ../../tests/test_renderer_software.cpp platform/okgf_bridge.cpp platform/renderer_platform.cpp platform/runtime_time.cpp platform/runtime_failure_hooks_portable.cpp platform/windows_imports_portable.cpp $(GAME_CPP_ROOT)/src/System.cpp $(GAME_CPP_ROOT)/src/SystemImports.cpp $(GAME_CPP_ROOT)/src/GlobalsV.cpp $(GAME_CPP_ROOT)/src/EC_Struct.cpp $(GAME_CPP_ROOT)/src/EC_Mem.cpp $(GAME_CPP_ROOT)/src/GR_GraphBuf.cpp $(GAME_CPP_ROOT)/src/GR_Main.cpp $(GAME_CPP_ROOT)/runtime/runtime.cpp $(BUILD)/host-renderer/pixels.o $(BUILD)/host-renderer/copy.o $(BUILD)/host-renderer/alpha_buffers.o $(BUILD)/host-renderer/gradients.o $(BUILD)/host-renderer/precision.o -Wl,--gc-sections -lm -o $(BUILD)/test_renderer_software; result=$$?; fi; if test $$result -eq 0; then $(BUILD)/test_renderer_software; result=$$?; fi; if test $$patched -eq 1; then git -C $(GAME_CPP_ROOT) apply -R --ignore-space-change $(GR_MAIN_PORTABLE_PATCH_FILE) || exit $$?; fi; if test $$patched_text -eq 1; then git -C $(GAME_CPP_ROOT) apply -R --ignore-space-change $(HOST_EC_PATCH_FILE) || exit $$?; fi; exit $$result

host-user-writable-test: | $(BUILD)
	@patched=0; if test "$$(uname -o)" = Msys; then git -C $(GAME_CPP_ROOT) apply --ignore-space-change $(HOST_EC_PATCH_FILE) || exit $$?; patched=1; fi; g++ -std=c++20 -Wall -Wextra -Wpedantic -Iplatform -I$(GAME_CPP_ROOT)/src -I$(GAME_CPP_ROOT)/runtime ../../tests/test_user_writable.cpp platform/package.cpp platform/game_path.cpp platform/user_root.cpp platform/ec_file_adapter.cpp platform/windows_imports_portable.cpp $(GAME_CPP_ROOT)/src/EC_File.cpp $(GAME_CPP_ROOT)/src/EC_Struct.cpp $(GAME_CPP_ROOT)/runtime/runtime.cpp -lz -o $(BUILD)/test_user_writable; result=$$?; if test $$result -eq 0; then $(BUILD)/test_user_writable $(BUILD)/m9-user-root; result=$$?; fi; if test $$patched -eq 1; then git -C $(GAME_CPP_ROOT) apply -R --ignore-space-change $(HOST_EC_PATCH_FILE) || exit $$?; fi; exit $$result

host-runtime-config-slice-test: host-dat-config-test

host-global-cache-synthetic-test: host-dat-config-test

host-corrupt-package-test: | $(BUILD)
	g++ -std=c++20 -Wall -Wextra -Iplatform ../../tests/test_corrupt_package.cpp platform/package.cpp -lz -o $(BUILD)/test_corrupt_package
	$(BUILD)/test_corrupt_package

host-ec-file-test: | $(BUILD)
	@patched=0; if test "$$(uname -o)" = Msys; then git -C $(GAME_CPP_ROOT) apply --ignore-space-change $(HOST_EC_PATCH_FILE) || exit $$?; patched=1; fi; g++ -std=c++20 -Wall -Wextra -Wpedantic -Iplatform -I$(GAME_CPP_ROOT)/src -I$(GAME_CPP_ROOT)/runtime ../../tests/test_ec_file.cpp platform/package.cpp platform/game_path.cpp platform/user_root.cpp platform/ec_file_adapter.cpp platform/windows_imports_portable.cpp $(GAME_CPP_ROOT)/src/EC_File.cpp $(GAME_CPP_ROOT)/src/EC_Struct.cpp $(GAME_CPP_ROOT)/runtime/runtime.cpp -lz -o $(BUILD)/test_ec_file; result=$$?; if test $$result -eq 0; then $(BUILD)/test_ec_file "../../windows/Space Rangers HD A War Apart"; result=$$?; fi; if test $$patched -eq 1; then git -C $(GAME_CPP_ROOT) apply -R --ignore-space-change $(HOST_EC_PATCH_FILE) || exit $$?; fi; exit $$result

host-ec-file-synthetic-test: | $(BUILD)
	@patched=0; if test "$$(uname -o)" = Msys; then git -C $(GAME_CPP_ROOT) apply --ignore-space-change $(HOST_EC_PATCH_FILE) || exit $$?; patched=1; fi; g++ -std=c++20 -Wall -Wextra -Wpedantic -Iplatform -I$(GAME_CPP_ROOT)/src -I$(GAME_CPP_ROOT)/runtime ../../tests/test_ec_file_synthetic.cpp platform/package.cpp platform/game_path.cpp platform/user_root.cpp platform/ec_file_adapter.cpp platform/windows_imports_portable.cpp $(GAME_CPP_ROOT)/src/EC_File.cpp $(GAME_CPP_ROOT)/src/EC_Struct.cpp $(GAME_CPP_ROOT)/runtime/runtime.cpp -lz -o $(BUILD)/test_ec_file_synthetic; result=$$?; if test $$result -eq 0; then $(BUILD)/test_ec_file_synthetic; result=$$?; fi; if test $$patched -eq 1; then git -C $(GAME_CPP_ROOT) apply -R --ignore-space-change $(HOST_EC_PATCH_FILE) || exit $$?; fi; exit $$result

host-blockpar-synthetic-test: | $(BUILD)
	@patched=0; if test "$$(uname -o)" = Msys; then git -C $(GAME_CPP_ROOT) apply --ignore-space-change $(HOST_EC_PATCH_FILE) || exit $$?; patched=1; fi; g++ -std=c++20 -Wall -Wextra -Wpedantic -ffunction-sections -fdata-sections -Iplatform -I$(GAME_CPP_ROOT)/src -I$(GAME_CPP_ROOT)/runtime ../../tests/test_blockpar_synthetic.cpp ../../tests/blockpar_test_support.cpp platform/package.cpp platform/game_path.cpp platform/user_root.cpp platform/ec_file_adapter.cpp platform/windows_imports_portable.cpp platform/sysutils_imports_portable.cpp platform/runtime_time.cpp platform/runtime_failure_hooks_portable.cpp platform/zlib_bridge.cpp $(GAME_CPP_ROOT)/src/CrcUnit.cpp $(GAME_CPP_ROOT)/src/BlockParException.cpp $(GAME_CPP_ROOT)/src/EC_File.cpp $(GAME_CPP_ROOT)/src/EC_Struct.cpp $(GAME_CPP_ROOT)/src/EC_Buf.cpp $(GAME_CPP_ROOT)/src/EC_BlockPar.cpp $(GAME_CPP_ROOT)/src/EC_Str.cpp $(GAME_CPP_ROOT)/src/EC_Mem.cpp $(GAME_CPP_ROOT)/src/aPacket.cpp $(GAME_CPP_ROOT)/runtime/runtime.cpp -Wl,--gc-sections -lz -o $(BUILD)/test_blockpar_synthetic; result=$$?; if test $$result -eq 0; then $(BUILD)/test_blockpar_synthetic; result=$$?; fi; if test $$patched -eq 1; then git -C $(GAME_CPP_ROOT) apply -R --ignore-space-change $(HOST_EC_PATCH_FILE) || exit $$?; fi; exit $$result

host-dat-config-test: | $(BUILD)
	@patched_text=0; patched_gr=0; patched_zlib=0; if test "$$(uname -o)" = Msys; then git -C $(GAME_CPP_ROOT) apply --ignore-space-change $(HOST_EC_PATCH_FILE) || exit $$?; patched_text=1; fi; git -C $(GAME_CPP_ROOT) apply --ignore-space-change $(GR_MAIN_PORTABLE_PATCH_FILE) || exit $$?; patched_gr=1; git -C $(GAME_CPP_ROOT) apply --ignore-space-change $(GR_MAIN_ZLIB_PORTABLE_PATCH_FILE) || exit $$?; patched_zlib=1; g++ -std=c++20 -Wall -Wextra -Wpedantic -DSRHD_PORTABLE_RENDERER -DSRHD_PORTABLE_RUNTIME_SLICE -ffunction-sections -fdata-sections -Iplatform -I$(GAME_CPP_ROOT)/src -I$(GAME_CPP_ROOT)/runtime ../../tests/test_dat_synthetic.cpp platform/package.cpp platform/game_path.cpp platform/user_root.cpp platform/ec_file_adapter.cpp platform/windows_imports_portable.cpp platform/sysutils_imports_portable.cpp platform/runtime_time.cpp platform/runtime_failure_hooks_portable.cpp platform/runtime_settings_slice.cpp platform/ui_metadata_slice.cpp platform/zlib_bridge.cpp $(GAME_CPP_ROOT)/src/CrcUnit.cpp $(GAME_CPP_ROOT)/src/System.cpp $(GAME_CPP_ROOT)/src/SystemImports.cpp $(GAME_CPP_ROOT)/src/BlockParException.cpp $(GAME_CPP_ROOT)/src/GI_Main.cpp $(GAME_CPP_ROOT)/src/GlobalsV.cpp $(GAME_CPP_ROOT)/src/aMyFunction.cpp $(GAME_CPP_ROOT)/src/EC_File.cpp $(GAME_CPP_ROOT)/src/EC_Struct.cpp $(GAME_CPP_ROOT)/src/EC_Buf.cpp $(GAME_CPP_ROOT)/src/EC_Cache.cpp $(GAME_CPP_ROOT)/src/EC_BlockPar.cpp $(GAME_CPP_ROOT)/src/EC_Data.cpp $(GAME_CPP_ROOT)/src/EC_Str.cpp $(GAME_CPP_ROOT)/src/EC_Mem.cpp $(GAME_CPP_ROOT)/src/aPacket.cpp $(GAME_CPP_ROOT)/src/MessageText.cpp $(GAME_CPP_ROOT)/src/GR_GraphBuf.cpp $(GAME_CPP_ROOT)/src/GR_Main.cpp $(GAME_CPP_ROOT)/runtime/runtime.cpp -Wl,--gc-sections -lz -o $(BUILD)/test_dat_synthetic; result=$$?; if test $$result -eq 0; then $(BUILD)/test_dat_synthetic; result=$$?; fi; if test $$patched_zlib -eq 1; then git -C $(GAME_CPP_ROOT) apply -R --ignore-space-change $(GR_MAIN_ZLIB_PORTABLE_PATCH_FILE) || exit $$?; fi; if test $$patched_gr -eq 1; then git -C $(GAME_CPP_ROOT) apply -R --ignore-space-change $(GR_MAIN_PORTABLE_PATCH_FILE) || exit $$?; fi; if test $$patched_text -eq 1; then git -C $(GAME_CPP_ROOT) apply -R --ignore-space-change $(HOST_EC_PATCH_FILE) || exit $$?; fi; exit $$result

host-configured-package-test: | $(BUILD)
	@patched=0; if test "$$(uname -o)" = Msys; then git -C $(GAME_CPP_ROOT) apply --ignore-space-change $(HOST_EC_PATCH_FILE) || exit $$?; patched=1; fi; g++ -std=c++20 -Wall -Wextra -Wpedantic -ffunction-sections -fdata-sections -Iplatform -I$(GAME_CPP_ROOT)/src -I$(GAME_CPP_ROOT)/runtime ../../tests/test_configured_packages.cpp platform/blockpar_test_support.cpp platform/package.cpp platform/game_path.cpp platform/user_root.cpp platform/ec_file_adapter.cpp platform/windows_imports_portable.cpp platform/sysutils_imports_portable.cpp platform/runtime_time.cpp platform/runtime_failure_hooks_portable.cpp platform/zlib_bridge.cpp $(GAME_CPP_ROOT)/src/CrcUnit.cpp $(GAME_CPP_ROOT)/src/BlockParException.cpp $(GAME_CPP_ROOT)/src/EC_File.cpp $(GAME_CPP_ROOT)/src/EC_Struct.cpp $(GAME_CPP_ROOT)/src/EC_Buf.cpp $(GAME_CPP_ROOT)/src/EC_BlockPar.cpp $(GAME_CPP_ROOT)/src/EC_Str.cpp $(GAME_CPP_ROOT)/src/EC_Mem.cpp $(GAME_CPP_ROOT)/src/aPacket.cpp $(GAME_CPP_ROOT)/runtime/runtime.cpp -Wl,--gc-sections -lz -o $(BUILD)/test_configured_packages; result=$$?; if test $$result -eq 0; then $(BUILD)/test_configured_packages "../../windows/Space Rangers HD A War Apart"; result=$$?; fi; if test $$patched -eq 1; then git -C $(GAME_CPP_ROOT) apply -R --ignore-space-change $(HOST_EC_PATCH_FILE) || exit $$?; fi; exit $$result

host-gr-main-no-mod-test: | $(BUILD)
	@$(MAKE) host-config-harness TEST=../../tests/test_gr_main_no_mod_startup.cpp OUTPUT=test_gr_main_no_mod_startup ARGS=

host-gr-main-config-test: | $(BUILD)
	@$(MAKE) host-config-harness TEST=../../tests/test_gr_main_config_startup.cpp OUTPUT=test_gr_main_config_startup ARGS=

host-gr-main-release-config-test: | $(BUILD)
	@$(MAKE) host-config-harness TEST=../../tests/test_gr_main_release_config.cpp OUTPUT=test_gr_main_release_config ARGS='"../../windows/Space Rangers HD A War Apart"'

.PHONY: host-config-harness
host-config-harness: | $(BUILD)
	@patched=0; if test "$$(uname -o)" = Msys; then git -C $(GAME_CPP_ROOT) apply --ignore-space-change $(HOST_EC_PATCH_FILE) || exit $$?; patched=1; fi; g++ -std=c++20 -Wall -Wextra -Wpedantic -ffunction-sections -fdata-sections -Iplatform -I$(GAME_CPP_ROOT)/src -I$(GAME_CPP_ROOT)/runtime $(TEST) platform/blockpar_test_support.cpp platform/package.cpp platform/game_path.cpp platform/user_root.cpp platform/ec_file_adapter.cpp platform/windows_imports_portable.cpp platform/sysutils_imports_portable.cpp platform/runtime_time.cpp platform/runtime_failure_hooks_portable.cpp platform/zlib_bridge.cpp $(GAME_CPP_ROOT)/src/CrcUnit.cpp $(GAME_CPP_ROOT)/src/BlockParException.cpp $(GAME_CPP_ROOT)/src/EC_File.cpp $(GAME_CPP_ROOT)/src/EC_Struct.cpp $(GAME_CPP_ROOT)/src/EC_Buf.cpp $(GAME_CPP_ROOT)/src/EC_BlockPar.cpp $(GAME_CPP_ROOT)/src/EC_Str.cpp $(GAME_CPP_ROOT)/src/EC_Mem.cpp $(GAME_CPP_ROOT)/src/aPacket.cpp $(GAME_CPP_ROOT)/runtime/runtime.cpp -Wl,--gc-sections -lz -o $(BUILD)/$(OUTPUT); result=$$?; if test $$result -eq 0; then $(BUILD)/$(OUTPUT) $(ARGS); result=$$?; fi; if test $$patched -eq 1; then git -C $(GAME_CPP_ROOT) apply -R --ignore-space-change $(HOST_EC_PATCH_FILE) || exit $$?; fi; exit $$result

host-platform-startup-test: | $(BUILD)
	@patched=0; if test "$$(uname -o)" = Msys; then git -C $(GAME_CPP_ROOT) apply --ignore-space-change $(HOST_EC_PATCH_FILE) || exit $$?; patched=1; fi; g++ -std=c++20 -Wall -Wextra -Wpedantic -ffunction-sections -fdata-sections -Iplatform -I$(GAME_CPP_ROOT)/src -I$(GAME_CPP_ROOT)/runtime ../../tests/test_platform_startup.cpp platform/blockpar_test_support.cpp platform/package.cpp platform/game_path.cpp platform/user_root.cpp platform/ec_file_adapter.cpp platform/windows_imports_portable.cpp platform/sysutils_imports_portable.cpp platform/runtime_time.cpp platform/runtime_failure_hooks_portable.cpp platform/zlib_bridge.cpp platform/runtime_platform.cpp platform/startup_slice.cpp $(GAME_CPP_ROOT)/src/CrcUnit.cpp $(GAME_CPP_ROOT)/src/BlockParException.cpp $(GAME_CPP_ROOT)/src/EC_File.cpp $(GAME_CPP_ROOT)/src/EC_Struct.cpp $(GAME_CPP_ROOT)/src/EC_Buf.cpp $(GAME_CPP_ROOT)/src/EC_BlockPar.cpp $(GAME_CPP_ROOT)/src/EC_Str.cpp $(GAME_CPP_ROOT)/src/EC_Mem.cpp $(GAME_CPP_ROOT)/src/aPacket.cpp $(GAME_CPP_ROOT)/src/MessageText.cpp $(GAME_CPP_ROOT)/runtime/runtime.cpp -Wl,--gc-sections -lz -o $(BUILD)/test_platform_startup; result=$$?; if test $$result -eq 0; then $(BUILD)/test_platform_startup; result=$$?; fi; if test $$patched -eq 1; then git -C $(GAME_CPP_ROOT) apply -R --ignore-space-change $(HOST_EC_PATCH_FILE) || exit $$?; fi; exit $$result

host-package-cycle-test: | $(BUILD)
	g++ -std=c++20 -Wall -Wextra -Iplatform ../../tests/test_package_cycles.cpp platform/package.cpp -lz -o $(BUILD)/test_package_cycles
	$(BUILD)/test_package_cycles

host-bitmap-cache-test: host-renderer-software-test | $(BUILD)
	@cmake -S ../../upstream/okgf -B $(BUILD)/host-okgf -DOKGF_MATH_BACKEND=NATIVE >/dev/null && cmake --build $(BUILD)/host-okgf --parallel 2 >/dev/null && patched_text=0; if test "$$(uname -o)" = Msys; then git -C $(GAME_CPP_ROOT) apply --ignore-space-change $(HOST_EC_PATCH_FILE) || exit $$?; patched_text=1; fi; git -C $(GAME_CPP_ROOT) apply --ignore-space-change $(GR_MAIN_PORTABLE_PATCH_FILE) || exit $$?; patched_gr=1; git -C $(GAME_CPP_ROOT) apply --ignore-space-change $(GR_MAIN_ZLIB_PORTABLE_PATCH_FILE) || exit $$?; patched_zlib=1; g++ -std=c++20 -Wall -Wextra -Wpedantic -DSRHD_PORTABLE_RENDERER -ffunction-sections -fdata-sections -Iplatform -I$(GAME_CPP_ROOT)/src -I$(GAME_CPP_ROOT)/runtime -I../../upstream/okgf/include ../../tests/test_bitmap_cache_synthetic.cpp platform/okgf_bridge.cpp platform/renderer_platform.cpp platform/package.cpp platform/game_path.cpp platform/user_root.cpp platform/ec_file_adapter.cpp platform/windows_imports_portable.cpp platform/sysutils_imports_portable.cpp platform/runtime_time.cpp platform/runtime_failure_hooks_portable.cpp platform/zlib_bridge.cpp $(GAME_CPP_ROOT)/src/CrcUnit.cpp $(GAME_CPP_ROOT)/src/System.cpp $(GAME_CPP_ROOT)/src/SystemImports.cpp $(GAME_CPP_ROOT)/src/BlockParException.cpp $(GAME_CPP_ROOT)/src/GI_Main.cpp $(GAME_CPP_ROOT)/src/GlobalsV.cpp $(GAME_CPP_ROOT)/src/aMyFunction.cpp $(GAME_CPP_ROOT)/src/EC_File.cpp $(GAME_CPP_ROOT)/src/EC_Struct.cpp $(GAME_CPP_ROOT)/src/EC_Buf.cpp $(GAME_CPP_ROOT)/src/EC_Cache.cpp $(GAME_CPP_ROOT)/src/EC_CacheBitmap.cpp $(GAME_CPP_ROOT)/src/EC_BlockPar.cpp $(GAME_CPP_ROOT)/src/EC_Data.cpp $(GAME_CPP_ROOT)/src/EC_Str.cpp $(GAME_CPP_ROOT)/src/EC_Mem.cpp $(GAME_CPP_ROOT)/src/aPacket.cpp $(GAME_CPP_ROOT)/src/MessageText.cpp $(GAME_CPP_ROOT)/src/GR_GraphBuf.cpp $(GAME_CPP_ROOT)/src/GR_Main.cpp $(GAME_CPP_ROOT)/runtime/runtime.cpp $(BUILD)/host-renderer/pixels.o $(BUILD)/host-renderer/copy.o $(BUILD)/host-renderer/alpha_buffers.o $(BUILD)/host-renderer/gradients.o $(BUILD)/host-renderer/precision.o -Wl,--gc-sections -L$(BUILD)/host-okgf -lokgf -lpng -ljpeg -lz -lm -o $(BUILD)/test_bitmap_cache_synthetic; result=$$?; if test $$result -eq 0; then LD_LIBRARY_PATH=$(BUILD)/host-okgf:$$LD_LIBRARY_PATH $(BUILD)/test_bitmap_cache_synthetic; result=$$?; fi; if test $$patched_zlib -eq 1; then git -C $(GAME_CPP_ROOT) apply -R --ignore-space-change $(GR_MAIN_ZLIB_PORTABLE_PATCH_FILE) || exit $$?; fi; if test $$patched_gr -eq 1; then git -C $(GAME_CPP_ROOT) apply -R --ignore-space-change $(GR_MAIN_PORTABLE_PATCH_FILE) || exit $$?; fi; if test $$patched_text -eq 1; then git -C $(GAME_CPP_ROOT) apply -R --ignore-space-change $(HOST_EC_PATCH_FILE) || exit $$?; fi; exit $$result


host-bitmap-cache-release-test: | $(BUILD)
	@set -e; mkdir -p $(BUILD)/host-renderer; gcc -std=c11 -I../../upstream/okgf/include -I../../upstream/okgf/src -c ../../upstream/okgf/src/pixels.c -o $(BUILD)/host-renderer/pixels.o; gcc -std=c11 -I../../upstream/okgf/include -I../../upstream/okgf/src -c ../../upstream/okgf/src/copy.c -o $(BUILD)/host-renderer/copy.o; gcc -std=c11 -I../../upstream/okgf/include -I../../upstream/okgf/src -c ../../upstream/okgf/src/alpha_buffers.c -o $(BUILD)/host-renderer/alpha_buffers.o; gcc -std=c11 -I../../upstream/okgf/include -I../../upstream/okgf/src -c ../../upstream/okgf/src/gradients.c -o $(BUILD)/host-renderer/gradients.o; gcc -std=c11 -I../../upstream/okgf/include -I../../upstream/okgf/src -c ../../upstream/okgf/src/math/precision.c -o $(BUILD)/host-renderer/precision.o; cmake -S ../../upstream/okgf -B $(BUILD)/host-okgf -DOKGF_MATH_BACKEND=NATIVE >/dev/null; cmake --build $(BUILD)/host-okgf --parallel 2 >/dev/null; patched_text=0; if test "$$(uname -o)" = Msys; then git -C $(GAME_CPP_ROOT) apply --ignore-space-change $(HOST_EC_PATCH_FILE) || exit $$?; patched_text=1; fi; git -C $(GAME_CPP_ROOT) apply --ignore-space-change $(GR_MAIN_PORTABLE_PATCH_FILE) || exit $$?; patched_gr=1; git -C $(GAME_CPP_ROOT) apply --ignore-space-change $(GR_MAIN_ZLIB_PORTABLE_PATCH_FILE) || exit $$?; patched_zlib=1; g++ -std=c++20 -Wall -Wextra -Wpedantic -DSRHD_PORTABLE_RENDERER -ffunction-sections -fdata-sections -Iplatform -I$(GAME_CPP_ROOT)/src -I$(GAME_CPP_ROOT)/runtime -I../../upstream/okgf/include ../../tests/test_bitmap_cache_release.cpp platform/okgf_bridge.cpp platform/renderer_platform.cpp platform/runtime_platform.cpp platform/startup_slice.cpp platform/runtime_settings_slice.cpp platform/ui_metadata_slice.cpp platform/package.cpp platform/game_path.cpp platform/user_root.cpp platform/ec_file_adapter.cpp platform/windows_imports_portable.cpp platform/sysutils_imports_portable.cpp platform/runtime_time.cpp platform/runtime_failure_hooks_portable.cpp platform/zlib_bridge.cpp $(GAME_CPP_ROOT)/src/CrcUnit.cpp $(GAME_CPP_ROOT)/src/System.cpp $(GAME_CPP_ROOT)/src/SystemImports.cpp $(GAME_CPP_ROOT)/src/BlockParException.cpp $(GAME_CPP_ROOT)/src/GI_Main.cpp $(GAME_CPP_ROOT)/src/GlobalsV.cpp $(GAME_CPP_ROOT)/src/aMyFunction.cpp $(GAME_CPP_ROOT)/src/EC_File.cpp $(GAME_CPP_ROOT)/src/EC_Struct.cpp $(GAME_CPP_ROOT)/src/EC_Buf.cpp $(GAME_CPP_ROOT)/src/EC_Cache.cpp $(GAME_CPP_ROOT)/src/EC_CacheBitmap.cpp $(GAME_CPP_ROOT)/src/EC_BlockPar.cpp $(GAME_CPP_ROOT)/src/EC_Data.cpp $(GAME_CPP_ROOT)/src/EC_Str.cpp $(GAME_CPP_ROOT)/src/EC_Mem.cpp $(GAME_CPP_ROOT)/src/aPacket.cpp $(GAME_CPP_ROOT)/src/MessageText.cpp $(GAME_CPP_ROOT)/src/GR_GraphBuf.cpp $(GAME_CPP_ROOT)/src/GR_Main.cpp $(GAME_CPP_ROOT)/runtime/runtime.cpp $(BUILD)/host-renderer/pixels.o $(BUILD)/host-renderer/copy.o $(BUILD)/host-renderer/alpha_buffers.o $(BUILD)/host-renderer/gradients.o $(BUILD)/host-renderer/precision.o -Wl,--gc-sections -L$(BUILD)/host-okgf -lokgf -lpng -ljpeg -lz -lm -o $(BUILD)/test_bitmap_cache_release; result=$$?; if test $$result -eq 0; then LD_LIBRARY_PATH=$(BUILD)/host-okgf:$$LD_LIBRARY_PATH $(BUILD)/test_bitmap_cache_release "../../windows/Space Rangers HD A War Apart" "$(BUILD)/m13-release-user"; result=$$?; fi; if test $$patched_zlib -eq 1; then git -C $(GAME_CPP_ROOT) apply -R --ignore-space-change $(GR_MAIN_ZLIB_PORTABLE_PATCH_FILE) || exit $$?; fi; if test $$patched_gr -eq 1; then git -C $(GAME_CPP_ROOT) apply -R --ignore-space-change $(GR_MAIN_PORTABLE_PATCH_FILE) || exit $$?; fi; if test $$patched_text -eq 1; then git -C $(GAME_CPP_ROOT) apply -R --ignore-space-change $(HOST_EC_PATCH_FILE) || exit $$?; fi; exit $$result

host-milestone13-regression: host-milestone12-regression host-bitmap-cache-test
	@echo MILESTONE 13 HOST REGRESSION PASS


host-gi-format0-cpu-test: | $(BUILD)
	@cmake -S ../../upstream/okgf -B $(BUILD)/host-okgf-m14p-$(shell uname -s) -DOKGF_MATH_BACKEND=NATIVE >/dev/null && cmake --build $(BUILD)/host-okgf-m14p-$(shell uname -s) --parallel 2 >/dev/null && g++ -std=c++20 -Wall -Wextra -Wpedantic -Iplatform -I$(GAME_CPP_ROOT)/src -I$(GAME_CPP_ROOT)/runtime -I../../upstream/okgf/include ../../tests/test_gi_format0_cpu.cpp platform/gi_format0_cpu.cpp platform/gi_format0_okgf_bridge.cpp platform/okgf_bridge.cpp -L$(BUILD)/host-okgf-m14p-$(shell uname -s) -lokgf -lm -o $(BUILD)/test_gi_format0_cpu && LD_LIBRARY_PATH=$(BUILD)/host-okgf-m14p-$(shell uname -s):$$LD_LIBRARY_PATH $(BUILD)/test_gi_format0_cpu && ! nm -C $(BUILD)/test_gi_format0_cpu | grep -E 'GR_DX|Direct3D|TGraphBufGR|TCGiEC|GetOrCreateSurface|CreateTextureCache|GR_CreateTexture'


host-gi-format0-release-test: | $(BUILD)
	@patched=0; patched_zlib=0; git -C $(GAME_CPP_ROOT) apply --ignore-space-change $(GR_MAIN_PORTABLE_PATCH_FILE); patched=1; git -C $(GAME_CPP_ROOT) apply --ignore-space-change $(GR_MAIN_ZLIB_PORTABLE_PATCH_FILE); patched_zlib=1; g++ -std=c++20 -Wall -Wextra -Wpedantic -DSRHD_PORTABLE_RENDERER -DSRHD_PORTABLE_RUNTIME_SLICE -ffunction-sections -fdata-sections -Iplatform -I$(GAME_CPP_ROOT)/src -I$(GAME_CPP_ROOT)/runtime -I../../upstream/okgf/include ../../tests/test_gi_format0_release.cpp platform/gi_format0_cpu.cpp platform/gi_format0_okgf_bridge.cpp platform/okgf_bridge.cpp platform/runtime_platform.cpp platform/package.cpp platform/game_path.cpp platform/user_root.cpp platform/ec_file_adapter.cpp platform/windows_imports_portable.cpp platform/sysutils_imports_portable.cpp platform/runtime_time.cpp platform/runtime_failure_hooks_portable.cpp platform/runtime_settings_slice.cpp platform/startup_slice.cpp platform/zlib_bridge.cpp $(GAME_CPP_ROOT)/src/CrcUnit.cpp $(GAME_CPP_ROOT)/src/System.cpp $(GAME_CPP_ROOT)/src/SystemImports.cpp $(GAME_CPP_ROOT)/src/BlockParException.cpp $(GAME_CPP_ROOT)/src/GI_Main.cpp $(GAME_CPP_ROOT)/src/GlobalsV.cpp $(GAME_CPP_ROOT)/src/aMyFunction.cpp $(GAME_CPP_ROOT)/src/EC_File.cpp $(GAME_CPP_ROOT)/src/EC_Struct.cpp $(GAME_CPP_ROOT)/src/EC_Buf.cpp $(GAME_CPP_ROOT)/src/EC_Cache.cpp $(GAME_CPP_ROOT)/src/EC_BlockPar.cpp $(GAME_CPP_ROOT)/src/EC_Data.cpp $(GAME_CPP_ROOT)/src/EC_Str.cpp $(GAME_CPP_ROOT)/src/EC_Mem.cpp $(GAME_CPP_ROOT)/src/aPacket.cpp $(GAME_CPP_ROOT)/src/MessageText.cpp $(GAME_CPP_ROOT)/src/GR_Main.cpp $(GAME_CPP_ROOT)/runtime/runtime.cpp -Wl,--gc-sections -L$(BUILD)/host-okgf-m14p-$(shell uname -s) -lokgf -lz -lm -o $(BUILD)/test_gi_format0_release; result=$$?; if test $$result -eq 0; then LD_LIBRARY_PATH=$(BUILD)/host-okgf-m14p-$(shell uname -s):$$LD_LIBRARY_PATH $(BUILD)/test_gi_format0_release "../../windows/Space Rangers HD A War Apart" "$(BUILD)/m14p-release-user"; result=$$?; fi; if test $$patched_zlib -eq 1; then git -C $(GAME_CPP_ROOT) apply -R --ignore-space-change $(GR_MAIN_ZLIB_PORTABLE_PATCH_FILE) || exit $$?; fi; if test $$patched -eq 1; then git -C $(GAME_CPP_ROOT) apply -R --ignore-space-change $(GR_MAIN_PORTABLE_PATCH_FILE) || exit $$?; fi; exit $$result

host-gai-cpu-test: | $(BUILD)
	@cmake -S ../../upstream/okgf -B $(BUILD)/host-okgf-gai-$(shell uname -s) -DOKGF_MATH_BACKEND=NATIVE >/dev/null && cmake --build $(BUILD)/host-okgf-gai-$(shell uname -s) --parallel 2 >/dev/null && patched=0; if test "$$(uname -o)" = Msys; then git -C $(GAME_CPP_ROOT) apply --ignore-space-change $(HOST_EC_PATCH_FILE) || exit $$?; patched=1; fi; g++ -std=c++20 -Wall -Wextra -Wpedantic -ffunction-sections -fdata-sections -Iplatform -I$(GAME_CPP_ROOT)/src -I$(GAME_CPP_ROOT)/runtime -I../../upstream/okgf/include ../../tests/test_gai_cpu.cpp platform/gai_cpu.cpp platform/gi_format0_cpu.cpp platform/gi_format0_okgf_bridge.cpp platform/okgf_bridge.cpp platform/zlib_bridge.cpp -Wl,--gc-sections -L$(BUILD)/host-okgf-gai-$(shell uname -s) -lokgf -lz -lm -o $(BUILD)/test_gai_cpu; result=$$?; if test $$result -eq 0; then LD_LIBRARY_PATH=$(BUILD)/host-okgf-gai-$(shell uname -s):$$LD_LIBRARY_PATH $(BUILD)/test_gai_cpu; result=$$?; fi; if test $$result -eq 0; then ! nm -C $(BUILD)/test_gai_cpu | grep -E 'GR_DX|Direct3D|IDirect3DTexture9|TGraphBufGR|TgiGR|TCGaiEC|GI_GAIFile|GetOrCreateFrameSurface'; result=$$?; fi; if test $$patched -eq 1; then git -C $(GAME_CPP_ROOT) apply -R --ignore-space-change $(HOST_EC_PATCH_FILE) || exit $$?; fi; exit $$result

host-gi-format2-cpu-test: | $(BUILD)
	@mkdir -p $(BUILD)/host-gi-format2; gcc -std=c11 -I../../upstream/okgf/include -I../../upstream/okgf/src -c ../../upstream/okgf/src/rle.c -o $(BUILD)/host-gi-format2/rle.o; result=$$?; if test $$result -eq 0; then gcc -std=c11 -I../../upstream/okgf/include -I../../upstream/okgf/src -c ../../upstream/okgf/src/pixels.c -o $(BUILD)/host-gi-format2/pixels.o; result=$$?; fi; if test $$result -eq 0; then gcc -std=c11 -I../../upstream/okgf/include -I../../upstream/okgf/src -c ../../upstream/okgf/src/alpha_buffers.c -o $(BUILD)/host-gi-format2/alpha_buffers.o; result=$$?; fi; if test $$result -eq 0; then g++ -std=c++20 -Wall -Wextra -Wpedantic -Iplatform -I../../upstream/okgf/include ../../tests/test_gi_format2_cpu.cpp platform/gi_format2_cpu.cpp platform/okgf_rle_bridge.cpp $(BUILD)/host-gi-format2/rle.o $(BUILD)/host-gi-format2/pixels.o $(BUILD)/host-gi-format2/alpha_buffers.o -lm -o $(BUILD)/test_gi_format2_cpu; result=$$?; fi; if test $$result -eq 0; then $(BUILD)/test_gi_format2_cpu; result=$$?; fi; if test $$result -eq 0; then ! nm -C $(BUILD)/test_gi_format2_cpu | grep -E 'GR_DX|Direct3D|IDirect3DTexture9|TGraphBufGR|TgiGR|TCGiEC|TCGaiEC|GI_GAIFile'; result=$$?; fi; exit $$result

host-gai-playback-cpu-test: | $(BUILD)
	@g++ -std=c++20 -Wall -Wextra -Wpedantic -Iplatform ../../tests/test_gai_playback_cpu.cpp platform/gai_playback_cpu.cpp -o $(BUILD)/test_gai_playback_cpu && $(BUILD)/test_gai_playback_cpu && ! nm -C $(BUILD)/test_gai_playback_cpu | grep -E 'GR_DX|Direct3D|IDirect3DTexture9|TGraphBufGR|TgiGR|TCGiEC|GI_GAIFile|GI_GAI|GI_MessageLoop|SoundManager'

host-gai-sequence-cpu-test: | $(BUILD)
	@g++ -std=c++20 -Wall -Wextra -Wpedantic -Iplatform ../../tests/test_gai_sequence_cpu.cpp platform/gai_cpu.cpp -o $(BUILD)/test_gai_sequence_cpu && $(BUILD)/test_gai_sequence_cpu && ! nm -C $(BUILD)/test_gai_sequence_cpu | grep -E 'GR_DX|Direct3D|IDirect3DTexture9|TGraphBufGR|TgiGR|TCGiEC|GI_GAIFile|GI_GAI|GI_MessageLoop|SoundManager'

host-milestone17-regression: host-gai-sequence-cpu-test host-gai-playback-cpu-test
	@echo MILESTONE 17 HOST REGRESSION PASS

host-software-compositor-test: | $(BUILD)
	@g++ -std=c++20 -Wall -Wextra -Wpedantic -Iplatform ../../tests/test_software_compositor.cpp platform/software_compositor.cpp -o $(BUILD)/test_software_compositor && $(BUILD)/test_software_compositor && ! nm -C $(BUILD)/test_software_compositor | grep -E 'GR_DX|Direct3D|TGraphBufGR|TgiGR|TCGiEC|TCGaiEC|GI_GAI'

host-milestone18-regression: host-milestone17-regression host-software-compositor-test
	@echo MILESTONE 18 HOST REGRESSION PASS

host-scene-compositor-test: | $(BUILD)
	@g++ -std=c++20 -Wall -Wextra -Wpedantic -Iplatform ../../tests/test_scene_compositor.cpp platform/scene_compositor.cpp platform/software_compositor.cpp -o $(BUILD)/test_scene_compositor && $(BUILD)/test_scene_compositor && ! nm -C $(BUILD)/test_scene_compositor | grep -E 'GR_DX|Direct3D|TGraphBufGR|TgiGR|TCGiEC|TCGaiEC|GI_GAI|Forms'

host-milestone19-regression: host-milestone18-regression host-scene-compositor-test
	@echo MILESTONE 19 HOST REGRESSION PASS

host-gi-object-test: | $(BUILD)
	@mkdir -p $(BUILD)/host-gi-object
	@gcc -std=c11 -ffunction-sections -fdata-sections -I../../upstream/okgf/include -I../../upstream/okgf/src -c ../../upstream/okgf/src/rle.c -o $(BUILD)/host-gi-object/rle.o
	@gcc -std=c11 -ffunction-sections -fdata-sections -I../../upstream/okgf/include -I../../upstream/okgf/src -c ../../upstream/okgf/src/pixels.c -o $(BUILD)/host-gi-object/pixels.o
	@gcc -std=c11 -ffunction-sections -fdata-sections -I../../upstream/okgf/include -I../../upstream/okgf/src -c ../../upstream/okgf/src/alpha_buffers.c -o $(BUILD)/host-gi-object/alpha_buffers.o
	@patched=0; if test "$$(uname -o)" = Msys; then git -C $(GAME_CPP_ROOT) apply --ignore-space-change $(HOST_EC_PATCH_FILE) || exit $$?; patched=1; fi; g++ -std=c++20 -Wall -Wextra -Wpedantic -ffunction-sections -fdata-sections -Iplatform -I$(GAME_CPP_ROOT)/src -I$(GAME_CPP_ROOT)/runtime -I../../upstream/okgf/include ../../tests/test_gi_object.cpp platform/gi_object.cpp platform/gai_frame_sequence_cpu.cpp platform/package.cpp platform/gai_cpu.cpp platform/gai_playback_cpu.cpp platform/gi_format0_cpu.cpp platform/gi_format0_okgf_bridge.cpp platform/gi_format2_cpu.cpp platform/okgf_rle_bridge.cpp platform/scene_compositor.cpp platform/software_compositor.cpp platform/zlib_bridge.cpp $(BUILD)/host-gi-object/rle.o $(BUILD)/host-gi-object/pixels.o $(BUILD)/host-gi-object/alpha_buffers.o -Wl,--gc-sections -lz -lm -o $(BUILD)/test_gi_object; result=$$?; if test $$patched -eq 1; then git -C $(GAME_CPP_ROOT) apply -R --ignore-space-change $(HOST_EC_PATCH_FILE) || exit $$?; fi; exit $$result
	@$(BUILD)/test_gi_object
	@! nm -C $(BUILD)/test_gi_object | grep -E 'GR_DX|Direct3D|TGraphBufGR|TgiGR|TCGiEC|TCGaiEC|GI_GAI|Forms'

host-milestone20-regression: host-milestone19-regression host-gi-object-test

host-image-cpu-test: | $(BUILD)
	@mkdir -p $(BUILD)/host-image-cpu && gcc -std=c11 -Wall -Wextra -Wpedantic -I../../upstream/okgf/include -I../../upstream/okgf/src -c ../../upstream/okgf/src/bmp_read.c -o $(BUILD)/host-image-cpu/bmp_read.o && gcc -std=c11 -Wall -Wextra -Wpedantic -I../../upstream/okgf/include -I../../upstream/okgf/src -c ../../upstream/okgf/src/psd_read.c -o $(BUILD)/host-image-cpu/psd_read.o && gcc -std=c11 -Wall -Wextra -Wpedantic -I../../upstream/okgf/include -I../../upstream/okgf/src -c ../../upstream/okgf/src/image_read.c -o $(BUILD)/host-image-cpu/image_read.o && gcc -std=c11 -Wall -Wextra -Wpedantic -I../../upstream/okgf/include -c ../../tests/okgf_image_test_stubs.c -o $(BUILD)/host-image-cpu/stubs.o && g++ -std=c++20 -Wall -Wextra -Wpedantic -Iplatform -I../../upstream/okgf/include ../../tests/test_image_cpu.cpp platform/image_cpu.cpp platform/image_okgf_bridge.cpp $(BUILD)/host-image-cpu/bmp_read.o $(BUILD)/host-image-cpu/psd_read.o $(BUILD)/host-image-cpu/image_read.o $(BUILD)/host-image-cpu/stubs.o -o $(BUILD)/test_image_cpu && $(BUILD)/test_image_cpu && ! nm -C $(BUILD)/test_image_cpu | grep -E 'GR_DX|Direct3D|TGraphBufGR|TCBitmapEC|TCTBitmapEC|TCAlphaBitmapEC|TImageGI|Forms'
	@echo IMAGE CPU HOST TEST PASS

host-alpha-bitmap-cpu-test: | $(BUILD)
	@mkdir -p $(BUILD)/host-alpha-bitmap && gcc -std=c11 -Wall -Wextra -Wpedantic -I../../upstream/okgf/include -I../../upstream/okgf/src -c ../../upstream/okgf/src/bmp_read.c -o $(BUILD)/host-alpha-bitmap/bmp_read.o && gcc -std=c11 -Wall -Wextra -Wpedantic -I../../upstream/okgf/include -I../../upstream/okgf/src -c ../../upstream/okgf/src/psd_read.c -o $(BUILD)/host-alpha-bitmap/psd_read.o && gcc -std=c11 -Wall -Wextra -Wpedantic -I../../upstream/okgf/include -I../../upstream/okgf/src -c ../../upstream/okgf/src/image_read.c -o $(BUILD)/host-alpha-bitmap/image_read.o && gcc -std=c11 -Wall -Wextra -Wpedantic -I../../upstream/okgf/include -I../../upstream/okgf/src -c ../../upstream/okgf/src/rle.c -o $(BUILD)/host-alpha-bitmap/rle.o && gcc -std=c11 -Wall -Wextra -Wpedantic -I../../upstream/okgf/include -I../../upstream/okgf/src -c ../../upstream/okgf/src/pixels.c -o $(BUILD)/host-alpha-bitmap/pixels.o && gcc -std=c11 -Wall -Wextra -Wpedantic -I../../upstream/okgf/include -I../../upstream/okgf/src -c ../../upstream/okgf/src/alpha_buffers.c -o $(BUILD)/host-alpha-bitmap/alpha_buffers.o && gcc -std=c11 -Wall -Wextra -Wpedantic -I../../upstream/okgf/include -c ../../tests/okgf_image_test_stubs.c -o $(BUILD)/host-alpha-bitmap/stubs.o && g++ -std=c++20 -Wall -Wextra -Wpedantic -Iplatform -I../../upstream/okgf/include ../../tests/test_alpha_bitmap_cpu.cpp platform/alpha_bitmap_cpu.cpp platform/image_cpu.cpp platform/image_okgf_bridge.cpp platform/okgf_rle_bridge.cpp $(BUILD)/host-alpha-bitmap/bmp_read.o $(BUILD)/host-alpha-bitmap/psd_read.o $(BUILD)/host-alpha-bitmap/image_read.o $(BUILD)/host-alpha-bitmap/rle.o $(BUILD)/host-alpha-bitmap/pixels.o $(BUILD)/host-alpha-bitmap/alpha_buffers.o $(BUILD)/host-alpha-bitmap/stubs.o -o $(BUILD)/test_alpha_bitmap_cpu && $(BUILD)/test_alpha_bitmap_cpu && ! nm -C $(BUILD)/test_alpha_bitmap_cpu | grep -E 'GR_DX|Direct3D|TGraphBufGR|TCBitmapEC|TCTBitmapEC|TCAlphaBitmapEC|TImageGI|Forms'

host-trans-bitmap-cpu-test: host-alpha-bitmap-cpu-test | $(BUILD)
	@g++ -std=c++20 -Wall -Wextra -Wpedantic -Iplatform -I../../upstream/okgf/include ../../tests/test_trans_bitmap_cpu.cpp platform/trans_bitmap_cpu.cpp platform/alpha_bitmap_cpu.cpp platform/image_cpu.cpp platform/image_okgf_bridge.cpp platform/okgf_rle_bridge.cpp $(BUILD)/host-alpha-bitmap/bmp_read.o $(BUILD)/host-alpha-bitmap/psd_read.o $(BUILD)/host-alpha-bitmap/image_read.o $(BUILD)/host-alpha-bitmap/rle.o $(BUILD)/host-alpha-bitmap/pixels.o $(BUILD)/host-alpha-bitmap/alpha_buffers.o $(BUILD)/host-alpha-bitmap/stubs.o -o $(BUILD)/test_trans_bitmap_cpu && $(BUILD)/test_trans_bitmap_cpu && ! nm -C $(BUILD)/test_trans_bitmap_cpu | grep -E 'GR_DX|Direct3D|TGraphBufGR|TCBitmapEC|TCTBitmapEC|TCAlphaBitmapEC|TImageGI|Forms'

host-simple-bitmap-cpu-test: host-alpha-bitmap-cpu-test | $(BUILD)
	@g++ -std=c++20 -Wall -Wextra -Wpedantic -Iplatform -I../../upstream/okgf/include ../../tests/test_simple_bitmap_cpu.cpp platform/simple_bitmap_cpu.cpp platform/software_compositor.cpp platform/image_cpu.cpp platform/image_okgf_bridge.cpp $(BUILD)/host-alpha-bitmap/bmp_read.o $(BUILD)/host-alpha-bitmap/psd_read.o $(BUILD)/host-alpha-bitmap/image_read.o $(BUILD)/host-alpha-bitmap/rle.o $(BUILD)/host-alpha-bitmap/pixels.o $(BUILD)/host-alpha-bitmap/alpha_buffers.o $(BUILD)/host-alpha-bitmap/stubs.o -o $(BUILD)/test_simple_bitmap_cpu && $(BUILD)/test_simple_bitmap_cpu && ! nm -C $(BUILD)/test_simple_bitmap_cpu | grep -E 'GR_DX|Direct3D|TGraphBufGR|TCBitmapEC|TCTBitmapEC|TCAlphaBitmapEC|TImageGI|Forms'

host-image-layout-test: | $(BUILD)
	@g++ -std=c++20 -Wall -Wextra -Wpedantic -Iplatform ../../tests/test_image_layout.cpp platform/image_layout.cpp -o $(BUILD)/test_image_layout && $(BUILD)/test_image_layout && ! nm -C $(BUILD)/test_image_layout | grep -E 'GR_DX|Direct3D|TGraphBufGR|TCBitmapEC|TCTBitmapEC|TCAlphaBitmapEC|TImageGI|Forms'

host-image-object-test: host-alpha-bitmap-cpu-test | $(BUILD)
	@g++ -std=c++20 -Wall -Wextra -Wpedantic -Iplatform -I$(GAME_CPP_ROOT)/src -I$(GAME_CPP_ROOT)/runtime -I../../upstream/okgf/include ../../tests/test_image_object.cpp platform/image_object.cpp platform/gi_image_cpu.cpp platform/gi_format0_cpu.cpp platform/gi_format0_okgf_bridge.cpp platform/gi_format2_cpu.cpp platform/image_layout.cpp platform/simple_bitmap_cpu.cpp platform/trans_bitmap_cpu.cpp platform/alpha_bitmap_cpu.cpp platform/software_compositor.cpp platform/image_cpu.cpp platform/image_okgf_bridge.cpp platform/okgf_rle_bridge.cpp platform/package.cpp $(BUILD)/host-alpha-bitmap/bmp_read.o $(BUILD)/host-alpha-bitmap/psd_read.o $(BUILD)/host-alpha-bitmap/image_read.o $(BUILD)/host-alpha-bitmap/rle.o $(BUILD)/host-alpha-bitmap/pixels.o $(BUILD)/host-alpha-bitmap/alpha_buffers.o $(BUILD)/host-alpha-bitmap/stubs.o -lz -o $(BUILD)/test_image_object && $(BUILD)/test_image_object && ! nm -C $(BUILD)/test_image_object | grep -E 'GR_DX|Direct3D|TGraphBufGR|TCBitmapEC|TCTBitmapEC|TCAlphaBitmapEC|TImageGI|Forms'

host-m21-presentation-scene-test: host-alpha-bitmap-cpu-test | $(BUILD)
	@patched=0; if test "$$(uname -o)" = Msys; then git -C $(GAME_CPP_ROOT) apply --ignore-space-change $(HOST_EC_PATCH_FILE) || exit $$?; patched=1; fi; g++ -std=c++20 -Wall -Wextra -Wpedantic -ffunction-sections -fdata-sections -Iplatform -I$(GAME_CPP_ROOT)/src -I$(GAME_CPP_ROOT)/runtime -I../../upstream/okgf/include ../../tests/test_m21_presentation_scene.cpp platform/presentation_scene.cpp platform/image_object.cpp platform/gi_image_cpu.cpp platform/image_layout.cpp platform/simple_bitmap_cpu.cpp platform/trans_bitmap_cpu.cpp platform/alpha_bitmap_cpu.cpp platform/image_cpu.cpp platform/image_okgf_bridge.cpp platform/gi_object.cpp platform/gai_frame_sequence_cpu.cpp platform/package.cpp platform/gai_cpu.cpp platform/gai_playback_cpu.cpp platform/gi_format0_cpu.cpp platform/gi_format0_okgf_bridge.cpp platform/gi_format2_cpu.cpp platform/okgf_rle_bridge.cpp platform/scene_compositor.cpp platform/software_compositor.cpp platform/zlib_bridge.cpp $(BUILD)/host-alpha-bitmap/bmp_read.o $(BUILD)/host-alpha-bitmap/psd_read.o $(BUILD)/host-alpha-bitmap/image_read.o $(BUILD)/host-alpha-bitmap/rle.o $(BUILD)/host-alpha-bitmap/pixels.o $(BUILD)/host-alpha-bitmap/alpha_buffers.o $(BUILD)/host-alpha-bitmap/stubs.o -Wl,--gc-sections -lz -lm -o $(BUILD)/test_m21_presentation_scene; result=$$?; if test $$result -eq 0; then $(BUILD)/test_m21_presentation_scene; result=$$?; fi; if test $$result -eq 0; then ! nm -C $(BUILD)/test_m21_presentation_scene | grep -E 'GR_DX|Direct3D|TGraphBufGR|TCBitmapEC|TCTBitmapEC|TCAlphaBitmapEC|TImageGI|GI_MessageLoop|Forms'; result=$$?; fi; if test $$patched -eq 1; then git -C $(GAME_CPP_ROOT) apply -R --ignore-space-change $(HOST_EC_PATCH_FILE) || exit $$?; fi; exit $$result

host-m21-simple-release-test: | $(BUILD)
	@mkdir -p $(BUILD)/host-m21-simple-release && gcc -std=c11 -Wall -Wextra -Wpedantic -I../../upstream/okgf/include $$($$(command -v pkg-config >/dev/null && printf 'pkg-config --cflags libpng' || printf '')) -c ../../upstream/okgf/src/bmp_read.c -o $(BUILD)/host-m21-simple-release/bmp_read.o && gcc -std=c11 -Wall -Wextra -Wpedantic -I../../upstream/okgf/include $$($$(command -v pkg-config >/dev/null && printf 'pkg-config --cflags libpng' || printf '')) -c ../../upstream/okgf/src/psd_read.c -o $(BUILD)/host-m21-simple-release/psd_read.o && gcc -std=c11 -Wall -Wextra -Wpedantic -I../../upstream/okgf/include $$($$(command -v pkg-config >/dev/null && printf 'pkg-config --cflags libpng' || printf '')) -c ../../upstream/okgf/src/png_read.c -o $(BUILD)/host-m21-simple-release/png_read.o && gcc -std=c11 -Wall -Wextra -Wpedantic -I../../upstream/okgf/include $$($$(command -v pkg-config >/dev/null && printf 'pkg-config --cflags libpng' || printf '')) -c ../../upstream/okgf/src/image_read.c -o $(BUILD)/host-m21-simple-release/image_read.o && gcc -std=c11 -Wall -Wextra -Wpedantic -I../../upstream/okgf/include -c ../../tests/okgf_jpeg_test_stubs.c -o $(BUILD)/host-m21-simple-release/jpeg_stubs.o && g++ -std=c++20 -Wall -Wextra -Wpedantic -Iplatform -I../../upstream/okgf/include ../../tests/test_m21_simple_release.cpp platform/package.cpp platform/image_cpu.cpp platform/image_okgf_bridge.cpp $(BUILD)/host-m21-simple-release/bmp_read.o $(BUILD)/host-m21-simple-release/psd_read.o $(BUILD)/host-m21-simple-release/png_read.o $(BUILD)/host-m21-simple-release/image_read.o $(BUILD)/host-m21-simple-release/jpeg_stubs.o $$($$(command -v pkg-config >/dev/null && printf 'pkg-config --libs libpng' || printf '-lpng')) -lz -lm -o $(BUILD)/test_m21_simple_release && $(BUILD)/test_m21_simple_release "../space_rangers_hd_a_war_apart_port/windows/Space Rangers HD A War Apart/DATA/common.pkg" && ! nm -C $(BUILD)/test_m21_simple_release | grep -E 'GR_DX|Direct3D|TGraphBufGR|TCBitmapEC|TCTBitmapEC|TCAlphaBitmapEC|TImageGI|Forms'

host-m21-ui-image-regression: host-milestone20-regression host-image-cpu-test host-alpha-bitmap-cpu-test host-trans-bitmap-cpu-test host-simple-bitmap-cpu-test host-image-layout-test host-image-object-test host-m21-presentation-scene-test
	@echo M21 PORTABLE UI IMAGE FOUNDATION HOST REGRESSION PASS

host-ui-object-test: | $(BUILD)
	@g++ -std=c++20 -Wall -Wextra -Wpedantic -Iplatform ../../tests/test_ui_object.cpp platform/ui_object.cpp platform/ui_tree_fingerprint.cpp -o $(BUILD)/test_ui_object && $(BUILD)/test_ui_object && ! nm -C $(BUILD)/test_ui_object | grep -E 'GI_MessageLoop|TObjectGI|TPanelGI|GR_DX|Direct3D|TGraphBufGR|Forms'

host-m24-zone-test: | $(BUILD)
	@g++ -std=c++20 -Wall -Wextra -Wpedantic -Iplatform ../../tests/test_ui_zone.cpp platform/ui_object.cpp platform/ui_zone.cpp -o $(BUILD)/test_ui_zone && $(BUILD)/test_ui_zone && ! nm -C $(BUILD)/test_ui_zone | grep -E 'GI_MessageLoop|TObjectGI|TPanelGI|GR_DX|Direct3D|TGraphBufGR|Forms'

host-ui-config-test: host-alpha-bitmap-cpu-test host-m26-okgf | $(BUILD)
	@patched=0; if test "$$(uname -o)" = Msys; then git -C $(GAME_CPP_ROOT) apply --ignore-space-change $(HOST_EC_PATCH_FILE) || exit $$?; patched=1; fi; g++ -std=c++20 -Wall -Wextra -Wpedantic -ffunction-sections -fdata-sections -Iplatform -I$(GAME_CPP_ROOT)/src -I$(GAME_CPP_ROOT)/runtime -I../../upstream/okgf/include ../../tests/test_ui_config.cpp ../../tests/blockpar_test_support.cpp platform/ui_object.cpp platform/ui_zone.cpp platform/ui_graph_button.cpp platform/ui_window.cpp platform/ui_config.cpp platform/ui_graph_buffer.cpp platform/graph_buffer_cpu.cpp platform/ui_scroll_bar.cpp platform/ui_panel_scroll_bar.cpp platform/ui_edit.cpp platform/ui_tree_renderer.cpp platform/ui_gai.cpp platform/ui_label.cpp platform/tagged_text.cpp platform/font_renderer.cpp platform/aft_font.cpp platform/font_repository.cpp platform/image_object.cpp platform/gi_image_cpu.cpp platform/image_layout.cpp platform/simple_bitmap_cpu.cpp platform/trans_bitmap_cpu.cpp platform/alpha_bitmap_cpu.cpp platform/software_compositor.cpp platform/image_cpu.cpp platform/image_okgf_bridge.cpp platform/gi_object.cpp platform/gai_frame_sequence_cpu.cpp platform/package.cpp platform/gai_cpu.cpp platform/gai_playback_cpu.cpp platform/gi_format0_cpu.cpp platform/gi_format0_okgf_bridge.cpp platform/gi_format2_cpu.cpp platform/okgf_rle_bridge.cpp platform/scene_compositor.cpp platform/game_path.cpp platform/user_root.cpp platform/ec_file_adapter.cpp platform/windows_imports_portable.cpp platform/sysutils_imports_portable.cpp platform/runtime_time.cpp platform/runtime_failure_hooks_portable.cpp platform/zlib_bridge.cpp $(GAME_CPP_ROOT)/src/CrcUnit.cpp $(GAME_CPP_ROOT)/src/BlockParException.cpp $(GAME_CPP_ROOT)/src/EC_File.cpp $(GAME_CPP_ROOT)/src/EC_Struct.cpp $(GAME_CPP_ROOT)/src/EC_Buf.cpp $(GAME_CPP_ROOT)/src/EC_BlockPar.cpp $(GAME_CPP_ROOT)/src/EC_Str.cpp $(GAME_CPP_ROOT)/src/EC_Mem.cpp $(GAME_CPP_ROOT)/src/aPacket.cpp $(GAME_CPP_ROOT)/runtime/runtime.cpp $(BUILD)/host-alpha-bitmap/bmp_read.o $(BUILD)/host-alpha-bitmap/psd_read.o $(BUILD)/host-alpha-bitmap/image_read.o $(BUILD)/host-alpha-bitmap/rle.o $(BUILD)/host-alpha-bitmap/pixels.o $(BUILD)/host-alpha-bitmap/alpha_buffers.o $(BUILD)/host-alpha-bitmap/stubs.o -Wl,--gc-sections -L$(BUILD)/host-okgf -lokgf -lpng -ljpeg -lz -lm -o $(BUILD)/test_ui_config; result=$$?; if test $$result -eq 0; then LD_LIBRARY_PATH=$(BUILD)/host-okgf $(BUILD)/test_ui_config; result=$$?; fi; if test $$result -eq 0; then ! grep -n -E 'GI_MessageLoop|TObjectGI|TPanelGI|GR_DX|Direct3D|TGraphBufGR|Forms' platform/ui_object.* platform/ui_config.*; result=$$?; fi; if test $$patched -eq 1; then git -C $(GAME_CPP_ROOT) apply -R --ignore-space-change $(HOST_EC_PATCH_FILE) || exit $$?; fi; exit $$result

host-ui-tree-render-test: host-alpha-bitmap-cpu-test | $(BUILD)
	@patched=0; if test "$$(uname -o)" = Msys; then git -C $(GAME_CPP_ROOT) apply --ignore-space-change $(HOST_EC_PATCH_FILE) || exit $$?; patched=1; fi; g++ -std=c++20 -Wall -Wextra -Wpedantic -ffunction-sections -fdata-sections -Iplatform -I$(GAME_CPP_ROOT)/src -I$(GAME_CPP_ROOT)/runtime -I../../upstream/okgf/include ../../tests/test_ui_tree_render.cpp platform/ui_object.cpp platform/ui_graph_button.cpp platform/ui_window.cpp platform/ui_tree_renderer.cpp platform/ui_gai.cpp platform/ui_tree_fingerprint.cpp platform/image_object.cpp platform/gi_image_cpu.cpp platform/image_layout.cpp platform/simple_bitmap_cpu.cpp platform/trans_bitmap_cpu.cpp platform/alpha_bitmap_cpu.cpp platform/software_compositor.cpp platform/image_cpu.cpp platform/image_okgf_bridge.cpp platform/gi_object.cpp platform/gai_frame_sequence_cpu.cpp platform/package.cpp platform/gai_cpu.cpp platform/gai_playback_cpu.cpp platform/gi_format0_cpu.cpp platform/gi_format0_okgf_bridge.cpp platform/gi_format2_cpu.cpp platform/okgf_rle_bridge.cpp platform/zlib_bridge.cpp platform/scene_compositor.cpp $(BUILD)/host-alpha-bitmap/bmp_read.o $(BUILD)/host-alpha-bitmap/psd_read.o $(BUILD)/host-alpha-bitmap/image_read.o $(BUILD)/host-alpha-bitmap/rle.o $(BUILD)/host-alpha-bitmap/pixels.o $(BUILD)/host-alpha-bitmap/alpha_buffers.o $(BUILD)/host-alpha-bitmap/stubs.o -Wl,--gc-sections -lz -lm -o $(BUILD)/test_ui_tree_render; result=$$?; if test $$result -eq 0; then $(BUILD)/test_ui_tree_render; result=$$?; fi; if test $$result -eq 0; then ! nm -C $(BUILD)/test_ui_tree_render | grep -E 'GI_MessageLoop|TObjectGI|TPanelGI|GR_DX|Direct3D|TGraphBufGR|Forms'; result=$$?; fi; if test $$patched -eq 1; then git -C $(GAME_CPP_ROOT) apply -R --ignore-space-change $(HOST_EC_PATCH_FILE) || exit $$?; fi; exit $$result

host-m22-ui-regression: host-ui-object-test host-ui-config-test host-ui-tree-render-test
	@echo M22 PORTABLE UI OBJECT LAYOUT REGRESSION PASS

host-aft-font-test: | $(BUILD)
	@g++ -std=c++20 -Wall -Wextra -Wpedantic -Iplatform ../../tests/test_aft_font.cpp platform/aft_font.cpp -o $(BUILD)/test_aft_font && $(BUILD)/test_aft_font

host-font-renderer-test: host-alpha-bitmap-cpu-test | $(BUILD)
	@g++ -std=c++20 -Wall -Wextra -Wpedantic -Iplatform -I../../upstream/okgf/include ../../tests/test_font_renderer.cpp platform/aft_font.cpp platform/font_renderer.cpp platform/okgf_rle_bridge.cpp $(BUILD)/host-alpha-bitmap/rle.o $(BUILD)/host-alpha-bitmap/pixels.o $(BUILD)/host-alpha-bitmap/alpha_buffers.o -o $(BUILD)/test_font_renderer && $(BUILD)/test_font_renderer

host-tagged-text-test: host-alpha-bitmap-cpu-test | $(BUILD)
	@g++ -std=c++20 -Wall -Wextra -Wpedantic -Iplatform -I../../upstream/okgf/include ../../tests/test_tagged_text.cpp platform/aft_font.cpp platform/tagged_text.cpp platform/font_renderer.cpp platform/okgf_rle_bridge.cpp $(BUILD)/host-alpha-bitmap/rle.o $(BUILD)/host-alpha-bitmap/pixels.o $(BUILD)/host-alpha-bitmap/alpha_buffers.o -o $(BUILD)/test_tagged_text && $(BUILD)/test_tagged_text

# M23 Label/config assertions share the M22 factory executable and its upstream stubs.
host-ui-label-test: host-ui-config-test
	@echo M23 UI LABEL HOST TEST PASS

host-m23-ui-text-test: host-alpha-bitmap-cpu-test | $(BUILD)
	@patched=0; if test "$$(uname -o)" = Msys; then git -C $(GAME_CPP_ROOT) apply --ignore-space-change $(HOST_EC_PATCH_FILE) || exit $$?; patched=1; fi; g++ -std=c++20 -Wall -Wextra -Wpedantic -ffunction-sections -fdata-sections -Iplatform -I../../tests -I$(GAME_CPP_ROOT)/src -I$(GAME_CPP_ROOT)/runtime -I../../upstream/okgf/include ../../tests/test_m23_ui_text.cpp platform/ui_object.cpp platform/ui_tree_renderer.cpp platform/ui_gai.cpp platform/ui_tree_fingerprint.cpp platform/ui_label.cpp platform/tagged_text.cpp platform/font_renderer.cpp platform/aft_font.cpp platform/image_object.cpp platform/gi_image_cpu.cpp platform/image_layout.cpp platform/simple_bitmap_cpu.cpp platform/trans_bitmap_cpu.cpp platform/alpha_bitmap_cpu.cpp platform/software_compositor.cpp platform/image_cpu.cpp platform/image_okgf_bridge.cpp platform/gi_object.cpp platform/gai_frame_sequence_cpu.cpp platform/package.cpp platform/gai_cpu.cpp platform/gai_playback_cpu.cpp platform/gi_format0_cpu.cpp platform/gi_format0_okgf_bridge.cpp platform/gi_format2_cpu.cpp platform/okgf_rle_bridge.cpp platform/zlib_bridge.cpp platform/scene_compositor.cpp $(BUILD)/host-alpha-bitmap/bmp_read.o $(BUILD)/host-alpha-bitmap/psd_read.o $(BUILD)/host-alpha-bitmap/image_read.o $(BUILD)/host-alpha-bitmap/rle.o $(BUILD)/host-alpha-bitmap/pixels.o $(BUILD)/host-alpha-bitmap/alpha_buffers.o $(BUILD)/host-alpha-bitmap/stubs.o -Wl,--gc-sections -lz -lm -o $(BUILD)/test_m23_ui_text; result=$$?; if test $$result -eq 0; then $(BUILD)/test_m23_ui_text; result=$$?; fi; if test $$patched -eq 1; then git -C $(GAME_CPP_ROOT) apply -R --ignore-space-change $(HOST_EC_PATCH_FILE) || exit $$?; fi; exit $$result

host-m26-edit-test: host-alpha-bitmap-cpu-test | $(BUILD)
	@patched=0; if test "$$(uname -o)" = Msys; then git -C $(GAME_CPP_ROOT) apply --ignore-space-change $(HOST_EC_PATCH_FILE) || exit $$?; patched=1; fi; g++ -std=c++20 -Wall -Wextra -Wpedantic -ffunction-sections -fdata-sections -Iplatform -I../../tests -I$(GAME_CPP_ROOT)/src -I$(GAME_CPP_ROOT)/runtime -I../../upstream/okgf/include ../../tests/test_m26_edit.cpp platform/ui_object.cpp platform/ui_tree_renderer.cpp platform/ui_gai.cpp platform/ui_tree_fingerprint.cpp platform/ui_label.cpp platform/ui_edit.cpp platform/tagged_text.cpp platform/font_renderer.cpp platform/aft_font.cpp platform/image_object.cpp platform/gi_image_cpu.cpp platform/image_layout.cpp platform/simple_bitmap_cpu.cpp platform/trans_bitmap_cpu.cpp platform/alpha_bitmap_cpu.cpp platform/software_compositor.cpp platform/image_cpu.cpp platform/image_okgf_bridge.cpp platform/gi_object.cpp platform/gai_frame_sequence_cpu.cpp platform/package.cpp platform/gai_cpu.cpp platform/gai_playback_cpu.cpp platform/gi_format0_cpu.cpp platform/gi_format0_okgf_bridge.cpp platform/gi_format2_cpu.cpp platform/okgf_rle_bridge.cpp platform/zlib_bridge.cpp platform/scene_compositor.cpp $(BUILD)/host-alpha-bitmap/bmp_read.o $(BUILD)/host-alpha-bitmap/psd_read.o $(BUILD)/host-alpha-bitmap/image_read.o $(BUILD)/host-alpha-bitmap/rle.o $(BUILD)/host-alpha-bitmap/pixels.o $(BUILD)/host-alpha-bitmap/alpha_buffers.o $(BUILD)/host-alpha-bitmap/stubs.o -Wl,--gc-sections -lz -lm -o $(BUILD)/test_m26_edit; result=$$?; if test $$result -eq 0; then $(BUILD)/test_m26_edit; result=$$?; fi; if test $$patched -eq 1; then git -C $(GAME_CPP_ROOT) apply -R --ignore-space-change $(HOST_EC_PATCH_FILE) || exit $$?; fi; exit $$result

host-m24-ui-controls-test: host-alpha-bitmap-cpu-test | $(BUILD)
	@patched=0; if test "$$(uname -o)" = Msys; then git -C $(GAME_CPP_ROOT) apply --ignore-space-change $(HOST_EC_PATCH_FILE) || exit $$?; patched=1; fi; g++ -std=c++20 -Wall -Wextra -Wpedantic -ffunction-sections -fdata-sections -Iplatform -I../../tests -I$(GAME_CPP_ROOT)/src -I$(GAME_CPP_ROOT)/runtime -I../../upstream/okgf/include ../../tests/test_m24_ui_controls.cpp platform/ui_object.cpp platform/ui_graph_button.cpp platform/ui_window.cpp platform/ui_zone.cpp platform/ui_controls_fingerprint.cpp platform/ui_controls_checkpoint.cpp platform/ui_tree_renderer.cpp platform/ui_gai.cpp platform/ui_tree_fingerprint.cpp platform/ui_label.cpp platform/tagged_text.cpp platform/font_renderer.cpp platform/aft_font.cpp platform/image_object.cpp platform/gi_image_cpu.cpp platform/image_layout.cpp platform/simple_bitmap_cpu.cpp platform/trans_bitmap_cpu.cpp platform/alpha_bitmap_cpu.cpp platform/software_compositor.cpp platform/image_cpu.cpp platform/image_okgf_bridge.cpp platform/gi_object.cpp platform/gai_frame_sequence_cpu.cpp platform/package.cpp platform/gai_cpu.cpp platform/gai_playback_cpu.cpp platform/gi_format0_cpu.cpp platform/gi_format0_okgf_bridge.cpp platform/gi_format2_cpu.cpp platform/okgf_rle_bridge.cpp platform/zlib_bridge.cpp platform/scene_compositor.cpp $(BUILD)/host-alpha-bitmap/bmp_read.o $(BUILD)/host-alpha-bitmap/psd_read.o $(BUILD)/host-alpha-bitmap/image_read.o $(BUILD)/host-alpha-bitmap/rle.o $(BUILD)/host-alpha-bitmap/pixels.o $(BUILD)/host-alpha-bitmap/alpha_buffers.o $(BUILD)/host-alpha-bitmap/stubs.o -Wl,--gc-sections -lz -lm -o $(BUILD)/test_m24_ui_controls; result=$$?; if test $$result -eq 0; then $(BUILD)/test_m24_ui_controls; result=$$?; fi; if test $$patched -eq 1; then git -C $(GAME_CPP_ROOT) apply -R --ignore-space-change $(HOST_EC_PATCH_FILE) || exit $$?; fi; exit $$result

host-m23-ui-regression: host-m22-ui-regression host-aft-font-test host-font-renderer-test host-tagged-text-test host-ui-label-test host-m23-ui-text-test
	@echo M23 PORTABLE AFT TEXT LABEL HOST REGRESSION PASS

host-m24-ui-regression: host-m23-ui-regression host-m24-zone-test host-m24-ui-controls-test
	@echo M24 PORTABLE UI CONTROLS HOST REGRESSION PASS

host-m25-ui-regression: host-m24-ui-regression host-gi-image-test host-ui-gai-test
	@echo M25 PORTABLE GI GAI UI HOST REGRESSION PASS

host-ui-gai-test: host-alpha-bitmap-cpu-test | $(BUILD)
	@g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -ffunction-sections -fdata-sections -Iplatform -I$(GAME_CPP_ROOT)/src -I$(GAME_CPP_ROOT)/runtime -I../../upstream/okgf/include ../../tests/test_m25_ui_gai.cpp platform/ui_object.cpp platform/ui_tree_renderer.cpp platform/ui_gai.cpp platform/ui_tree_fingerprint.cpp platform/ui_graph_button.cpp platform/ui_window.cpp platform/ui_zone.cpp platform/image_object.cpp platform/gi_image_cpu.cpp platform/image_layout.cpp platform/simple_bitmap_cpu.cpp platform/trans_bitmap_cpu.cpp platform/alpha_bitmap_cpu.cpp platform/software_compositor.cpp platform/image_cpu.cpp platform/image_okgf_bridge.cpp platform/gi_object.cpp platform/gai_frame_sequence_cpu.cpp platform/package.cpp platform/gai_cpu.cpp platform/gai_playback_cpu.cpp platform/gi_format0_cpu.cpp platform/gi_format0_okgf_bridge.cpp platform/gi_format2_cpu.cpp platform/okgf_rle_bridge.cpp platform/zlib_bridge.cpp platform/scene_compositor.cpp $(BUILD)/host-alpha-bitmap/bmp_read.o $(BUILD)/host-alpha-bitmap/psd_read.o $(BUILD)/host-alpha-bitmap/image_read.o $(BUILD)/host-alpha-bitmap/rle.o $(BUILD)/host-alpha-bitmap/pixels.o $(BUILD)/host-alpha-bitmap/alpha_buffers.o $(BUILD)/host-alpha-bitmap/stubs.o -Wl,--gc-sections -lz -lm -o $(BUILD)/test_m25_ui_gai && $(BUILD)/test_m25_ui_gai

host-ui-gai-release-test: host-alpha-bitmap-cpu-test | $(BUILD)
	@g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -ffunction-sections -fdata-sections -Iplatform -I$(GAME_CPP_ROOT)/src -I$(GAME_CPP_ROOT)/runtime -I../../upstream/okgf/include ../../tests/test_m25_gai_release.cpp platform/ui_object.cpp platform/ui_tree_renderer.cpp platform/ui_gai.cpp platform/ui_tree_fingerprint.cpp platform/ui_graph_button.cpp platform/ui_window.cpp platform/ui_zone.cpp platform/image_object.cpp platform/gi_image_cpu.cpp platform/image_layout.cpp platform/simple_bitmap_cpu.cpp platform/trans_bitmap_cpu.cpp platform/alpha_bitmap_cpu.cpp platform/software_compositor.cpp platform/image_cpu.cpp platform/image_okgf_bridge.cpp platform/gi_object.cpp platform/gai_frame_sequence_cpu.cpp platform/package.cpp platform/gai_cpu.cpp platform/gai_playback_cpu.cpp platform/gi_format0_cpu.cpp platform/gi_format0_okgf_bridge.cpp platform/gi_format2_cpu.cpp platform/okgf_rle_bridge.cpp platform/zlib_bridge.cpp platform/scene_compositor.cpp $(BUILD)/host-alpha-bitmap/bmp_read.o $(BUILD)/host-alpha-bitmap/psd_read.o $(BUILD)/host-alpha-bitmap/image_read.o $(BUILD)/host-alpha-bitmap/rle.o $(BUILD)/host-alpha-bitmap/pixels.o $(BUILD)/host-alpha-bitmap/alpha_buffers.o $(BUILD)/host-alpha-bitmap/stubs.o -Wl,--gc-sections -lz -lm -o $(BUILD)/test_m25_gai_release && $(BUILD)/test_m25_gai_release "$(GAME_ROOT)"

host-m25-controls-release-test: host-alpha-bitmap-cpu-test | $(BUILD)
	@g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -ffunction-sections -fdata-sections -Iplatform -I$(GAME_CPP_ROOT)/src -I$(GAME_CPP_ROOT)/runtime -I../../upstream/okgf/include ../../tests/test_m25_controls_release.cpp platform/ui_object.cpp platform/ui_tree_renderer.cpp platform/ui_gai.cpp platform/ui_tree_fingerprint.cpp platform/ui_graph_button.cpp platform/ui_window.cpp platform/ui_zone.cpp platform/image_object.cpp platform/gi_image_cpu.cpp platform/image_layout.cpp platform/simple_bitmap_cpu.cpp platform/trans_bitmap_cpu.cpp platform/alpha_bitmap_cpu.cpp platform/software_compositor.cpp platform/image_cpu.cpp platform/image_okgf_bridge.cpp platform/gi_object.cpp platform/gai_frame_sequence_cpu.cpp platform/package.cpp platform/gai_cpu.cpp platform/gai_playback_cpu.cpp platform/gi_format0_cpu.cpp platform/gi_format0_okgf_bridge.cpp platform/gi_format2_cpu.cpp platform/okgf_rle_bridge.cpp platform/zlib_bridge.cpp platform/scene_compositor.cpp $(BUILD)/host-alpha-bitmap/bmp_read.o $(BUILD)/host-alpha-bitmap/psd_read.o $(BUILD)/host-alpha-bitmap/image_read.o $(BUILD)/host-alpha-bitmap/rle.o $(BUILD)/host-alpha-bitmap/pixels.o $(BUILD)/host-alpha-bitmap/alpha_buffers.o $(BUILD)/host-alpha-bitmap/stubs.o -Wl,--gc-sections -lz -lm -o $(BUILD)/test_m25_controls_release && $(BUILD)/test_m25_controls_release "$(GAME_ROOT)"
