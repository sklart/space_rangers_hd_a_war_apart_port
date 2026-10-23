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
    "${CMAKE_CURRENT_BINARY_DIR}/core/source/CacheNative.cpp" cache_resources.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/core/source/L3GCpuPhase.cpp" init_runtime.cpp cached_frame.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/core/source/3GPlatformNative.cpp" platform_3g.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/core/source/3GDeviceDefaults.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/source/MatrixLib/3G/DeviceState.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/source/MatrixGame/src/MatrixSampleStateManager.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/core/source/SkinResourcesNative.cpp" native_skin.cpp skin_pipeline.cpp)
target_link_libraries(matrixgame-native-cache PUBLIC matrixgame-native-vector-object)
target_include_directories(matrixgame-native-cache PUBLIC "${CMAKE_CURRENT_LIST_DIR}")
if(WIN32)
    target_sources(matrixgame-native-cache PRIVATE platform_win32.cpp)
    target_link_libraries(matrixgame-native-cache PUBLIC d3d9 user32)
endif()
if(BUILD_TESTING OR MATRIXGAME_INIT_BUILD_LINK_PROBE)
    add_executable(matrixgame-native-bootstrap "${PORT_ROOT}/tests/test_matrixgame_bootstrap.cpp"
        "${PORT_ROOT}/tests/matrixgame_bootstrap_sampling.cpp")
    target_link_libraries(matrixgame-native-bootstrap PRIVATE matrixgame-native-cache)
    target_compile_definitions(matrixgame-native-bootstrap PRIVATE
        MATRIXGAME_FULL_VECTOR_HEADER="${ENGINE_SOURCE}/MatrixLib/3G/VectorObject.hpp"
        MATRIXGAME_FULL_BILLBOARD_HEADER="${ENGINE_SOURCE}/MatrixLib/3G/CBillboard.hpp")
    target_compile_definitions(matrixgame-native-bootstrap PRIVATE
        MATRIXGAME_FULL_3G_HEADER="${ENGINE_SOURCE}/MatrixLib/3G/3g.hpp")
    if(WIN32)
        target_sources(matrixgame-native-bootstrap PRIVATE "${PORT_ROOT}/tests/test_matrixgame_bootstrap_win32.cpp")
        target_compile_options(matrixgame-native-bootstrap PRIVATE /EHsc)
    endif()
    target_compile_options(matrixgame-native-bootstrap PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
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
