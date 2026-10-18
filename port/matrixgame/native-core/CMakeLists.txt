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
    "${PORT_ROOT}/upstream/matrixgame/MatrixLib/Base/CException.hpp")
add_library(matrixgame-native-core STATIC "${CORE_SOURCE}/CRC32.cpp"
    "${CORE_SOURCE}/CBuf.cpp" cbuf_utf16.cpp exception_native.cpp)
target_include_directories(matrixgame-native-core PUBLIC "${CORE_SOURCE}" "${CMAKE_CURRENT_LIST_DIR}")
target_compile_features(matrixgame-native-core PUBLIC cxx_std_20)
option(CORE_SANITIZERS "Enable ASan and UBSan on host" OFF)
if(CORE_SANITIZERS)
    target_compile_options(matrixgame-native-core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(matrixgame-native-core PUBLIC -fsanitize=address,undefined)
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
endif()
