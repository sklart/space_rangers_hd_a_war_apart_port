cmake_minimum_required(VERSION 3.24)
project(MatrixGameNavigationReference LANGUAGES C CXX)
if(NOT MSVC OR NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "Original navigation reference requires MSVC Windows x86 (-A Win32)")
endif()
get_filename_component(PORT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(ORIGINAL "${PORT_ROOT}/upstream/matrixgame")
set(ZLIB_SOURCE "${PORT_ROOT}/build/e2e515/deps/zlib-1.3.2" CACHE PATH "Pinned local zlib v1.3.2 source")
find_package(Git REQUIRED)
foreach(SOURCE_SPEC IN ITEMS "${ORIGINAL}|218115a72fa9c5d43269f4ca84a8596a0bcb82f1" "${ZLIB_SOURCE}|da607da739fa6047df13e66a2af6b8bec7c2a498")
    string(REPLACE "|" ";" SOURCE_PAIR "${SOURCE_SPEC}")
    list(GET SOURCE_PAIR 0 SOURCE_DIR)
    list(GET SOURCE_PAIR 1 EXPECTED_REVISION)
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${SOURCE_DIR}" rev-parse HEAD
        OUTPUT_VARIABLE ACTUAL_REVISION OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE REVISION_RESULT)
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${SOURCE_DIR}" status --porcelain
        OUTPUT_VARIABLE DIRTY_SOURCE OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE STATUS_RESULT)
    if(NOT REVISION_RESULT EQUAL 0 OR NOT STATUS_RESULT EQUAL 0 OR NOT ACTUAL_REVISION STREQUAL EXPECTED_REVISION OR DIRTY_SOURCE)
        message(FATAL_ERROR "Pinned reference source is changed or unavailable: ${SOURCE_DIR}")
    endif()
endforeach()
set(ZLIB_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZLIB_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(ZLIB_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ZLIB_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory("${ZLIB_SOURCE}" "${CMAKE_CURRENT_BINARY_DIR}/zlib")
# Compile original files without portable overlays. Original MAXEXP_EXPORTS
# selects disk I/O without the optional package API.
set(REFERENCE_SOURCES
    "${ORIGINAL}/MatrixGame/src/Logic/MatrixRoadNetwork.cpp"
    "${ORIGINAL}/MatrixLib/Base/CBuf.cpp"
    "${ORIGINAL}/MatrixLib/Base/CBlockPar.cpp"
    "${ORIGINAL}/MatrixLib/Base/CStorage.cpp"
    "${ORIGINAL}/MatrixLib/Base/CRC32.cpp"
    "${ORIGINAL}/MatrixLib/Base/CFile.cpp"
    "${ORIGINAL}/MatrixLib/Base/CHeap.cpp"
    "${ORIGINAL}/MatrixLib/Base/CMain.cpp"
    "${ORIGINAL}/MatrixLib/3G/Form.cpp"
    "${ORIGINAL}/MatrixLib/Base/CException.cpp"
    "${ORIGINAL}/MatrixLib/Base/Tracer.cpp")
add_executable(matrixgame-navigation-x86-reference "${PORT_ROOT}/tests/test_matrixgame_navigation.cpp" ${REFERENCE_SOURCES})
add_executable(matrixgame-lifecycle-x86-reference "${PORT_ROOT}/tests/test_matrixgame_native_lifecycle.cpp" ${REFERENCE_SOURCES})
foreach(REFERENCE_TARGET IN ITEMS matrixgame-navigation-x86-reference matrixgame-lifecycle-x86-reference)
target_include_directories(${REFERENCE_TARGET} PRIVATE
    "${ORIGINAL}/MatrixLib/Base" "${ORIGINAL}/MatrixLib/3G" "${ORIGINAL}/MatrixGame/src/Logic"
    "${ZLIB_SOURCE}" "${CMAKE_CURRENT_BINARY_DIR}/zlib")
target_compile_features(${REFERENCE_TARGET} PRIVATE cxx_std_20)
target_compile_definitions(${REFERENCE_TARGET} PRIVATE
    MAXEXP_EXPORTS _RELDEBUG _CRT_SECURE_NO_WARNINGS NOMINMAX)
target_compile_options(${REFERENCE_TARGET} PRIVATE /UNDEBUG /Gy /Gw /fp:precise)
target_link_options(${REFERENCE_TARGET} PRIVATE /OPT:REF)
target_link_libraries(${REFERENCE_TARGET} PRIVATE zlibstatic)
endforeach()
