cmake_minimum_required(VERSION 3.20)
project(MatrixGameNativeEngine LANGUAGES CXX)
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
file(GLOB_RECURSE ENGINE_UNITS CONFIGURE_DEPENDS "${ENGINE_SOURCE}/MatrixGame/src/*.cpp")
list(FILTER ENGINE_UNITS EXCLUDE REGEX "/MatrixGameDll.cpp$")
# Opt-in full compilation target. Remaining Windows/D3D dependencies must be ported;
# a successful default core build is not evidence that this target compiled.
add_library(matrixgame-native-engine STATIC EXCLUDE_FROM_ALL ${ENGINE_UNITS})
target_link_libraries(matrixgame-native-engine PUBLIC matrixgame-native-core)
target_include_directories(matrixgame-native-engine PRIVATE
    "${ENGINE_SOURCE}/MatrixGame" "${ENGINE_SOURCE}/MatrixGame/src"
    "${ENGINE_SOURCE}/MatrixLib/Base" "${ENGINE_SOURCE}/MatrixLib/3G" "${ENGINE_SOURCE}/MatrixLib/Bitmap")
target_compile_features(matrixgame-native-engine PUBLIC cxx_std_20)
