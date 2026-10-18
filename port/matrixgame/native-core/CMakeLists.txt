cmake_minimum_required(VERSION 3.20)
project(MatrixGameNativeCore LANGUAGES CXX)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
get_filename_component(PORT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(CORE_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/source")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${PORT_ROOT}/tools/matrixgame_core_prepare.py"
    --output "${CORE_SOURCE}" RESULT_VARIABLE PREPARE_RESULT OUTPUT_QUIET)
if(NOT PREPARE_RESULT EQUAL 0)
    message(FATAL_ERROR "Pinned MatrixLib source preparation failed")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${PORT_ROOT}/tools/matrixgame_core_prepare.py"
    "${PORT_ROOT}/upstream/matrixgame/MatrixLib/Base/BaseDef.hpp"
    "${PORT_ROOT}/upstream/matrixgame/MatrixLib/Base/CRC32.hpp"
    "${PORT_ROOT}/upstream/matrixgame/MatrixLib/Base/CRC32.cpp"
    "${PORT_ROOT}/upstream/matrixgame/MatrixLib/Base/CBuf.hpp"
    "${PORT_ROOT}/upstream/matrixgame/MatrixLib/Base/CBuf.cpp"
    "${PORT_ROOT}/upstream/matrixgame/MatrixLib/Base/CException.hpp"
    "${PORT_ROOT}/upstream/matrixgame/MatrixLib/Base/CFile.hpp"
    "${PORT_ROOT}/upstream/matrixgame/MatrixLib/Base/CFile.cpp"
    "${PORT_ROOT}/upstream/matrixgame/MatrixLib/Base/CMain.hpp"
    "${PORT_ROOT}/tools/matrixgame_storage_overrides.py"
    "${CMAKE_CURRENT_LIST_DIR}/storage_zl03.inc"
    "${CMAKE_CURRENT_LIST_DIR}/storage_preflight.inc"
    "${PORT_ROOT}/upstream/matrixgame/MatrixLib/Base/CStorage.hpp"
    "${PORT_ROOT}/upstream/matrixgame/MatrixLib/Base/CStorage.cpp"
    "${PORT_ROOT}/upstream/matrixgame/MatrixLib/Base/CHeap.hpp"
    "${PORT_ROOT}/upstream/matrixgame/MatrixLib/Base/CBlockPar.hpp"
    "${PORT_ROOT}/upstream/matrixgame/MatrixLib/Base/utils.hpp")
add_library(matrixgame-native-core STATIC "${CORE_SOURCE}/CRC32.cpp"
    "${CORE_SOURCE}/CBuf.cpp" "${CORE_SOURCE}/CBuf_file.cpp"
    "${CORE_SOURCE}/CStorage.cpp" heap_native.cpp cbuf_utf16.cpp exception_native.cpp file_native.cpp "${PORT_ROOT}/port/switch/platform/package.cpp")
target_include_directories(matrixgame-native-core PUBLIC "${CORE_SOURCE}" "${CMAKE_CURRENT_LIST_DIR}")
target_compile_features(matrixgame-native-core PUBLIC cxx_std_20)
find_package(ZLIB REQUIRED)
target_link_libraries(matrixgame-native-core PUBLIC ZLIB::ZLIB)
target_include_directories(matrixgame-native-core PRIVATE "${PORT_ROOT}/port/switch/platform")
if(NOT CMAKE_CROSSCOMPILING)
    find_package(Threads REQUIRED)
    target_link_libraries(matrixgame-native-core PUBLIC Threads::Threads)
