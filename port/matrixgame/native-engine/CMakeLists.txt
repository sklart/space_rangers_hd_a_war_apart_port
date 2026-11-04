cmake_minimum_required(VERSION 3.20)
project(MatrixGameNativeEngine LANGUAGES CXX)
include(CTest)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
get_filename_component(PORT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
include("${CMAKE_CURRENT_LIST_DIR}/../deko/materials.cmake")
set(ENGINE_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/source")
file(GLOB_RECURSE ENGINE_INPUTS CONFIGURE_DEPENDS
    "${PORT_ROOT}/upstream/matrixgame/MatrixGame/*.cpp"
    "${PORT_ROOT}/upstream/matrixgame/MatrixGame/*.h"
    "${PORT_ROOT}/upstream/matrixgame/MatrixGame/*.hpp"
    "${PORT_ROOT}/upstream/matrixgame/MatrixLib/*.cpp"
    "${PORT_ROOT}/upstream/matrixgame/MatrixLib/*.h"
    "${PORT_ROOT}/upstream/matrixgame/MatrixLib/*.hpp")
file(GLOB ENGINE_PREPARERS CONFIGURE_DEPENDS "${PORT_ROOT}/tools/matrixgame_*prepare.py"
    "${PORT_ROOT}/tools/matrixgame_*overrides.py")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${ENGINE_INPUTS} ${ENGINE_PREPARERS})
execute_process(COMMAND "${Python3_EXECUTABLE}" "${PORT_ROOT}/tools/matrixgame_engine_prepare.py"
    --output "${ENGINE_SOURCE}" RESULT_VARIABLE SOURCE_RESULT)
if(NOT SOURCE_RESULT EQUAL 0)
    message(FATAL_ERROR "Native engine source preparation failed")
endif()
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../native-core" "${CMAKE_CURRENT_BINARY_DIR}/core")
if(CORE_SANITIZERS)
    target_compile_options(matrixgame-material-combiner PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(matrixgame-material-combiner PUBLIC -fsanitize=address,undefined)
endif()
# Explicit original VectorObject CPU subset, not a full engine/GPU target.
add_library(matrixgame-native-vector-object STATIC "${CMAKE_CURRENT_BINARY_DIR}/core/source/VectorObjectCpu.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/core/source/VectorObjectAnimNative.cpp")
target_link_libraries(matrixgame-native-vector-object PUBLIC matrixgame-native-core)
add_library(matrixgame-native-cache STATIC
    native_diagnostic_journal.cpp
    "${ENGINE_SOURCE}/MatrixGame/src/OriginalTextureUnions.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/core/source/CacheNative.cpp" cache_resources.cpp cache_owner_state.cpp platform_globals.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/core/source/L3GCpuPhase.cpp" init_runtime.cpp original_game.cpp cached_frame.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/core/source/3GPlatformNative.cpp" platform_3g.cpp map_device_backend.cpp debug_font_backend.cpp native_debug_font.cpp offscreen_backend.cpp offscreen_scope.cpp native_surfaces.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/core/source/3GDeviceDefaults.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/source/MatrixLib/3G/DeviceState.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/source/MatrixGame/src/GameDefaultsNative.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/source/MatrixGame/src/MatrixConfig.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/source/MatrixGame/src/MatrixGame.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixLogic.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixMap.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixTerSurface.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixMapTexture.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixWater.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixSkinManager.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Effects/MatrixEffect.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Effects/MatrixEffectPointLight.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Effects/MatrixEffectSmokeAndFire.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Effects/MatrixEffectExplosion.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Effects/MatrixEffectKonus.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Effects/MatrixEffectLandscapeSpot.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Effects/MatrixEffectWeapon.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Effects/MatrixEffectSelection.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Effects/MatrixEffectMovingObject.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Effects/MatrixEffectPath.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Effects/MatrixEffectFirePlasma.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Effects/MatrixEffectFlame.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Effects/MatrixEffectBigBoom.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Effects/MatrixEffectLightening.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Effects/MatrixEffectRepair.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Effects/MatrixEffectShleif.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixMapTrace.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixObjectRobot.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixRobot.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixObjectBuilding.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixObjectCannon.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Logic/MatrixEnvironment.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Logic/MatrixAIGroup.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixSide.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Interface/CConstructor.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Interface/CInterface.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Interface/CIFaceMenu.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Interface/CIFaceStatic.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Interface/CIFaceImage.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Interface/CAnimation.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Interface/CCounter.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Interface/CHistory.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixMultiSelection.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Effects/MatrixEffectMoveTo.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Effects/MatrixEffectDust.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Interface/CIFaceElement.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Interface/CIFaceButton.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Effects/MatrixEffectBillboard.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Effects/MatrixEffectElevatorField.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Effects/MatrixEffectZahvat.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixFlyer.cpp"
    "${ENGINE_SOURCE}/MatrixLib/3G/CBillboard.cpp"
    "${ENGINE_SOURCE}/MatrixLib/3G/OriginalVectorAnim.cpp"
    "${ENGINE_SOURCE}/MatrixLib/3G/OriginalVectorGroup.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixMapGroup.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/OriginalMapPrepare.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixVisiCalc.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixGamePathUtils.cpp" output_paths_backend.cpp
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixMapStatic.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixMinimap.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Interface/MatrixHint.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixObject.cpp"
    "${ENGINE_SOURCE}/MatrixLib/3G/ShadowStencil.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixProgressBar.cpp"
    "${ENGINE_SOURCE}/MatrixLib/3G/ShadowProj.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixShadowManager.cpp"
    "${ENGINE_SOURCE}/MatrixLib/3G/OriginalShadowProjection.cpp"
    "${ENGINE_SOURCE}/MatrixLib/3G/OriginalShadowTexture.cpp" shadow_texture_backend.cpp
    "${ENGINE_SOURCE}/MatrixLib/3G/OriginalVectorPicking.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/DevConsole.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixSoundManager.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixDebugInfo.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/OriginalTransition.cpp" transition_backend.cpp native_screenshot.cpp
    "${ENGINE_SOURCE}/MatrixGame/src/input.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/source/MatrixGame/src/GameStaticResources.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/source/MatrixGame/src/MatrixLoadProgress.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixRenderPipeline.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/OriginalRendererOps.cpp"
    "${ENGINE_SOURCE}/MatrixLib/3G/OriginalManagedTexture.cpp" legacy_texture_backend.cpp native_texture.cpp native_device_state.cpp native_stored_state.cpp
    "${ENGINE_SOURCE}/MatrixLib/3G/OriginalUnmanagedTexture.cpp" unmanaged_texture_backend.cpp
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixCursor.cpp" cursor_backend.cpp native_cursor.cpp
    "${ENGINE_SOURCE}/MatrixGame/src/Text/Font.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Text/Parser.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Text/Render.cpp" text_backend.cpp native_text.cpp native_text_backend.cpp
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixCamera.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixInstantDraw.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/source/MatrixGame/src/MatrixSampleStateManager.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/core/source/SkinResourcesNative.cpp" native_skin.cpp skin_pipeline.cpp)
target_link_libraries(matrixgame-native-cache PUBLIC matrixgame-native-vector-object matrixgame-material-combiner)
# Compile the complete original CGame TU; link only the original methods whose
# dependencies are actually closed. This is not a full-engine link claim.
set_property(SOURCE "${ENGINE_SOURCE}/MatrixGame/src/MatrixGame.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Text/Font.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Text/Parser.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Text/Render.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/text_backend.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixCursor.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/cursor_backend.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixCamera.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixInstantDraw.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/offscreen_backend.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/unmanaged_texture_backend.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/cache_owner_state.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/platform_globals.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/init_runtime.cpp"
    "${ENGINE_SOURCE}/MatrixLib/3G/OriginalManagedTexture.cpp"
    "${ENGINE_SOURCE}/MatrixLib/3G/OriginalUnmanagedTexture.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixLogic.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixMap.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixTerSurface.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixMapTexture.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixWater.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixSkinManager.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Effects/MatrixEffect.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Effects/MatrixEffectPointLight.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Effects/MatrixEffectSmokeAndFire.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Effects/MatrixEffectExplosion.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Effects/MatrixEffectKonus.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Effects/MatrixEffectLandscapeSpot.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Effects/MatrixEffectWeapon.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Effects/MatrixEffectSelection.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Effects/MatrixEffectMovingObject.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Effects/MatrixEffectFirePlasma.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Effects/MatrixEffectPath.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixVisiCalc.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Effects/MatrixEffectFlame.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Effects/MatrixEffectBigBoom.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Effects/MatrixEffectLightening.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Effects/MatrixEffectRepair.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Effects/MatrixEffectShleif.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixMapTrace.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixObjectRobot.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixRobot.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixObjectBuilding.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixObjectCannon.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Logic/MatrixEnvironment.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Logic/MatrixAIGroup.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixSide.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Interface/CConstructor.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Interface/CInterface.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Interface/CIFaceMenu.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Interface/CIFaceStatic.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Interface/CIFaceImage.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Interface/CAnimation.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Interface/CCounter.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Interface/CHistory.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixMultiSelection.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Effects/MatrixEffectMoveTo.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Effects/MatrixEffectDust.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Interface/CIFaceElement.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Interface/CIFaceButton.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Effects/MatrixEffectBillboard.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Effects/MatrixEffectElevatorField.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Effects/MatrixEffectZahvat.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixFlyer.cpp"
    "${ENGINE_SOURCE}/MatrixLib/3G/CBillboard.cpp"
    "${ENGINE_SOURCE}/MatrixLib/3G/OriginalVectorAnim.cpp"
    "${ENGINE_SOURCE}/MatrixLib/3G/OriginalVectorGroup.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixMapGroup.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/OriginalMapPrepare.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixGamePathUtils.cpp" output_paths_backend.cpp
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixMapStatic.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixMinimap.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Interface/MatrixHint.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixObject.cpp"
    "${ENGINE_SOURCE}/MatrixLib/3G/ShadowStencil.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixProgressBar.cpp"
    "${ENGINE_SOURCE}/MatrixLib/3G/ShadowProj.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixShadowManager.cpp"
    "${ENGINE_SOURCE}/MatrixLib/3G/OriginalShadowProjection.cpp"
    "${ENGINE_SOURCE}/MatrixLib/3G/OriginalShadowTexture.cpp"
    "${ENGINE_SOURCE}/MatrixLib/3G/OriginalVectorPicking.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/shadow_texture_backend.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/DevConsole.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixSoundManager.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixDebugInfo.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/OriginalTransition.cpp" transition_backend.cpp native_screenshot.cpp
    "${ENGINE_SOURCE}/MatrixGame/src/Logic/MatrixRoadNetwork.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixRenderPipeline.cpp" APPEND PROPERTY COMPILE_OPTIONS
    "$<$<CXX_COMPILER_ID:MSVC>:/Gy>" "$<$<CXX_COMPILER_ID:MSVC>:/Gw>"
    "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-ffunction-sections>"
    "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-fdata-sections>")
# UBSan vptr metadata would retain RTTI for unported map-object key functions
# even when their original methods are discarded. The skin callback table also
# retains callbacks inspecting those real, still-unported object types. The
# effects factory metadata similarly references unported concrete effects;
# group object queries reference the unported static-object/robot RTTI.
# Static-object dispatch references concrete robot/building/cannon RTTI too.
# Full minimap rendering also references those still-unported concrete types.
# Full map-object logic references unported robot and weapon-effect RTTI.
# Do not fabricate RTTI. Keep address and the other undefined-behavior checks.
set_property(SOURCE "${ENGINE_SOURCE}/MatrixGame/src/MatrixLogic.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixMap.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixSkinManager.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixMapGroup.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixMapStatic.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixMinimap.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixObject.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/Effects/MatrixEffect.cpp" APPEND PROPERTY COMPILE_OPTIONS
    "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-fno-sanitize=vptr>")

# GCC ASan registers the original console's global command table even when
# Keyboard/ShowHelp are unused; that registration roots every command handler
# and its still-unported map dependencies. Keep the whole TU, all command
# bodies, Address stack/heap checks and UBSan. Only ASan global redzones for
# this TU are excluded from the addressed bootstrap, not from the game logic.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_FLAGS MATCHES "-fsanitize=[^ ]*address")
    set_property(SOURCE "${ENGINE_SOURCE}/MatrixGame/src/DevConsole.cpp"
        APPEND PROPERTY COMPILE_OPTIONS "--param=asan-globals=0")
endif()
target_link_options(matrixgame-native-cache INTERFACE
    "$<$<CXX_COMPILER_ID:MSVC>:/OPT:REF>"
    "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wl,--gc-sections>")
target_include_directories(matrixgame-native-cache PUBLIC "${CMAKE_CURRENT_LIST_DIR}")
target_include_directories(matrixgame-native-cache BEFORE PUBLIC
    "${ENGINE_SOURCE}/MatrixLib/3G" "${ENGINE_SOURCE}/MatrixLib/Base"
    "${ENGINE_SOURCE}/MatrixLib/Bitmap" "${ENGINE_SOURCE}/MatrixGame/src")
target_include_directories(matrixgame-native-cache PRIVATE "${ENGINE_SOURCE}/MatrixGame")
if(WIN32)
    enable_language(RC)
    set(MATRIXGAME_D3DX9_ROOT "" CACHE PATH "Root of the official Microsoft.DXSDK.D3DX package (or legacy SDK)")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(D3DX9_ARCH x64)
    else()
        set(D3DX9_ARCH x86)
    endif()
    find_path(MATRIXGAME_D3DX9_INCLUDE d3dx9core.h
        HINTS "${MATRIXGAME_D3DX9_ROOT}" ENV DXSDK_DIR
        PATH_SUFFIXES build/native/include Include)
    find_library(MATRIXGAME_D3DX9_LIBRARY NAMES d3dx9
        HINTS "${MATRIXGAME_D3DX9_ROOT}" ENV DXSDK_DIR
        PATH_SUFFIXES "build/native/release/lib/${D3DX9_ARCH}" "Lib/${D3DX9_ARCH}")
    if(NOT MATRIXGAME_D3DX9_INCLUDE OR NOT MATRIXGAME_D3DX9_LIBRARY)
        message(FATAL_ERROR "Original debug font requires Microsoft D3DX9 headers/library; set MATRIXGAME_D3DX9_ROOT")
    endif()
    target_include_directories(matrixgame-native-cache PRIVATE "${MATRIXGAME_D3DX9_INCLUDE}")
    target_sources(matrixgame-native-cache PRIVATE platform_win32.cpp)
    target_sources(matrixgame-native-cache PRIVATE "${ENGINE_SOURCE}/MatrixLib/3G/OriginalWindowsLoop.cpp")
    target_link_libraries(matrixgame-native-cache PUBLIC d3d9 user32 gdi32 winmm shell32 ole32 "${MATRIXGAME_D3DX9_LIBRARY}")
endif()
if(BUILD_TESTING OR MATRIXGAME_INIT_BUILD_LINK_PROBE)
    add_executable(matrixgame-native-bootstrap "${PORT_ROOT}/tests/test_matrixgame_bootstrap.cpp"
        "${PORT_ROOT}/tests/matrixgame_bootstrap_sampling.cpp"
        "${PORT_ROOT}/tests/matrixgame_bootstrap_font.cpp"
        "${PORT_ROOT}/tests/matrixgame_bootstrap_formatted_texture.cpp"
        "${PORT_ROOT}/tests/matrixgame_bootstrap_game.cpp"
        "${PORT_ROOT}/tests/matrixgame_bootstrap_progress_bar.cpp"
        "${PORT_ROOT}/tests/matrixgame_bootstrap_shadow_proj.cpp"
        "${PORT_ROOT}/tests/matrixgame_bootstrap_shadow_manager.cpp"
        "${PORT_ROOT}/tests/matrixgame_bootstrap_shadow_texture.cpp"
        "${PORT_ROOT}/tests/matrixgame_bootstrap_smoke_fire.cpp"
        "${PORT_ROOT}/tests/matrixgame_bootstrap_explosion.cpp"
        "${PORT_ROOT}/tests/matrixgame_bootstrap_selection.cpp"
        "${PORT_ROOT}/tests/matrixgame_bootstrap_callback_width.cpp"
        "${PORT_ROOT}/tests/matrixgame_bootstrap_cannon_units.cpp"
        "${PORT_ROOT}/tests/matrixgame_bootstrap_vector_group.cpp"
        "${PORT_ROOT}/tests/matrixgame_bootstrap_ai_owners.cpp")
    target_link_libraries(matrixgame-native-bootstrap PRIVATE matrixgame-native-cache)
    target_compile_definitions(matrixgame-native-bootstrap PRIVATE
        MATRIXGAME_FULL_VECTOR_HEADER="${ENGINE_SOURCE}/MatrixLib/3G/VectorObject.hpp"
        MATRIXGAME_FULL_BILLBOARD_HEADER="${ENGINE_SOURCE}/MatrixLib/3G/CBillboard.hpp")
    target_compile_definitions(matrixgame-native-bootstrap PRIVATE
        MATRIXGAME_FULL_3G_HEADER="${ENGINE_SOURCE}/MatrixLib/3G/3g.hpp")
    target_compile_definitions(matrixgame-native-bootstrap PRIVATE
        MATRIXGAME_FULL_CONFIG_HEADER="${ENGINE_SOURCE}/MatrixGame/src/MatrixConfig.hpp"
        MATRIXGAME_FULL_CAMERA_HEADER="${ENGINE_SOURCE}/MatrixGame/src/MatrixCamera.hpp"
        MATRIXGAME_FULL_KONUS_HEADER="${ENGINE_SOURCE}/MatrixGame/src/Effects/MatrixEffectKonus.hpp")
    target_compile_definitions(matrixgame-native-bootstrap PRIVATE
        MATRIXGAME_FULL_CURSOR_HEADER="${ENGINE_SOURCE}/MatrixGame/src/MatrixCursor.hpp")
    if(WIN32)
        target_sources(matrixgame-native-bootstrap PRIVATE "${PORT_ROOT}/tests/test_matrixgame_bootstrap_win32.cpp"
            "${PORT_ROOT}/tests/matrixgame_bootstrap_offscreen.cpp"
            "${PORT_ROOT}/tests/matrixgame_bootstrap_unmanaged_texture.cpp")
        target_include_directories(matrixgame-native-bootstrap PRIVATE "${MATRIXGAME_D3DX9_INCLUDE}")
        target_compile_options(matrixgame-native-bootstrap PRIVATE /EHsc)
    endif()
    target_compile_options(matrixgame-native-bootstrap PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
    add_executable(matrixgame-original-init "${PORT_ROOT}/tests/test_matrixgame_original_init.cpp")
    target_link_libraries(matrixgame-original-init PRIVATE matrixgame-native-cache)
    target_compile_options(matrixgame-original-init PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/EHsc> $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
    if(WIN32)
        add_executable(matrixgame-native-shadow-texture-hal
            "${PORT_ROOT}/tests/matrixgame_shadow_texture_hal_main.cpp"
            "${PORT_ROOT}/tests/matrixgame_bootstrap_shadow_texture_gpu.cpp"
            "${PORT_ROOT}/tests/matrixgame_box_probe_sdk.cpp")
        target_link_libraries(matrixgame-native-shadow-texture-hal PRIVATE matrixgame-native-cache)
        target_include_directories(matrixgame-native-shadow-texture-hal PRIVATE "${MATRIXGAME_D3DX9_INCLUDE}")
        target_compile_options(matrixgame-native-shadow-texture-hal PRIVATE /EHsc /UNDEBUG)
    endif()
endif()
option(MATRIXGAME_INIT_BUILD_LINK_PROBE "Link the native init/cache contract and real deko upload; no NRO/runtime claim" OFF)
if(MATRIXGAME_DEVKITPRO)
    set(MATRIXGAME_DEBUG_FONT_ASSET "" CACHE FILEPATH "Exact original MS Sans Serif 25 D3DX bake (MGDF25A1); no substitute font")
    if(NOT EXISTS "${MATRIXGAME_DEBUG_FONT_ASSET}")
        message(FATAL_ERROR "Native debug font needs its original D3DX bake: set MATRIXGAME_DEBUG_FONT_ASSET; tools/matrixgame_font_checks.py windows exports and verifies it")
    endif()
    find_program(MATRIXGAME_FONT_OBJCOPY aarch64-none-elf-objcopy HINTS "${MATRIXGAME_DEVKITPRO}/devkitA64/bin" REQUIRED)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/debug-font")
    add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/debug-font/original-font.o"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${MATRIXGAME_DEBUG_FONT_ASSET}" original-font.bin
        COMMAND "${MATRIXGAME_FONT_OBJCOPY}" -I binary -O elf64-littleaarch64 -B aarch64
            --rename-section .data=.rodata,alloc,load,readonly,data,contents original-font.bin original-font.o
        DEPENDS "${MATRIXGAME_DEBUG_FONT_ASSET}" WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/debug-font" VERBATIM)
    set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/debug-font/original-font.o" PROPERTIES GENERATED TRUE EXTERNAL_OBJECT TRUE)
    target_sources(matrixgame-native-cache PRIVATE native_debug_font_asset.cpp "${CMAKE_CURRENT_BINARY_DIR}/debug-font/original-font.o")
    set(MATRIXGAME_TEXT_FONT_ASSETS "" CACHE PATH "Original D3DX Rangers10/Verdana12-14 UI bakes (MGTEXT01)")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/text-font")
    foreach(id RANGE 0 3)
        if(NOT EXISTS "${MATRIXGAME_TEXT_FONT_ASSETS}/font${id}.bin")
            message(FATAL_ERROR "Original UI text requires verified D3DX font assets: set MATRIXGAME_TEXT_FONT_ASSETS")
        endif()
        add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/text-font/font${id}.o"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${MATRIXGAME_TEXT_FONT_ASSETS}/font${id}.bin" font${id}.bin
            COMMAND "${MATRIXGAME_FONT_OBJCOPY}" -I binary -O elf64-littleaarch64 -B aarch64
                --rename-section .data=.rodata,alloc,load,readonly,data,contents font${id}.bin font${id}.o
            DEPENDS "${MATRIXGAME_TEXT_FONT_ASSETS}/font${id}.bin" WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/text-font" VERBATIM)
        set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/text-font/font${id}.o" PROPERTIES GENERATED TRUE EXTERNAL_OBJECT TRUE)
        target_sources(matrixgame-native-cache PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/text-font/font${id}.o")
    endforeach()
    target_sources(matrixgame-native-cache PRIVATE native_text_asset.cpp)
    target_sources(matrixgame-native-cache PRIVATE platform_switch.cpp native_rangers_canvas.cpp native_diagnostic_smoke.cpp applet_window_lifecycle.cpp applet_engine_events.cpp
        native_input_switch.cpp native_input_engine.cpp native_input.cpp native_form_backend.cpp native_form_session.cpp
        native_loop.cpp native_loop_switch.cpp
        "${ENGINE_SOURCE}/MatrixLib/3G/OriginalNativeLoop.cpp"
        "${ENGINE_SOURCE}/MatrixGame/src/OriginalNativeForm.cpp")
    target_include_directories(matrixgame-native-cache PRIVATE "${MATRIXGAME_DEVKITPRO}/libnx/include")
    target_link_directories(matrixgame-native-cache PUBLIC "${MATRIXGAME_DEVKITPRO}/libnx/lib")
    # materials.cmake selects the SDK once for every native consumer.
    target_link_libraries(matrixgame-native-cache PUBLIC ${MATRIXGAME_DEKO_LIBRARY} nx)
    # Full original Init/form/loop target, separate from the minimal bootstrap.
    # Font creation/draw happens through the original map debug-info path;
    # standalone font diagnostics are not mandatory steps of the game launcher.
    add_executable(matrixgame-native-entry EXCLUDE_FROM_ALL "${PORT_ROOT}/tests/probe_matrixgame_native_entry.cpp")
    option(MATRIXGAME_DIAGNOSTIC_RANGERS_HOST "Bind real owning Rangers services in the separate diagnostic" OFF)
    set(DIAGNOSTIC_RANGERS_METADATA_ARGS)
    if(MATRIXGAME_DIAGNOSTIC_RANGERS_HOST)
        set(BRIDGE_BUILD_HOST_LINK_PROBE OFF CACHE BOOL "Separate host-only link probe" FORCE)
        set(BRIDGE_BUILD_LINK_PROBE OFF CACHE BOOL "Separate bridge-only link probe" FORCE)
        foreach(input BRIDGE_RANGERS_HOST_ARCHIVE BRIDGE_RANGERS_OKGF_ARCHIVE BRIDGE_RANGERS_SOFTFLOAT_ARCHIVE MATRIXGAME_RANGERS_DEPENDENCY_MANIFEST)
            if(NOT EXISTS "${${input}}")
                message(FATAL_ERROR "Real diagnostic host requires verified ${input}; use tools/matrixgame_hosted_diagnostic_sdk.py")
            endif()
        endforeach()
        add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../native" "${CMAKE_CURRENT_BINARY_DIR}/rangers")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${BRIDGE_RANGERS_HOST_ARCHIVE}"
            "${BRIDGE_RANGERS_OKGF_ARCHIVE}" "${BRIDGE_RANGERS_SOFTFLOAT_ARCHIVE}" "${MATRIXGAME_RANGERS_DEPENDENCY_MANIFEST}")
        list(APPEND DIAGNOSTIC_RANGERS_METADATA_ARGS --rangers-host
            --rangers-archive "${BRIDGE_RANGERS_HOST_ARCHIVE}" --rangers-archive "${BRIDGE_RANGERS_OKGF_ARCHIVE}"
            --rangers-archive "${BRIDGE_RANGERS_SOFTFLOAT_ARCHIVE}" --rangers-manifest "${MATRIXGAME_RANGERS_DEPENDENCY_MANIFEST}")
    endif()
    file(GLOB_RECURSE DIAGNOSTIC_SOURCE_INPUTS CONFIGURE_DEPENDS
        "${PORT_ROOT}/port/matrixgame/*.cpp" "${PORT_ROOT}/port/matrixgame/*.hpp"
        "${PORT_ROOT}/port/matrixgame/*.h" "${PORT_ROOT}/port/matrixgame/*.inc"
        "${PORT_ROOT}/port/matrixgame/*.inl" "${PORT_ROOT}/port/matrixgame/*.glsl"
        "${PORT_ROOT}/port/matrixgame/*.cmake" "${PORT_ROOT}/port/matrixgame/*.txt")
    file(GLOB DIAGNOSTIC_TOOLS CONFIGURE_DEPENDS "${PORT_ROOT}/tools/matrixgame_*.py")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${DIAGNOSTIC_SOURCE_INPUTS}
        ${DIAGNOSTIC_TOOLS} "${PORT_ROOT}/tests/probe_matrixgame_native_entry.cpp"
        "${PORT_ROOT}/tests/matrixgame_native_diagnostic_main.cpp")
    string(TOUPPER "${CMAKE_BUILD_TYPE}" DIAGNOSTIC_CONFIG_UPPER)
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${PORT_ROOT}/tools/matrixgame_diagnostic_metadata.py"
        --engine-source "${ENGINE_SOURCE}" --devkitpro "${MATRIXGAME_DEVKITPRO}"
        --deko-library "${MATRIXGAME_DEKO_LIBRARY}"
        "--build-type=${CMAKE_BUILD_TYPE}"
        "--cxx-flags=${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${DIAGNOSTIC_CONFIG_UPPER}}"
        "--c-flags=${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_${DIAGNOSTIC_CONFIG_UPPER}}"
        ${DIAGNOSTIC_RANGERS_METADATA_ARGS}
        --output "${CMAKE_CURRENT_BINARY_DIR}/native-diagnostic-build.hpp"
        RESULT_VARIABLE DIAGNOSTIC_METADATA_RESULT)
    if(NOT DIAGNOSTIC_METADATA_RESULT EQUAL 0)
        message(FATAL_ERROR "Diagnostic source fingerprint preparation failed")
    endif()
    target_include_directories(matrixgame-native-entry PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
    target_include_directories(matrixgame-native-entry PRIVATE "${MATRIXGAME_DEVKITPRO}/libnx/include")
    target_link_libraries(matrixgame-native-entry PRIVATE matrixgame-native-cache)
    target_link_options(matrixgame-native-entry PRIVATE "-specs=${MATRIXGAME_DEVKITPRO}/libnx/switch.specs" -Wl,--gc-sections)
    set_target_properties(matrixgame-native-entry PROPERTIES SUFFIX ".elf")
    function(matrixgame_diagnostic_entry target map_name)
        add_executable(${target} EXCLUDE_FROM_ALL "${PORT_ROOT}/tests/matrixgame_native_diagnostic_main.cpp")
        target_include_directories(${target} PRIVATE "${MATRIXGAME_DEVKITPRO}/libnx/include" "${CMAKE_CURRENT_BINARY_DIR}")
        target_link_libraries(${target} PRIVATE matrixgame-native-cache)
        if(MATRIXGAME_DIAGNOSTIC_RANGERS_HOST)
            target_compile_definitions(${target} PRIVATE MATRIXGAME_DIAGNOSTIC_RANGERS_HOST=1)
            target_include_directories(${target} PRIVATE "${PORT_ROOT}/port/switch/platform")
            target_link_directories(${target} PRIVATE "${MATRIXGAME_DEVKITPRO}/portlibs/switch/lib")
            target_link_libraries(${target} PRIVATE -Wl,--start-group
                matrixgame-native-cache matrixgame-engine-callbacks matrixgame-rangers-bridge matrixgame-native-core
                "${BRIDGE_RANGERS_HOST_ARCHIVE}" "${BRIDGE_RANGERS_OKGF_ARCHIVE}" "${BRIDGE_RANGERS_SOFTFLOAT_ARCHIVE}"
                png z jpeg vorbisfile vorbis ogg m SDL2 EGL glapi drm_nouveau nx pthread -Wl,--end-group)
            target_link_options(${target} PRIVATE "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${map_name}")
        endif()
        target_link_options(${target} PRIVATE "-specs=${MATRIXGAME_DEVKITPRO}/libnx/switch.specs" -Wl,--gc-sections)
        set_target_properties(${target} PROPERTIES SUFFIX ".elf")
    endfunction()
    matrixgame_diagnostic_entry(matrixgame-native-diagnostic hosted-diagnostic.map)
    matrixgame_diagnostic_entry(matrixgame-native-interactive hosted-interactive.map)
    # Route SDL2's default-window reference to this entry's real VI owner.
    # This prevents default_window.o's automatic second display at startup.
    target_link_options(matrixgame-native-diagnostic PRIVATE -Wl,--wrap=nwindowGetDefault)
    target_link_options(matrixgame-native-interactive PRIVATE -Wl,--wrap=nwindowGetDefault)
    target_compile_definitions(matrixgame-native-interactive PRIVATE MATRIXGAME_DIAGNOSTIC_MANUAL=1)
    # SDK packaging is opt-in through an explicit target, never part of ALL or
    # the main game NRO. Keep every generated file inside this binary directory.
    option(MATRIXGAME_DIAGNOSTIC_NRO "Expose the standalone diagnostic NRO target" ON)
    if(MATRIXGAME_DIAGNOSTIC_NRO)
        find_program(MATRIXGAME_NACPTOOL NAMES nacptool nacptool.exe
            PATHS "${MATRIXGAME_DEVKITPRO}/tools/bin" NO_DEFAULT_PATH REQUIRED)
        find_program(MATRIXGAME_ELF2NRO NAMES elf2nro elf2nro.exe
            PATHS "${MATRIXGAME_DEVKITPRO}/tools/bin" NO_DEFAULT_PATH REQUIRED)
        function(matrixgame_diagnostic_package entry package name title)
        set(DIAGNOSTIC_PACKAGE_DIR "${CMAKE_CURRENT_BINARY_DIR}/${package}")
        set(DIAGNOSTIC_NACP "${DIAGNOSTIC_PACKAGE_DIR}/control.nacp")
        set(DIAGNOSTIC_NRO "${DIAGNOSTIC_PACKAGE_DIR}/${name}.nro")
        set(DIAGNOSTIC_ICON "${MATRIXGAME_DEVKITPRO}/libnx/default_icon.jpg")
        add_custom_command(OUTPUT "${DIAGNOSTIC_NACP}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${DIAGNOSTIC_PACKAGE_DIR}"
            COMMAND "${MATRIXGAME_NACPTOOL}" --create "${title}"
                "SRHD AWA port" "0.1.0" "${DIAGNOSTIC_NACP}"
            DEPENDS "${MATRIXGAME_NACPTOOL}" "${CMAKE_CURRENT_LIST_FILE}" VERBATIM)
        add_custom_command(OUTPUT "${DIAGNOSTIC_NRO}"
            COMMAND "${MATRIXGAME_ELF2NRO}" "$<TARGET_FILE:${entry}>"
                "${DIAGNOSTIC_NRO}" "--icon=${DIAGNOSTIC_ICON}" "--nacp=${DIAGNOSTIC_NACP}"
            DEPENDS ${entry} "${DIAGNOSTIC_NACP}"
                "${DIAGNOSTIC_ICON}" "${MATRIXGAME_ELF2NRO}" VERBATIM)
        add_custom_target(${entry}-nro DEPENDS "${DIAGNOSTIC_NRO}")
        endfunction()
        matrixgame_diagnostic_package(matrixgame-native-diagnostic diagnostic-nro matrixgame-native-diagnostic "MatrixGame r34 Quick Smoke")
        matrixgame_diagnostic_package(matrixgame-native-interactive interactive-nro matrixgame-native-interactive "MatrixGame r34 Interactive")
    endif()
endif()
if(NOT WIN32)
    target_sources(matrixgame-native-cache PRIVATE native_frame_clock.cpp)
endif()
if(BUILD_TESTING AND NOT MATRIXGAME_DEVKITPRO)
    add_executable(matrixgame-rangers-canvas-pixels "${PORT_ROOT}/tests/test_matrixgame_rangers_canvas.cpp")
    target_compile_features(matrixgame-rangers-canvas-pixels PRIVATE cxx_std_20)
    target_compile_options(matrixgame-rangers-canvas-pixels PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
    add_test(NAME matrixgame-rangers-canvas-pixels COMMAND matrixgame-rangers-canvas-pixels)
    if(NOT WIN32)
        add_executable(matrixgame-native-bc1 "${PORT_ROOT}/tests/test_matrixgame_bc1.cpp"
            "${PORT_ROOT}/tests/matrixgame_allocation_fault.cpp")
        target_link_libraries(matrixgame-native-bc1 PRIVATE matrixgame-native-cache)
        target_compile_options(matrixgame-native-bc1 PRIVATE -UNDEBUG)
        set(MATRIXGAME_BC1_UNION_DIR "" CACHE PATH "Six original BuildTexUnions DDS observations from the Windows HAL reference")
        if(EXISTS "${MATRIXGAME_BC1_UNION_DIR}/union-5.dds")
            add_test(NAME matrixgame-native-bc1 COMMAND matrixgame-native-bc1
                "${MATRIXGAME_BC1_UNION_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/bc1-test-output")
            set_tests_properties(matrixgame-native-bc1 PROPERTIES FIXTURES_SETUP matrixgame-bc1-data)
            add_executable(matrixgame-native-original-unions "${PORT_ROOT}/tests/test_matrixgame_original_texture_unions.cpp")
            target_link_libraries(matrixgame-native-original-unions PRIVATE matrixgame-native-cache)
            target_compile_options(matrixgame-native-original-unions PRIVATE -UNDEBUG)
            add_test(NAME matrixgame-native-original-unions COMMAND matrixgame-native-original-unions
                "${PORT_ROOT}/windows/Space Rangers HD A War Apart/DATA/robots.pkg"
                "${MATRIXGAME_BC1_UNION_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/bc1-test-output")
            set_tests_properties(matrixgame-native-original-unions PROPERTIES FIXTURES_REQUIRED matrixgame-bc1-data)
        endif()
        add_executable(matrixgame-native-bc1-owner "${PORT_ROOT}/tests/test_matrixgame_bc1_owner_main.cpp"
            "${PORT_ROOT}/tests/matrixgame_bc1_original_owner.cpp")
        target_link_libraries(matrixgame-native-bc1-owner PRIVATE matrixgame-native-cache)
        target_compile_options(matrixgame-native-bc1-owner PRIVATE -UNDEBUG)
        add_test(NAME matrixgame-native-bc1-owner COMMAND matrixgame-native-bc1-owner)
        add_executable(matrixgame-native-map-queries "${PORT_ROOT}/tests/test_matrixgame_native_map_queries.cpp")
        target_link_libraries(matrixgame-native-map-queries PRIVATE matrixgame-native-cache)
        target_compile_options(matrixgame-native-map-queries PRIVATE -UNDEBUG)
        add_test(NAME matrixgame-native-map-queries COMMAND matrixgame-native-map-queries)
        add_executable(matrixgame-native-debug-font "${PORT_ROOT}/tests/test_matrixgame_native_font.cpp")
        target_link_libraries(matrixgame-native-debug-font PRIVATE matrixgame-native-cache)
        target_compile_options(matrixgame-native-debug-font PRIVATE -UNDEBUG)
        set(MATRIXGAME_DEBUG_FONT_GOLDENS "" CACHE FILEPATH "Windows HAL goldens for the original font")
        if(EXISTS "${MATRIXGAME_DEBUG_FONT_ASSET}" AND EXISTS "${MATRIXGAME_DEBUG_FONT_GOLDENS}")
            add_test(NAME matrixgame-native-debug-font COMMAND matrixgame-native-debug-font "${MATRIXGAME_DEBUG_FONT_ASSET}" "${MATRIXGAME_DEBUG_FONT_GOLDENS}")
        endif()
    endif()
    add_executable(matrixgame-native-cursor-input "${PORT_ROOT}/tests/test_matrixgame_native_cursor_input.cpp"
        native_cursor.cpp native_input.cpp native_input_engine.cpp)
    target_link_libraries(matrixgame-native-cursor-input PRIVATE matrixgame-native-cache)
    target_compile_options(matrixgame-native-cursor-input PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
    add_test(NAME matrixgame-native-cursor-input COMMAND matrixgame-native-cursor-input)
    add_executable(matrixgame-native-applet-lifecycle "${PORT_ROOT}/tests/test_matrixgame_applet_window.cpp"
        applet_window_lifecycle.cpp applet_engine_events.cpp)
    target_link_libraries(matrixgame-native-applet-lifecycle PRIVATE matrixgame-native-cache)
    target_compile_options(matrixgame-native-applet-lifecycle PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
    add_test(NAME matrixgame-native-applet-lifecycle COMMAND matrixgame-native-applet-lifecycle)
    add_executable(matrixgame-native-state-contract "${PORT_ROOT}/tests/test_matrixgame_native_state.cpp"
        native_device_state.cpp "${CMAKE_CURRENT_LIST_DIR}/../deko/fixed_vertex.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/../deko/material_combiner.cpp")
    target_include_directories(matrixgame-native-state-contract PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
    target_link_libraries(matrixgame-native-state-contract PRIVATE matrixgame-native-core)
    target_compile_options(matrixgame-native-state-contract PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
    add_test(NAME matrixgame-native-state-contract COMMAND matrixgame-native-state-contract)
    add_executable(matrixgame-texture-residency-contract
        "${PORT_ROOT}/tests/test_matrixgame_texture_residency.cpp" native_texture.cpp platform_3g.cpp)
    target_include_directories(matrixgame-texture-residency-contract PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
    target_link_libraries(matrixgame-texture-residency-contract PRIVATE matrixgame-native-core)
    if(NOT WIN32)
        target_sources(matrixgame-texture-residency-contract PRIVATE legacy_texture_backend.cpp unmanaged_texture_backend.cpp)
    endif()
    target_compile_options(matrixgame-texture-residency-contract PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
    add_test(NAME matrixgame-texture-residency-contract COMMAND matrixgame-texture-residency-contract)
    add_executable(matrixgame-native-entry-contract "${PORT_ROOT}/tests/test_matrixgame_native_entry.cpp")
    target_link_libraries(matrixgame-native-entry-contract PRIVATE matrixgame-native-cache)
    target_compile_options(matrixgame-native-entry-contract PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
    set(MATRIXGAME_NATIVE_ENTRY_RESOURCES "" CACHE PATH "Private original robots.pkg/CFG runtime directory")
    if(MATRIXGAME_NATIVE_ENTRY_RESOURCES)
        add_test(NAME matrixgame-native-entry-contract COMMAND matrixgame-native-entry-contract)
        set_tests_properties(matrixgame-native-entry-contract PROPERTIES WORKING_DIRECTORY "${MATRIXGAME_NATIVE_ENTRY_RESOURCES}")
    endif()
endif()
if(WIN32)
    # Full original Init/game dependencies; distinct from the minimal bootstrap.
    add_executable(matrixgame-windows-game EXCLUDE_FROM_ALL
        "${PORT_ROOT}/tests/matrixgame_windows_frame_main.cpp"
        "${PORT_ROOT}/tests/matrixgame_windows_frame_init.cpp"
        "${PORT_ROOT}/tests/matrixgame_windows_frame.rc"
        "${ENGINE_SOURCE}/MatrixGame/src/OriginalWindowsFrame.cpp")
    target_link_libraries(matrixgame-windows-game PRIVATE matrixgame-native-cache)
    target_include_directories(matrixgame-windows-game PRIVATE "${MATRIXGAME_D3DX9_INCLUDE}"
        "${PORT_ROOT}/upstream/matrixgame/MatrixGame")
    target_compile_options(matrixgame-windows-game PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:/EHsc> $<$<COMPILE_LANGUAGE:CXX>:/UNDEBUG>)
endif()
if(BUILD_TESTING AND (WIN32 OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU"))
    add_executable(matrixgame-native-borrowed-skin "${PORT_ROOT}/tests/test_matrixgame_borrowed_skin.cpp")
    target_link_libraries(matrixgame-native-borrowed-skin PRIVATE matrixgame-native-core)
    target_compile_options(matrixgame-native-borrowed-skin PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/EHsc> $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
    add_test(NAME matrixgame-native-borrowed-skin COMMAND matrixgame-native-borrowed-skin)
    add_executable(matrixgame-native-borrowed-skin-vo "${PORT_ROOT}/tests/test_matrixgame_borrowed_skin_vo.cpp")
    target_link_libraries(matrixgame-native-borrowed-skin-vo PRIVATE matrixgame-native-vector-object)
    target_compile_options(matrixgame-native-borrowed-skin-vo PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/EHsc> $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
    set(MATRIXGAME_BORROWED_SKIN_MODEL "" CACHE FILEPATH "Private original VO fixture for borrowed skin lifetime test")
    if(MATRIXGAME_BORROWED_SKIN_MODEL)
        add_test(NAME matrixgame-native-borrowed-skin-vo COMMAND matrixgame-native-borrowed-skin-vo "${MATRIXGAME_BORROWED_SKIN_MODEL}")
    endif()
    add_executable(matrixgame-native-path-effect
        "${PORT_ROOT}/tests/test_matrixgame_path_effect.cpp"
        "${ENGINE_SOURCE}/MatrixGame/src/Effects/MatrixEffectPath.cpp")
    target_link_libraries(matrixgame-native-path-effect PRIVATE matrixgame-native-cache)
    if(MSVC)
        target_compile_definitions(matrixgame-native-path-effect PRIVATE MATRIXGAME_PATH_TEST_ALLOCATOR)
        target_compile_options(matrixgame-native-path-effect PRIVATE /EHsc /UNDEBUG)
    else()
        target_compile_options(matrixgame-native-path-effect PRIVATE -UNDEBUG)
        target_link_options(matrixgame-native-path-effect PRIVATE -Wl,--wrap=realloc)
    endif()
    add_test(NAME matrixgame-native-path-effect COMMAND matrixgame-native-path-effect)
endif()
if(BUILD_TESTING OR MATRIXGAME_INIT_BUILD_LINK_PROBE)
    add_executable(matrixgame-native-animation-probe "${PORT_ROOT}/tests/test_matrixgame_animation.cpp")
    target_link_libraries(matrixgame-native-animation-probe PRIVATE matrixgame-native-cache)
    target_compile_options(matrixgame-native-animation-probe PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
    add_executable(matrixgame-native-callbacks-probe "${PORT_ROOT}/tests/test_matrixgame_skin_callbacks.cpp")
    target_link_libraries(matrixgame-native-callbacks-probe PRIVATE matrixgame-native-cache)
    target_compile_options(matrixgame-native-callbacks-probe PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
    add_executable(matrixgame-native-skin-probe "${PORT_ROOT}/tests/test_matrixgame_native_skin.cpp")
    target_link_libraries(matrixgame-native-skin-probe PRIVATE matrixgame-native-cache)
    target_compile_options(matrixgame-native-skin-probe PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
    add_executable(matrixgame-native-frame-probe "${PORT_ROOT}/tests/test_matrixgame_cached_frame.cpp")
    target_link_libraries(matrixgame-native-frame-probe PRIVATE matrixgame-native-cache)
    target_compile_options(matrixgame-native-frame-probe PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
    add_executable(matrixgame-native-init-probe "${PORT_ROOT}/tests/test_matrixgame_native_init.cpp")
    target_link_libraries(matrixgame-native-init-probe PRIVATE matrixgame-native-cache)
    target_compile_options(matrixgame-native-init-probe PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND (NOT CMAKE_CROSSCOMPILING OR CMAKE_SYSTEM_NAME STREQUAL "Linux"))
        target_compile_definitions(matrixgame-native-callbacks-probe PRIVATE MATRIXGAME_CALLBACK_ALLOC_FAULTS)
        target_link_options(matrixgame-native-callbacks-probe PRIVATE -Wl,--wrap=_Znwm)
        target_compile_definitions(matrixgame-native-skin-probe PRIVATE MATRIXGAME_SKIN_ALLOC_FAULTS)
        target_link_options(matrixgame-native-skin-probe PRIVATE -Wl,--wrap=_Znwm)
        target_compile_definitions(matrixgame-native-init-probe PRIVATE MATRIXGAME_INIT_ALLOC_FAULTS)
        target_link_options(matrixgame-native-init-probe PRIVATE -Wl,--wrap=calloc)
    endif()
    if(MATRIXGAME_INIT_BUILD_LINK_PROBE)
        if(NOT CMAKE_CROSSCOMPILING OR NOT MATRIXGAME_DEVKITPRO)
            message(FATAL_ERROR "Init GPU link probe requires devkitA64")
        endif()
        target_sources(matrixgame-native-init-probe PRIVATE
            "${PORT_ROOT}/tests/probe_matrixgame_cache_upload_link.cpp"
            "${PORT_ROOT}/port/matrixgame/deko/mesh_renderer.cpp")
        target_compile_definitions(matrixgame-native-init-probe PRIVATE MATRIXGAME_INIT_GPU_LINK)
        target_include_directories(matrixgame-native-init-probe PRIVATE
            "${PORT_ROOT}/port/matrixgame/deko" "${MATRIXGAME_SHADER_DIR}"
            "${MATRIXGAME_DEVKITPRO}/libnx/include")
        target_link_directories(matrixgame-native-init-probe PRIVATE "${MATRIXGAME_DEVKITPRO}/libnx/lib")
        target_link_libraries(matrixgame-native-init-probe PRIVATE ${MATRIXGAME_DEKO_LIBRARY} nx)
        target_link_options(matrixgame-native-init-probe PRIVATE
            "-specs=${MATRIXGAME_DEVKITPRO}/libnx/switch.specs" -Wl,--gc-sections -Wl,--build-id=sha1)
        set_target_properties(matrixgame-native-init-probe PROPERTIES SUFFIX ".elf")
        target_include_directories(matrixgame-native-bootstrap PRIVATE "${MATRIXGAME_DEVKITPRO}/libnx/include")
        target_link_directories(matrixgame-native-bootstrap PRIVATE "${MATRIXGAME_DEVKITPRO}/libnx/lib")
        target_link_libraries(matrixgame-native-bootstrap PRIVATE nx)
        target_link_options(matrixgame-native-bootstrap PRIVATE "-specs=${MATRIXGAME_DEVKITPRO}/libnx/switch.specs" -Wl,--gc-sections -Wl,--build-id=sha1)
        set_target_properties(matrixgame-native-bootstrap PROPERTIES SUFFIX ".elf")
        target_include_directories(matrixgame-original-init PRIVATE "${MATRIXGAME_DEVKITPRO}/libnx/include")
        target_link_directories(matrixgame-original-init PRIVATE "${MATRIXGAME_DEVKITPRO}/libnx/lib")
        target_link_libraries(matrixgame-original-init PRIVATE nx)
        target_link_options(matrixgame-original-init PRIVATE "-specs=${MATRIXGAME_DEVKITPRO}/libnx/switch.specs" -Wl,--gc-sections -Wl,--build-id=sha1)
        set_target_properties(matrixgame-original-init PROPERTIES SUFFIX ".elf")
        foreach(SESSION_TARGET matrixgame-native-frame-probe matrixgame-native-skin-probe matrixgame-native-callbacks-probe matrixgame-native-animation-probe)
        target_sources(${SESSION_TARGET} PRIVATE
            "${PORT_ROOT}/tests/probe_matrixgame_native_session_link.cpp"
            "${PORT_ROOT}/port/matrixgame/deko/native_session.cpp"
            "${PORT_ROOT}/port/matrixgame/deko/buffer_renderer.cpp"
            "${PORT_ROOT}/port/matrixgame/deko/mesh_renderer.cpp")
        target_compile_definitions(${SESSION_TARGET} PRIVATE MATRIXGAME_SESSION_GPU_LINK)
        target_include_directories(${SESSION_TARGET} PRIVATE
            "${MATRIXGAME_SHADER_DIR}" "${MATRIXGAME_DEVKITPRO}/libnx/include")
        target_link_directories(${SESSION_TARGET} PRIVATE "${MATRIXGAME_DEVKITPRO}/libnx/lib")
        target_link_libraries(${SESSION_TARGET} PRIVATE ${MATRIXGAME_DEKO_LIBRARY} nx)
        target_link_options(${SESSION_TARGET} PRIVATE
            "-specs=${MATRIXGAME_DEVKITPRO}/libnx/switch.specs" -Wl,--gc-sections -Wl,--build-id=sha1)
        set_target_properties(${SESSION_TARGET} PROPERTIES SUFFIX ".elf")
        endforeach()
    endif()
    add_executable(matrixgame-native-vector-object-probe "${PORT_ROOT}/tests/test_matrixgame_vector_object.cpp")
    target_link_libraries(matrixgame-native-vector-object-probe PRIVATE matrixgame-native-vector-object)
    target_compile_options(matrixgame-native-vector-object-probe PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
endif()
add_library(matrixgame-native-navigation STATIC
    "${ENGINE_SOURCE}/MatrixGame/src/Logic/MatrixRoadNetwork.cpp")
target_link_libraries(matrixgame-native-navigation PUBLIC matrixgame-native-core)
target_link_libraries(matrixgame-native-cache PUBLIC matrixgame-native-navigation)
target_include_directories(matrixgame-native-navigation PUBLIC "${ENGINE_SOURCE}/MatrixGame/src/Logic")
# Match the original release navigation path. GPU debug helpers remain separate.
target_compile_definitions(matrixgame-native-navigation PRIVATE _RELDEBUG)
option(NAVIGATION_BUILD_LINK_PROBE "Build a navigation CPU executable; does not execute CGame" OFF)
option(MATRIXGAME_BUILD_RANGERS_RUN "Build the typed native Run slot for a real Rangers caller" OFF)
if(MATRIXGAME_BUILD_RANGERS_RUN)
    if(NOT TARGET matrixgame-engine-callbacks)
        add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../native" "${CMAKE_CURRENT_BINARY_DIR}/rangers-run-bridge")
    endif()
    if(NOT MATRIXGAME_INIT_BUILD_LINK_PROBE)
        target_sources(matrixgame-native-cache PRIVATE native_form_backend.cpp native_form_session.cpp
            native_loop.cpp native_input_engine.cpp native_input.cpp applet_engine_events.cpp
            "${ENGINE_SOURCE}/MatrixLib/3G/OriginalNativeLoop.cpp"
            "${ENGINE_SOURCE}/MatrixGame/src/OriginalNativeForm.cpp")
    endif()
    add_library(matrixgame-native-rangers-run STATIC native_rangers_run.cpp)
    target_include_directories(matrixgame-native-rangers-run PUBLIC "${CMAKE_CURRENT_LIST_DIR}"
        "${PORT_ROOT}/upstream/cpp/src" "${PORT_ROOT}/upstream/cpp/runtime")
    target_link_libraries(matrixgame-native-rangers-run PUBLIC matrixgame-native-cache matrixgame-engine-callbacks)
    add_library(matrixgame-native-rangers-interface STATIC native_rangers_interface.cpp native_rangers_support.cpp rangers_caller_presentation.cpp)
    if(MATRIXGAME_DEVKITPRO)
        target_sources(matrixgame-native-rangers-interface PRIVATE rangers_input_switch.cpp)
    endif()
    if(WIN32)
        target_sources(matrixgame-native-rangers-interface PRIVATE rangers_legacy_presentation_win32.cpp)
    endif()
    target_link_libraries(matrixgame-native-rangers-interface PUBLIC matrixgame-native-rangers-run)
endif()
if(BUILD_TESTING OR NAVIGATION_BUILD_LINK_PROBE)
    add_executable(matrixgame-native-navigation-test "${PORT_ROOT}/tests/test_matrixgame_navigation.cpp")
    target_link_libraries(matrixgame-native-navigation-test PRIVATE matrixgame-native-navigation)
    target_compile_options(matrixgame-native-navigation-test PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
    if(BUILD_TESTING)
        add_test(NAME matrixgame-native-navigation-contract COMMAND matrixgame-native-navigation-test)
    endif()
endif()
file(GLOB_RECURSE ENGINE_UNITS CONFIGURE_DEPENDS "${ENGINE_SOURCE}/MatrixGame/src/*.cpp")
list(FILTER ENGINE_UNITS EXCLUDE REGEX "/MatrixGameDll.cpp$")
list(FILTER ENGINE_UNITS EXCLUDE REGEX "/Logic/MatrixRoadNetwork.cpp$")
list(FILTER ENGINE_UNITS EXCLUDE REGEX "/GameDefaultsNative.cpp$")
list(FILTER ENGINE_UNITS EXCLUDE REGEX "/MatrixCursorLifecycle.cpp$")
list(FILTER ENGINE_UNITS EXCLUDE REGEX "/(OriginalTransition|MatrixTransitionLifecycle).cpp$")
list(FILTER ENGINE_UNITS EXCLUDE REGEX "/(OriginalMapPrepare|OriginalGroupLifecycle).cpp$")
list(FILTER ENGINE_UNITS EXCLUDE REGEX "/OriginalRendererOps.cpp$")
list(FILTER ENGINE_UNITS EXCLUDE REGEX "/MatrixCameraLifecycle.cpp$")
# Opt-in full compilation target. Remaining Windows/D3D dependencies must be ported;
# a successful default core build is not evidence that this target compiled.
add_library(matrixgame-native-engine STATIC EXCLUDE_FROM_ALL ${ENGINE_UNITS})
target_link_libraries(matrixgame-native-engine PUBLIC matrixgame-native-core matrixgame-native-navigation)
target_include_directories(matrixgame-native-engine PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}"
    "${ENGINE_SOURCE}/MatrixGame" "${ENGINE_SOURCE}/MatrixGame/src"
    "${ENGINE_SOURCE}/MatrixLib/Base" "${ENGINE_SOURCE}/MatrixLib/3G" "${ENGINE_SOURCE}/MatrixLib/Bitmap")
target_compile_features(matrixgame-native-engine PUBLIC cxx_std_20)

# Full prepared MatrixLib units for per-file compatibility auditing. This is
# intentionally separate from the selected CPU native-core implementation and
# must not be linked together with it as a substitute for dependency closure.
file(GLOB_RECURSE MATRIXLIB_AUDIT_UNITS CONFIGURE_DEPENDS "${ENGINE_SOURCE}/MatrixLib/*.cpp")
add_library(matrixgame-native-matrixlib-audit STATIC EXCLUDE_FROM_ALL ${MATRIXLIB_AUDIT_UNITS})
target_include_directories(matrixgame-native-matrixlib-audit PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}"
    "${ENGINE_SOURCE}/MatrixLib/Base" "${ENGINE_SOURCE}/MatrixLib/3G" "${ENGINE_SOURCE}/MatrixLib/Bitmap"
    "${CMAKE_CURRENT_LIST_DIR}/../native-core" "${PORT_ROOT}/port/switch/platform"
    "${PORT_ROOT}/port/matrixgame/third_party/directxmath/Inc" "${PORT_ROOT}/port/matrixgame/third_party/sal")
target_compile_features(matrixgame-native-matrixlib-audit PRIVATE cxx_std_20)
if(NOT MSVC)
    target_compile_definitions(matrixgame-native-matrixlib-audit PRIVATE _POSIX_C_SOURCE=200809L)
    target_compile_options(matrixgame-native-matrixlib-audit PRIVATE -ffp-contract=off)
endif()

# Production portable presentation transaction/state lease tests; no SDK emulation.
if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    add_executable(matrixgame-native-presentation-test
        "${PORT_ROOT}/tests/test_matrixgame_native_presentation.cpp" native_device_state.cpp)
    target_include_directories(matrixgame-native-presentation-test PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
    target_link_libraries(matrixgame-native-presentation-test PRIVATE matrixgame-native-core matrixgame-material-combiner)
    target_compile_options(matrixgame-native-presentation-test PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
    add_test(NAME matrixgame-native-presentation-contract COMMAND matrixgame-native-presentation-test)
endif()

# Actual original text TUs plus the production native raster/texture bridge.
if(BUILD_TESTING AND NOT WIN32 AND NOT CMAKE_CROSSCOMPILING)
    set(MATRIXGAME_TEXT_FONT_ASSETS "" CACHE PATH "Verified original UI font bakes and Windows HAL goldens")
    add_executable(matrixgame-native-text-test "${PORT_ROOT}/tests/test_matrixgame_native_text.cpp"
        "${ENGINE_SOURCE}/MatrixGame/src/Text/Render.cpp" "${ENGINE_SOURCE}/MatrixGame/src/Text/Font.cpp"
        "${ENGINE_SOURCE}/MatrixGame/src/Text/Parser.cpp"
        platform_3g.cpp text_backend.cpp native_text.cpp native_text_backend.cpp native_texture.cpp legacy_texture_backend.cpp)
    target_include_directories(matrixgame-native-text-test PRIVATE "${CMAKE_CURRENT_LIST_DIR}" "${ENGINE_SOURCE}/MatrixGame/src"
        "${ENGINE_SOURCE}/MatrixLib/Base" "${ENGINE_SOURCE}/MatrixLib/3G" "${ENGINE_SOURCE}/MatrixLib/Bitmap")
    target_link_libraries(matrixgame-native-text-test PRIVATE matrixgame-native-core)
    target_compile_options(matrixgame-native-text-test PRIVATE -UNDEBUG -ffunction-sections -fdata-sections)
    target_link_options(matrixgame-native-text-test PRIVATE -Wl,--gc-sections)
    if(CORE_SANITIZERS)
        target_compile_options(matrixgame-native-text-test PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(matrixgame-native-text-test PRIVATE -fsanitize=address,undefined)
    endif()
    if(MATRIXGAME_TEXT_FONT_ASSETS)
        add_test(NAME matrixgame-native-text-contract COMMAND matrixgame-native-text-test "${MATRIXGAME_TEXT_FONT_ASSETS}")
    endif()
endif()

if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING AND NOT WIN32)
    add_executable(matrixgame-native-surfaces-test "${PORT_ROOT}/tests/test_matrixgame_native_surfaces.cpp"
        native_surfaces.cpp offscreen_backend.cpp offscreen_scope.cpp platform_3g.cpp)
    target_include_directories(matrixgame-native-surfaces-test PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
    target_link_libraries(matrixgame-native-surfaces-test PRIVATE matrixgame-native-core)
    target_compile_options(matrixgame-native-surfaces-test PRIVATE -UNDEBUG -ffunction-sections -fdata-sections)
    target_link_options(matrixgame-native-surfaces-test PRIVATE -Wl,--gc-sections)
    if(CORE_SANITIZERS)
        target_compile_options(matrixgame-native-surfaces-test PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(matrixgame-native-surfaces-test PRIVATE -fsanitize=address,undefined)
    endif()
    add_test(NAME matrixgame-native-surfaces-contract COMMAND matrixgame-native-surfaces-test)
endif()
