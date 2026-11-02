cmake_minimum_required(VERSION 3.20)
project(MatrixGameOriginalConfigContract LANGUAGES CXX)
get_filename_component(PORT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(MATRIXGAME_ENGINE_SOURCE "" CACHE PATH "Verified disposable native engine source, read only")
if(NOT EXISTS "${MATRIXGAME_ENGINE_SOURCE}/native-engine-source.json")
    message(FATAL_ERROR "Provide a verified generated engine source; never edit upstream")
endif()
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../native-core" "${CMAKE_CURRENT_BINARY_DIR}/core")
set(MATRIXGAME_CONFIG_DEP_INCLUDES "" CACHE STRING "Generated PNG/JPEG configuration header directories")
if(MATRIXGAME_CONFIG_DEP_INCLUDES)
    target_include_directories(matrixgame-native-core PRIVATE ${MATRIXGAME_CONFIG_DEP_INCLUDES})
endif()
add_executable(matrixgame-original-config-contract
    "${PORT_ROOT}/tests/test_matrixgame_original_config.cpp"
    "${MATRIXGAME_ENGINE_SOURCE}/MatrixGame/src/MatrixConfig.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../native-engine/platform_3g.cpp")
target_link_libraries(matrixgame-original-config-contract PRIVATE matrixgame-native-core)
target_include_directories(matrixgame-original-config-contract BEFORE PRIVATE
    "${MATRIXGAME_ENGINE_SOURCE}/MatrixLib/3G"
    "${MATRIXGAME_ENGINE_SOURCE}/MatrixLib/Base"
    "${MATRIXGAME_ENGINE_SOURCE}/MatrixLib/Bitmap"
    "${MATRIXGAME_ENGINE_SOURCE}/MatrixGame/src"
    "${CMAKE_CURRENT_LIST_DIR}/../native-engine")
if(MSVC)
    target_compile_options(matrixgame-original-config-contract PRIVATE /UNDEBUG /Gy /Gw)
    target_link_options(matrixgame-original-config-contract PRIVATE /OPT:REF)
else()
    target_compile_options(matrixgame-native-core PRIVATE -ffunction-sections -fdata-sections)
    target_compile_options(matrixgame-original-config-contract PRIVATE -UNDEBUG -ffunction-sections -fdata-sections -fno-pie)
    target_link_options(matrixgame-original-config-contract PRIVATE -Wl,--gc-sections -no-pie)
endif()