endif()
option(CORE_SANITIZERS "Enable ASan and UBSan on host" OFF)
if(CORE_SANITIZERS)
    target_compile_options(matrixgame-native-core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(matrixgame-native-core PUBLIC -fsanitize=address,undefined)
endif()
option(CORE_THREAD_SANITIZER "Enable ThreadSanitizer on host" OFF)
if(CORE_THREAD_SANITIZER)
    if(CORE_SANITIZERS)
        message(FATAL_ERROR "ASan and TSan require separate build directories")
    endif()
    target_compile_options(matrixgame-native-core PUBLIC -fsanitize=thread -fno-omit-frame-pointer)
    target_link_options(matrixgame-native-core PUBLIC -fsanitize=thread)
endif()
include(CTest)
if(BUILD_TESTING)
    add_executable(matrixgame-native-crc-test "${PORT_ROOT}/tests/test_matrixgame_native_crc.cpp")
    target_link_libraries(matrixgame-native-crc-test PRIVATE matrixgame-native-core)
    if(MSVC)
        target_compile_options(matrixgame-native-crc-test PRIVATE /UNDEBUG)
    else()
        target_compile_options(matrixgame-native-crc-test PRIVATE -UNDEBUG)
    endif()
    add_test(NAME matrixgame-native-crc-contract COMMAND matrixgame-native-crc-test)
    add_executable(matrixgame-native-buffer-test "${PORT_ROOT}/tests/test_matrixgame_native_buffer.cpp")
    target_link_libraries(matrixgame-native-buffer-test PRIVATE matrixgame-native-core)
    if(MSVC)
        target_compile_options(matrixgame-native-buffer-test PRIVATE /UNDEBUG)
    else()
        target_compile_options(matrixgame-native-buffer-test PRIVATE -UNDEBUG)
    endif()
    add_test(NAME matrixgame-native-buffer-contract COMMAND matrixgame-native-buffer-test)
    add_executable(matrixgame-native-file-test "${PORT_ROOT}/tests/test_matrixgame_native_file.cpp")
    target_link_libraries(matrixgame-native-file-test PRIVATE matrixgame-native-core)
    if(MSVC)
        target_compile_options(matrixgame-native-file-test PRIVATE /UNDEBUG)
    else()
        target_compile_options(matrixgame-native-file-test PRIVATE -UNDEBUG)
    endif()
    add_test(NAME matrixgame-native-file-contract COMMAND "${Python3_EXECUTABLE}"
        "${PORT_ROOT}/tests/probe_matrixgame_native_file.py"
        --binary "$<TARGET_FILE:matrixgame-native-file-test>" --output "${CMAKE_CURRENT_BINARY_DIR}/file-fixtures")
    add_executable(matrixgame-native-storage-test "${PORT_ROOT}/tests/test_matrixgame_native_storage.cpp")
    target_link_libraries(matrixgame-native-storage-test PRIVATE matrixgame-native-core)
    if(MSVC)
        target_compile_options(matrixgame-native-storage-test PRIVATE /UNDEBUG)
    else()
        target_compile_options(matrixgame-native-storage-test PRIVATE -UNDEBUG)
    endif()
    add_test(NAME matrixgame-native-storage-contract COMMAND "${Python3_EXECUTABLE}"
        "${PORT_ROOT}/tests/probe_matrixgame_native_storage.py"
        --binary "$<TARGET_FILE:matrixgame-native-storage-test>" --output "${CMAKE_CURRENT_BINARY_DIR}/storage-fixtures")
endif()

option(CORE_BUILD_LINK_PROBE "Link the CPU file contract ELF for dependency proof only; do not run/package" OFF)
if(CORE_BUILD_LINK_PROBE)
    if(NOT CMAKE_CROSSCOMPILING OR NOT MATRIXGAME_DEVKITPRO)
        message(FATAL_ERROR "CORE_BUILD_LINK_PROBE requires the Switch toolchain")
    endif()
    add_executable(matrixgame-native-core-link-probe "${PORT_ROOT}/tests/test_matrixgame_native_file.cpp")
    target_link_libraries(matrixgame-native-core-link-probe PRIVATE matrixgame-native-core nx)
    target_link_directories(matrixgame-native-core-link-probe PRIVATE "${MATRIXGAME_DEVKITPRO}/libnx/lib")
    target_link_options(matrixgame-native-core-link-probe PRIVATE
        "-specs=${MATRIXGAME_DEVKITPRO}/libnx/switch.specs" -fPIE -Wl,--gc-sections -Wl,--build-id=sha1)
    target_compile_options(matrixgame-native-core-link-probe PRIVATE -UNDEBUG)
    set_target_properties(matrixgame-native-core-link-probe PROPERTIES SUFFIX ".elf")
    add_executable(matrixgame-native-storage-link-probe "${PORT_ROOT}/tests/test_matrixgame_native_storage.cpp")
    target_link_libraries(matrixgame-native-storage-link-probe PRIVATE matrixgame-native-core nx)
    target_link_directories(matrixgame-native-storage-link-probe PRIVATE "${MATRIXGAME_DEVKITPRO}/libnx/lib")
    target_link_options(matrixgame-native-storage-link-probe PRIVATE
        "-specs=${MATRIXGAME_DEVKITPRO}/libnx/switch.specs" -fPIE -Wl,--gc-sections -Wl,--build-id=sha1)
    target_compile_options(matrixgame-native-storage-link-probe PRIVATE -UNDEBUG)
    set_target_properties(matrixgame-native-storage-link-probe PROPERTIES SUFFIX ".elf")
endif()
