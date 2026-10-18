cmake_minimum_required(VERSION 3.24)
project(SRHDMatrixGameNativeProbe LANGUAGES C CXX)
if(NOT MSVC OR NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "The native probe requires MSVC Windows x86 (-A Win32)")
endif()
set(MATRIXGAME_SOURCE_DIR "" CACHE PATH "Disposable pinned MatrixGame snapshot with the selected patch")
if(NOT EXISTS "${MATRIXGAME_SOURCE_DIR}/MatrixGame/src/MatrixGameDll.cpp")
    message(FATAL_ERROR "Pass an explicit MATRIXGAME_SOURCE_DIR; never edit the pinned submodule")
endif()
set(MATRIXGAME_BUILD_DLL OFF CACHE BOOL "Native EXE initialization creates its own D3D device" FORCE)
set(MATRIXGAME_CHEATS OFF CACHE BOOL "Do not enable gameplay cheats" FORCE)
set(MATRIXGAME_PKG_BRING_FROM_GAME OFF CACHE BOOL "Resources are staged separately" FORCE)
add_subdirectory("${MATRIXGAME_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/engine")
add_executable(matrixgame-ai-template-probe
    "${CMAKE_CURRENT_LIST_DIR}/../../../tests/probe_matrixgame_ai_templates_x86.cpp"
    "${MATRIXGAME_SOURCE_DIR}/MatrixGame/src/MatrixGameDll.cpp")
target_link_libraries(matrixgame-ai-template-probe PRIVATE MatrixGameInternal)
target_compile_features(matrixgame-ai-template-probe PRIVATE cxx_std_20)
# MatrixGameInternal inherits the legacy WINDOWS subsystem link flag. This
# integration probe uses main(), redirects its report, and sends no UI input.
target_link_options(matrixgame-ai-template-probe PRIVATE /ENTRY:mainCRTStartup)

add_executable(matrixgame-mesh-layout-probe
    "${CMAKE_CURRENT_LIST_DIR}/../../../tests/probe_matrixgame_mesh_layout_x86.cpp")
target_link_libraries(matrixgame-mesh-layout-probe PRIVATE MatrixGameInternal)
target_compile_features(matrixgame-mesh-layout-probe PRIVATE cxx_std_20)
target_link_options(matrixgame-mesh-layout-probe PRIVATE /ENTRY:mainCRTStartup)

# Original unmodified MatrixLib CRC is the Win32 arithmetic reference.
add_executable(matrixgame-crc-x86-reference
    "${CMAKE_CURRENT_LIST_DIR}/../../../tests/test_matrixgame_native_crc.cpp"
    "${MATRIXGAME_SOURCE_DIR}/MatrixLib/Base/CRC32.cpp")
target_include_directories(matrixgame-crc-x86-reference PRIVATE "${MATRIXGAME_SOURCE_DIR}/MatrixLib/Base")
target_compile_features(matrixgame-crc-x86-reference PRIVATE cxx_std_20)
target_compile_options(matrixgame-crc-x86-reference PRIVATE /UNDEBUG)

# Byte-for-byte baseline for original UTF-16 resource buffer semantics.
add_executable(matrixgame-buffer-x86-reference
    "${CMAKE_CURRENT_LIST_DIR}/../../../tests/test_matrixgame_native_buffer.cpp")
