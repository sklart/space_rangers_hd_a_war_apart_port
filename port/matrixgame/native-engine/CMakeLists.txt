cmake_minimum_required(VERSION 3.20)
project(MatrixGameNativeEngine LANGUAGES CXX)
include(CTest)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
get_filename_component(PORT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
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
# Explicit original VectorObject CPU subset, not a full engine/GPU target.
add_library(matrixgame-native-vector-object STATIC "${CMAKE_CURRENT_BINARY_DIR}/core/source/VectorObjectCpu.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/core/source/VectorObjectAnimNative.cpp")
target_link_libraries(matrixgame-native-vector-object PUBLIC matrixgame-native-core)
add_library(matrixgame-native-cache STATIC
    "${CMAKE_CURRENT_BINARY_DIR}/core/source/CacheNative.cpp" cache_resources.cpp cache_owner_state.cpp platform_globals.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/core/source/L3GCpuPhase.cpp" init_runtime.cpp cached_frame.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/core/source/3GPlatformNative.cpp" platform_3g.cpp map_device_backend.cpp debug_font_backend.cpp offscreen_backend.cpp
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
    "${ENGINE_SOURCE}/MatrixLib/3G/CBillboard.cpp"
    "${ENGINE_SOURCE}/MatrixLib/3G/OriginalVectorAnim.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixMapGroup.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/OriginalGroupLifecycle.cpp"
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
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixTransitionLifecycle.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/input.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/source/MatrixGame/src/GameStaticResources.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/source/MatrixGame/src/MatrixLoadProgress.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixRenderPipeline.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/OriginalRendererOps.cpp"
    "${ENGINE_SOURCE}/MatrixLib/3G/OriginalManagedTexture.cpp" legacy_texture_backend.cpp
    "${ENGINE_SOURCE}/MatrixLib/3G/OriginalUnmanagedTexture.cpp" unmanaged_texture_backend.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/source/MatrixGame/src/MatrixCursorLifecycle.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixCameraLifecycle.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/source/MatrixGame/src/MatrixSampleStateManager.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/core/source/SkinResourcesNative.cpp" native_skin.cpp skin_pipeline.cpp)
target_link_libraries(matrixgame-native-cache PUBLIC matrixgame-native-vector-object)
# Compile the complete original CGame TU; link only the original methods whose
# dependencies are actually closed. This is not a full-engine link claim.
set_property(SOURCE "${ENGINE_SOURCE}/MatrixGame/src/MatrixGame.cpp"
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
    "${ENGINE_SOURCE}/MatrixLib/3G/CBillboard.cpp"
    "${ENGINE_SOURCE}/MatrixLib/3G/OriginalVectorAnim.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixMapGroup.cpp"
    "${ENGINE_SOURCE}/MatrixGame/src/OriginalGroupLifecycle.cpp"
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
    "${ENGINE_SOURCE}/MatrixGame/src/MatrixTransitionLifecycle.cpp"
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
if(WIN32)
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
    target_link_libraries(matrixgame-native-cache PUBLIC d3d9 user32 winmm "${MATRIXGAME_D3DX9_LIBRARY}")
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
        "${PORT_ROOT}/tests/matrixgame_bootstrap_smoke_fire.cpp")
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
        target_link_libraries(matrixgame-native-init-probe PRIVATE deko3d nx)
        target_link_options(matrixgame-native-init-probe PRIVATE
            "-specs=${MATRIXGAME_DEVKITPRO}/libnx/switch.specs" -Wl,--gc-sections -Wl,--build-id=sha1)
        set_target_properties(matrixgame-native-init-probe PROPERTIES SUFFIX ".elf")
        target_include_directories(matrixgame-native-bootstrap PRIVATE "${MATRIXGAME_DEVKITPRO}/libnx/include")
        target_link_directories(matrixgame-native-bootstrap PRIVATE "${MATRIXGAME_DEVKITPRO}/libnx/lib")
        target_link_libraries(matrixgame-native-bootstrap PRIVATE nx)
        target_link_options(matrixgame-native-bootstrap PRIVATE "-specs=${MATRIXGAME_DEVKITPRO}/libnx/switch.specs" -Wl,--gc-sections -Wl,--build-id=sha1)
        set_target_properties(matrixgame-native-bootstrap PROPERTIES SUFFIX ".elf")
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
        target_link_libraries(${SESSION_TARGET} PRIVATE deko3d nx)
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
list(FILTER ENGINE_UNITS EXCLUDE REGEX "/MatrixTransitionLifecycle.cpp$")
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