target_include_directories(matrixgame-buffer-x86-reference PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../native-core")
target_link_libraries(matrixgame-buffer-x86-reference PRIVATE MatrixGameInternal)
target_compile_features(matrixgame-buffer-x86-reference PRIVATE cxx_std_20)
target_compile_definitions(matrixgame-buffer-x86-reference PRIVATE MATRIXGAME_ORIGINAL_BUFFER_REFERENCE)
target_compile_options(matrixgame-buffer-x86-reference PRIVATE /UNDEBUG)
target_link_options(matrixgame-buffer-x86-reference PRIVATE /ENTRY:mainCRTStartup)

add_executable(matrixgame-file-x86-reference
    "${CMAKE_CURRENT_LIST_DIR}/../../../tests/test_matrixgame_native_file.cpp")
target_link_libraries(matrixgame-file-x86-reference PRIVATE MatrixGameInternal)
target_compile_features(matrixgame-file-x86-reference PRIVATE cxx_std_20)
target_compile_definitions(matrixgame-file-x86-reference PRIVATE MATRIXGAME_ORIGINAL_FILE_REFERENCE)
target_compile_options(matrixgame-file-x86-reference PRIVATE /UNDEBUG)
target_link_options(matrixgame-file-x86-reference PRIVATE /ENTRY:mainCRTStartup)

add_executable(matrixgame-storage-x86-reference
    "${CMAKE_CURRENT_LIST_DIR}/../../../tests/test_matrixgame_native_storage.cpp")
target_link_libraries(matrixgame-storage-x86-reference PRIVATE MatrixGameInternal)
target_compile_features(matrixgame-storage-x86-reference PRIVATE cxx_std_20)
target_compile_definitions(matrixgame-storage-x86-reference PRIVATE MATRIXGAME_ORIGINAL_STORAGE_REFERENCE)
target_compile_options(matrixgame-storage-x86-reference PRIVATE /UNDEBUG)
target_link_options(matrixgame-storage-x86-reference PRIVATE /ENTRY:mainCRTStartup)

add_executable(matrixgame-blockpar-x86-reference
    "${CMAKE_CURRENT_LIST_DIR}/../../../tests/test_matrixgame_native_blockpar.cpp")
target_include_directories(matrixgame-blockpar-x86-reference PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../native-core")
target_link_libraries(matrixgame-blockpar-x86-reference PRIVATE MatrixGameInternal)
target_compile_features(matrixgame-blockpar-x86-reference PRIVATE cxx_std_20)
target_compile_definitions(matrixgame-blockpar-x86-reference PRIVATE MATRIXGAME_ORIGINAL_BLOCKPAR_REFERENCE)
target_compile_options(matrixgame-blockpar-x86-reference PRIVATE /UNDEBUG)
target_link_options(matrixgame-blockpar-x86-reference PRIVATE /ENTRY:mainCRTStartup)

add_executable(matrixgame-math-x86-reference "${CMAKE_CURRENT_LIST_DIR}/../../../tests/test_matrixgame_native_math.cpp")
target_link_libraries(matrixgame-math-x86-reference PRIVATE MatrixGameInternal)
target_compile_features(matrixgame-math-x86-reference PRIVATE cxx_std_20)
target_compile_definitions(matrixgame-math-x86-reference PRIVATE MATRIXGAME_ORIGINAL_MATH_REFERENCE)
target_compile_options(matrixgame-math-x86-reference PRIVATE /UNDEBUG)
target_link_options(matrixgame-math-x86-reference PRIVATE /ENTRY:mainCRTStartup)

add_executable(matrixgame-random-x86-reference "${CMAKE_CURRENT_LIST_DIR}/../../../tests/test_matrixgame_native_random.cpp")
target_link_libraries(matrixgame-random-x86-reference PRIVATE MatrixGameInternal)
target_compile_features(matrixgame-random-x86-reference PRIVATE cxx_std_20)
target_compile_definitions(matrixgame-random-x86-reference PRIVATE MATRIXGAME_ORIGINAL_RANDOM_REFERENCE)
target_compile_options(matrixgame-random-x86-reference PRIVATE /UNDEBUG)
target_link_options(matrixgame-random-x86-reference PRIVATE /ENTRY:mainCRTStartup)

add_executable(matrixgame-bitmap-x86-reference "${CMAKE_CURRENT_LIST_DIR}/../../../tests/probe_matrixgame_bitmap_x86.cpp")
target_link_libraries(matrixgame-bitmap-x86-reference PRIVATE MatrixGameInternal)
target_compile_features(matrixgame-bitmap-x86-reference PRIVATE cxx_std_20)
target_link_options(matrixgame-bitmap-x86-reference PRIVATE /ENTRY:mainCRTStartup)
add_executable(matrixgame-texture-request-x86-reference "${CMAKE_CURRENT_LIST_DIR}/../../../tests/test_matrixgame_native_texture_request.cpp")
target_link_libraries(matrixgame-texture-request-x86-reference PRIVATE MatrixGameInternal)
target_compile_definitions(matrixgame-texture-request-x86-reference PRIVATE MATRIXGAME_ORIGINAL_TEXTURE_REFERENCE)
target_compile_features(matrixgame-texture-request-x86-reference PRIVATE cxx_std_20)
target_link_options(matrixgame-texture-request-x86-reference PRIVATE /ENTRY:mainCRTStartup)
add_executable(matrixgame-raster-d3d9-reference "${CMAKE_CURRENT_LIST_DIR}/../../../tests/probe_matrixgame_raster_d3d9.cpp")
target_link_libraries(matrixgame-raster-d3d9-reference PRIVATE d3d9 user32)
target_compile_features(matrixgame-raster-d3d9-reference PRIVATE cxx_std_20)
add_executable(matrixgame-jpeg-d3dx-reference "${CMAKE_CURRENT_LIST_DIR}/../../../tests/probe_matrixgame_jpeg_d3dx.cpp")
target_link_libraries(matrixgame-jpeg-d3dx-reference PRIVATE MatrixGameInternal)
target_compile_features(matrixgame-jpeg-d3dx-reference PRIVATE cxx_std_20)
target_link_options(matrixgame-jpeg-d3dx-reference PRIVATE /ENTRY:mainCRTStartup)
add_executable(matrixgame-a8-d3d9-reference "${CMAKE_CURRENT_LIST_DIR}/../../../tests/probe_matrixgame_a8_d3d9.cpp")
target_link_libraries(matrixgame-a8-d3d9-reference PRIVATE d3d9 user32)
target_compile_features(matrixgame-a8-d3d9-reference PRIVATE cxx_std_20)

add_executable(matrixgame-sampler-d3d9-reference "${CMAKE_CURRENT_LIST_DIR}/../../../tests/probe_matrixgame_sampler_d3d9.cpp")
target_link_libraries(matrixgame-sampler-d3d9-reference PRIVATE d3d9 user32)
target_compile_features(matrixgame-sampler-d3d9-reference PRIVATE cxx_std_20)

add_executable(matrixgame-sampler-pixels-d3d9-reference "${CMAKE_CURRENT_LIST_DIR}/../../../tests/probe_matrixgame_sampler_pixels_d3d9.cpp")
target_link_libraries(matrixgame-sampler-pixels-d3d9-reference PRIVATE d3d9 user32)
target_compile_features(matrixgame-sampler-pixels-d3d9-reference PRIVATE cxx_std_20)

add_executable(matrixgame-anisotropy-d3d9-reference "${CMAKE_CURRENT_LIST_DIR}/../../../tests/probe_matrixgame_anisotropy_d3d9.cpp")
target_link_libraries(matrixgame-anisotropy-d3d9-reference PRIVATE MatrixGameInternal)
target_compile_features(matrixgame-anisotropy-d3d9-reference PRIVATE cxx_std_20)
target_link_options(matrixgame-anisotropy-d3d9-reference PRIVATE /ENTRY:mainCRTStartup)
