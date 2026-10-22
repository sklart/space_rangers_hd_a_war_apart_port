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
    "${PORT_ROOT}/tools/matrixgame_lifecycle_overrides.py"
    "${PORT_ROOT}/tools/matrixgame_big_buffer_overrides.py"
    "${PORT_ROOT}/tools/matrixgame_list_overrides.py"
    "${PORT_ROOT}/upstream/matrixgame/MatrixLib/Base/CList.cpp"
    "${PORT_ROOT}/upstream/matrixgame/MatrixLib/Base/CList.hpp"
    "${PORT_ROOT}/upstream/matrixgame/MatrixLib/3G/BigVB.hpp"
    "${PORT_ROOT}/upstream/matrixgame/MatrixLib/3G/BigIB.hpp"
    "${PORT_ROOT}/upstream/matrixgame/MatrixLib/Base/CMain.cpp"
    "${PORT_ROOT}/upstream/matrixgame/MatrixLib/3G/Form.cpp"
    "${PORT_ROOT}/upstream/matrixgame/MatrixLib/3G/Form.hpp"
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
    "${PORT_ROOT}/tools/matrixgame_blockpar_overrides.py"
    "${PORT_ROOT}/tools/matrixgame_math_overrides.py"
    "${PORT_ROOT}/tools/matrixgame_random_overrides.py"
    "${PORT_ROOT}/tools/matrixgame_engine_api_overrides.py"
    "${PORT_ROOT}/tools/matrixgame_vector_overrides.py"
    "${PORT_ROOT}/tools/matrixgame_vector_object_overrides.py"
    "${PORT_ROOT}/tools/matrixgame_cache_overrides.py"
    "${PORT_ROOT}/tools/matrixgame_3g_lifecycle_overrides.py"
    "${PORT_ROOT}/tools/matrixgame_platform_3g_overrides.py"
    "${PORT_ROOT}/tools/matrixgame_reminder_overrides.py"
    "${PORT_ROOT}/tools/matrixgame_bitmap_overrides.py"
    "${CMAKE_CURRENT_LIST_DIR}/bitmap_limits.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/bitmap_native.inc"
    "${CMAKE_CURRENT_LIST_DIR}/png_native.cpp"
    "${PORT_ROOT}/upstream/matrixgame/MatrixLib/Bitmap/CBitmap.hpp"
    "${PORT_ROOT}/upstream/matrixgame/MatrixLib/Bitmap/CBitmap.cpp"
    "${PORT_ROOT}/upstream/matrixgame/MatrixLib/Bitmap/FilePNG.hpp"
    "${PORT_ROOT}/upstream/matrixgame/MatrixLib/Bitmap/FilePNG.cpp"
    "${PORT_ROOT}/upstream/matrixgame/MatrixLib/Base/CReminder.hpp"
    "${PORT_ROOT}/upstream/matrixgame/MatrixLib/Base/CReminder.cpp"
    "${PORT_ROOT}/upstream/matrixgame/MatrixLib/3G/VectorObject.hpp"
    "${PORT_ROOT}/upstream/matrixgame/MatrixLib/3G/VectorObject.cpp"
    "${PORT_ROOT}/upstream/matrixgame/MatrixGame/src/MatrixGameDll.hpp"
    "${PORT_ROOT}/upstream/matrixgame/MatrixGame/src/MatrixGame.h"
    "${PORT_ROOT}/upstream/matrixgame/MatrixLib/Base/random.hpp"
    "${PORT_ROOT}/upstream/matrixgame/MatrixLib/Base/random.cpp"
    "${PORT_ROOT}/upstream/matrixgame/MatrixLib/3G/Math3D.hpp"
    "${PORT_ROOT}/upstream/matrixgame/MatrixLib/3G/Math3D.cpp"
    "${PORT_ROOT}/upstream/matrixgame/MatrixLib/Base/CBlockPar.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/storage_zl03.inc"
    "${CMAKE_CURRENT_LIST_DIR}/storage_preflight.inc"
    "${PORT_ROOT}/upstream/matrixgame/MatrixLib/Base/CStorage.hpp"
    "${PORT_ROOT}/upstream/matrixgame/MatrixLib/Base/CStorage.cpp"
    "${PORT_ROOT}/upstream/matrixgame/MatrixLib/Base/CHeap.hpp"
    "${PORT_ROOT}/upstream/matrixgame/MatrixLib/Base/CBlockPar.hpp"
    "${PORT_ROOT}/upstream/matrixgame/MatrixLib/Base/utils.hpp")
file(GLOB_RECURSE CORE_MATH_VENDOR_FILES "${PORT_ROOT}/port/matrixgame/third_party/*")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CORE_MATH_VENDOR_FILES})
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${PORT_ROOT}/tools/matrixgame_texture_request_overrides.py"
    "${PORT_ROOT}/upstream/matrixgame/MatrixLib/3G/Texture.cpp"
    "${PORT_ROOT}/upstream/matrixgame/MatrixLib/3G/Texture.hpp"
    "${PORT_ROOT}/upstream/matrixgame/MatrixLib/3G/Cache.cpp")
add_library(matrixgame-native-core STATIC texture_asset.cpp vector_geometry.cpp "${CORE_SOURCE}/CBitmap.cpp" "${CORE_SOURCE}/FilePNG.cpp" "${CORE_SOURCE}/CReminder.cpp" "${CORE_SOURCE}/CRC32.cpp"
    "${CORE_SOURCE}/CMain.cpp" "${CORE_SOURCE}/Form.cpp"
    buffer_resources.cpp buffer_geometry.cpp skin_callbacks.cpp
    "${CORE_SOURCE}/CBuf.cpp" "${CORE_SOURCE}/CBuf_file.cpp"
    "${CORE_SOURCE}/random.cpp" "${CORE_SOURCE}/Math3D.cpp" "${CORE_SOURCE}/CStorage.cpp" "${CORE_SOURCE}/CStorage_blockpar.cpp" "${CORE_SOURCE}/CBlockPar.cpp" heap_native.cpp cbuf_utf16.cpp exception_native.cpp file_native.cpp "${PORT_ROOT}/port/switch/platform/package.cpp")
target_include_directories(matrixgame-native-core PUBLIC "${CORE_SOURCE}" "${CMAKE_CURRENT_LIST_DIR}")
target_sources(matrixgame-native-core PRIVATE "${CORE_SOURCE}/CList.cpp")
target_sources(matrixgame-native-core PRIVATE texture_request.cpp "${CORE_SOURCE}/TextureFlags.cpp")
target_sources(matrixgame-native-core PRIVATE jpeg_texture.cpp)
target_sources(matrixgame-native-core PRIVATE texture_mips.cpp "${CORE_SOURCE}/BC.cpp")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${PORT_ROOT}/tools/matrixgame_bc_prepare.py" "${CMAKE_CURRENT_LIST_DIR}/bc_native_prelude.hpp")
target_compile_features(matrixgame-native-core PUBLIC cxx_std_20)
target_include_directories(matrixgame-native-core PUBLIC "${PORT_ROOT}/port/matrixgame/third_party/directxmath/Inc")
if(MSVC)
    target_compile_options(matrixgame-native-core PUBLIC /fp:precise)
    set_source_files_properties("${CORE_SOURCE}/BC.cpp" PROPERTIES COMPILE_OPTIONS /fp:strict)
else()
    target_include_directories(matrixgame-native-core PUBLIC "${PORT_ROOT}/port/matrixgame/third_party/sal")
    target_compile_definitions(matrixgame-native-core PUBLIC _POSIX_C_SOURCE=200809L)
    target_compile_options(matrixgame-native-core PUBLIC -ffp-contract=off)
    set_source_files_properties("${CORE_SOURCE}/BC.cpp" PROPERTIES COMPILE_OPTIONS -frounding-math)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        # GCC can sink the quantization arithmetic past the FPU guard's
        # destructor even with -frounding-math. Keep it inside the scope.
        set_property(SOURCE "${CORE_SOURCE}/BC.cpp" APPEND PROPERTY COMPILE_OPTIONS -fno-tree-sink)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set_property(SOURCE "${CORE_SOURCE}/BC.cpp" APPEND PROPERTY COMPILE_OPTIONS -ffp-model=strict)
    endif()
endif()

find_package(ZLIB REQUIRED)
find_package(PNG REQUIRED)
find_package(JPEG REQUIRED)
target_link_libraries(matrixgame-native-core PUBLIC ZLIB::ZLIB PNG::PNG JPEG::JPEG)
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
    add_executable(matrixgame-native-list-test "${PORT_ROOT}/tests/test_matrixgame_native_list.cpp")
    target_link_libraries(matrixgame-native-list-test PRIVATE matrixgame-native-core)
    target_compile_options(matrixgame-native-list-test PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
    add_test(NAME matrixgame-native-list-compatibility COMMAND matrixgame-native-list-test)
    add_executable(matrixgame-native-big-buffer-test "${PORT_ROOT}/tests/test_matrixgame_big_buffers.cpp")
    target_link_libraries(matrixgame-native-big-buffer-test PRIVATE matrixgame-native-core)
    target_compile_options(matrixgame-native-big-buffer-test PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
    add_test(NAME matrixgame-native-big-buffer-contract COMMAND matrixgame-native-big-buffer-test)
    if(UNIX AND NOT CMAKE_CROSSCOMPILING AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_executable(matrixgame-native-buffer-fault-test "${PORT_ROOT}/tests/test_matrixgame_buffer_faults.cpp")
        target_link_libraries(matrixgame-native-buffer-fault-test PRIVATE matrixgame-native-core)
        target_compile_options(matrixgame-native-buffer-fault-test PRIVATE -UNDEBUG)
        target_link_options(matrixgame-native-buffer-fault-test PRIVATE -Wl,--wrap=calloc -Wl,--wrap=realloc)
        add_test(NAME matrixgame-native-buffer-allocation-fault COMMAND matrixgame-native-buffer-fault-test)
    endif()
    add_executable(matrixgame-native-lifecycle-test "${PORT_ROOT}/tests/test_matrixgame_native_lifecycle.cpp")
    target_link_libraries(matrixgame-native-lifecycle-test PRIVATE matrixgame-native-core)
    target_compile_options(matrixgame-native-lifecycle-test PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
    add_test(NAME matrixgame-native-lifecycle-contract COMMAND matrixgame-native-lifecycle-test)
    add_executable(matrixgame-native-mips-test "${PORT_ROOT}/tests/test_matrixgame_native_mips.cpp")
    target_link_libraries(matrixgame-native-mips-test PRIVATE matrixgame-native-core)
    add_test(NAME matrixgame-native-mips-contract COMMAND "${Python3_EXECUTABLE}"
        "${PORT_ROOT}/tests/probe_matrixgame_native_mips.py"
        --binary "$<TARGET_FILE:matrixgame-native-mips-test>" --output "${CMAKE_CURRENT_BINARY_DIR}/mip-contract-fixtures")
    add_executable(matrixgame-sampler-state-test "${PORT_ROOT}/tests/test_matrixgame_sampler_state.cpp")
    target_link_libraries(matrixgame-sampler-state-test PRIVATE matrixgame-native-core)
    add_test(NAME matrixgame-sampler-state-contract COMMAND "${Python3_EXECUTABLE}"
        "${PORT_ROOT}/tests/probe_matrixgame_sampler_state.py"
        --binary "$<TARGET_FILE:matrixgame-sampler-state-test>" --output "${CMAKE_CURRENT_BINARY_DIR}/sampler-state-fixtures")
    add_executable(matrixgame-raster-state-test "${PORT_ROOT}/tests/test_matrixgame_raster_state.cpp")
    target_link_libraries(matrixgame-raster-state-test PRIVATE matrixgame-native-core)
    add_test(NAME matrixgame-raster-state-contract COMMAND "${Python3_EXECUTABLE}"
        "${PORT_ROOT}/tests/probe_matrixgame_raster_state.py"
        --binary "$<TARGET_FILE:matrixgame-raster-state-test>" --output "${CMAKE_CURRENT_BINARY_DIR}/raster-state-fixtures")
    add_executable(matrixgame-native-texture-request-test "${PORT_ROOT}/tests/test_matrixgame_native_texture_request.cpp")
    target_link_libraries(matrixgame-native-texture-request-test PRIVATE matrixgame-native-core)
    add_test(NAME matrixgame-native-texture-request-contract COMMAND "${Python3_EXECUTABLE}"
        "${PORT_ROOT}/tests/probe_matrixgame_native_texture_request.py"
        --binary "$<TARGET_FILE:matrixgame-native-texture-request-test>" --output "${CMAKE_CURRENT_BINARY_DIR}/texture-request-fixtures")
    add_executable(matrixgame-native-texture-test "${PORT_ROOT}/tests/test_matrixgame_native_texture.cpp")
    target_link_libraries(matrixgame-native-texture-test PRIVATE matrixgame-native-core)
    add_test(NAME matrixgame-native-texture-contract COMMAND "${Python3_EXECUTABLE}"
        "${PORT_ROOT}/tests/probe_matrixgame_native_texture.py"
        --binary "$<TARGET_FILE:matrixgame-native-texture-test>" --output "${CMAKE_CURRENT_BINARY_DIR}/texture-fixtures")
    add_executable(matrixgame-native-bitmap-test "${PORT_ROOT}/tests/test_matrixgame_native_bitmap.cpp")
    target_link_libraries(matrixgame-native-bitmap-test PRIVATE matrixgame-native-core)
    add_test(NAME matrixgame-native-bitmap-contract COMMAND "${Python3_EXECUTABLE}"
        "${PORT_ROOT}/tests/probe_matrixgame_native_bitmap.py"
        --binary "$<TARGET_FILE:matrixgame-native-bitmap-test>" --output "${CMAKE_CURRENT_BINARY_DIR}/bitmap-fixtures")
    if(UNIX AND NOT CMAKE_CROSSCOMPILING AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_executable(matrixgame-native-bitmap-fault-test "${PORT_ROOT}/tests/test_matrixgame_native_bitmap.cpp")
        target_compile_definitions(matrixgame-native-bitmap-fault-test PRIVATE MATRIXGAME_BITMAP_FAULT_INJECTION)
        target_link_libraries(matrixgame-native-bitmap-fault-test PRIVATE matrixgame-native-core)
        target_link_options(matrixgame-native-bitmap-fault-test PRIVATE -Wl,--wrap=calloc -Wl,--wrap=realloc)
        add_test(NAME matrixgame-native-bitmap-allocation-fault COMMAND "${Python3_EXECUTABLE}"
            "${PORT_ROOT}/tests/probe_matrixgame_native_bitmap.py"
            --binary "$<TARGET_FILE:matrixgame-native-bitmap-fault-test>" --output "${CMAKE_CURRENT_BINARY_DIR}/bitmap-fault-fixtures")
    endif()
    add_executable(matrixgame-native-reminder-test "${PORT_ROOT}/tests/test_matrixgame_native_reminder.cpp")
    target_link_libraries(matrixgame-native-reminder-test PRIVATE matrixgame-native-core)
    add_test(NAME matrixgame-native-reminder-contract COMMAND "${Python3_EXECUTABLE}"
        "${PORT_ROOT}/tests/probe_matrixgame_native_reminder.py"
        --binary "$<TARGET_FILE:matrixgame-native-reminder-test>" --output "${CMAKE_CURRENT_BINARY_DIR}/reminder-fixtures")
    add_executable(matrixgame-native-vector-test "${PORT_ROOT}/tests/test_matrixgame_native_vector.cpp")
    target_link_libraries(matrixgame-native-vector-test PRIVATE matrixgame-native-core)
    add_test(NAME matrixgame-native-vector-contract COMMAND "${Python3_EXECUTABLE}"
        "${PORT_ROOT}/tests/probe_matrixgame_native_vector.py"
        --binary "$<TARGET_FILE:matrixgame-native-vector-test>" --output "${CMAKE_CURRENT_BINARY_DIR}/vector-fixtures")
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
    add_executable(matrixgame-native-random-test "${PORT_ROOT}/tests/test_matrixgame_native_random.cpp")
    target_link_libraries(matrixgame-native-random-test PRIVATE matrixgame-native-core)
    add_test(NAME matrixgame-native-random-contract COMMAND "${Python3_EXECUTABLE}"
        "${PORT_ROOT}/tests/probe_matrixgame_native_random.py"
        --binary "$<TARGET_FILE:matrixgame-native-random-test>" --output "${CMAKE_CURRENT_BINARY_DIR}/random-fixtures")
    add_executable(matrixgame-native-math-test "${PORT_ROOT}/tests/test_matrixgame_native_math.cpp")
    target_link_libraries(matrixgame-native-math-test PRIVATE matrixgame-native-core)
    add_test(NAME matrixgame-native-math-contract COMMAND "${Python3_EXECUTABLE}"
        "${PORT_ROOT}/tests/probe_matrixgame_native_math.py"
        --binary "$<TARGET_FILE:matrixgame-native-math-test>" --output "${CMAKE_CURRENT_BINARY_DIR}/math-fixtures")
    add_executable(matrixgame-native-blockpar-test "${PORT_ROOT}/tests/test_matrixgame_native_blockpar.cpp")
    target_link_libraries(matrixgame-native-blockpar-test PRIVATE matrixgame-native-core)
    add_test(NAME matrixgame-native-blockpar-contract COMMAND "${Python3_EXECUTABLE}"
        "${PORT_ROOT}/tests/probe_matrixgame_native_blockpar.py"
        --binary "$<TARGET_FILE:matrixgame-native-blockpar-test>" --output "${CMAKE_CURRENT_BINARY_DIR}/blockpar-fixtures")
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

option(CORE_BUILD_LINK_PROBE "Link CPU resource/configuration contract ELFs for dependency proof only; do not run/package" OFF)
if(CORE_BUILD_LINK_PROBE)
    if(NOT CMAKE_CROSSCOMPILING OR NOT MATRIXGAME_DEVKITPRO)
        message(FATAL_ERROR "CORE_BUILD_LINK_PROBE requires the Switch toolchain")
    endif()
    add_executable(matrixgame-native-bitmap-link-probe "${PORT_ROOT}/tests/test_matrixgame_native_bitmap.cpp")
    target_link_libraries(matrixgame-native-bitmap-link-probe PRIVATE matrixgame-native-core nx)
    target_link_directories(matrixgame-native-bitmap-link-probe PRIVATE "${MATRIXGAME_DEVKITPRO}/libnx/lib")
    target_link_options(matrixgame-native-bitmap-link-probe PRIVATE
        "-specs=${MATRIXGAME_DEVKITPRO}/libnx/switch.specs" -fPIE -Wl,--gc-sections -Wl,--build-id=sha1)
    set_target_properties(matrixgame-native-bitmap-link-probe PROPERTIES SUFFIX ".elf")
    add_executable(matrixgame-native-reminder-link-probe "${PORT_ROOT}/tests/test_matrixgame_native_reminder.cpp")
    target_link_libraries(matrixgame-native-reminder-link-probe PRIVATE matrixgame-native-core nx)
    target_link_directories(matrixgame-native-reminder-link-probe PRIVATE "${MATRIXGAME_DEVKITPRO}/libnx/lib")
    target_link_options(matrixgame-native-reminder-link-probe PRIVATE
        "-specs=${MATRIXGAME_DEVKITPRO}/libnx/switch.specs" -fPIE -Wl,--gc-sections -Wl,--build-id=sha1)
    set_target_properties(matrixgame-native-reminder-link-probe PROPERTIES SUFFIX ".elf")
    add_executable(matrixgame-native-vector-link-probe "${PORT_ROOT}/tests/test_matrixgame_native_vector.cpp")
    target_link_libraries(matrixgame-native-vector-link-probe PRIVATE matrixgame-native-core nx)
    target_link_directories(matrixgame-native-vector-link-probe PRIVATE "${MATRIXGAME_DEVKITPRO}/libnx/lib")
    target_link_options(matrixgame-native-vector-link-probe PRIVATE
        "-specs=${MATRIXGAME_DEVKITPRO}/libnx/switch.specs" -fPIE -Wl,--gc-sections -Wl,--build-id=sha1)
    set_target_properties(matrixgame-native-vector-link-probe PROPERTIES SUFFIX ".elf")
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
    add_executable(matrixgame-native-blockpar-link-probe "${PORT_ROOT}/tests/test_matrixgame_native_blockpar.cpp")
    target_link_libraries(matrixgame-native-blockpar-link-probe PRIVATE matrixgame-native-core nx)
    target_link_directories(matrixgame-native-blockpar-link-probe PRIVATE "${MATRIXGAME_DEVKITPRO}/libnx/lib")
    target_link_options(matrixgame-native-blockpar-link-probe PRIVATE
        "-specs=${MATRIXGAME_DEVKITPRO}/libnx/switch.specs" -fPIE -Wl,--gc-sections -Wl,--build-id=sha1)
    set_target_properties(matrixgame-native-blockpar-link-probe PROPERTIES SUFFIX ".elf")
    add_executable(matrixgame-native-random-link-probe "${PORT_ROOT}/tests/test_matrixgame_native_random.cpp")
    target_compile_definitions(matrixgame-native-random-link-probe PRIVATE MATRIXGAME_RANDOM_NO_THREADS)
    target_link_libraries(matrixgame-native-random-link-probe PRIVATE matrixgame-native-core nx)
    target_link_directories(matrixgame-native-random-link-probe PRIVATE "${MATRIXGAME_DEVKITPRO}/libnx/lib")
    target_link_options(matrixgame-native-random-link-probe PRIVATE
        "-specs=${MATRIXGAME_DEVKITPRO}/libnx/switch.specs" -fPIE -Wl,--gc-sections -Wl,--build-id=sha1)
    set_target_properties(matrixgame-native-random-link-probe PROPERTIES SUFFIX ".elf")
    add_executable(matrixgame-native-math-link-probe "${PORT_ROOT}/tests/test_matrixgame_native_math.cpp")
    target_link_libraries(matrixgame-native-math-link-probe PRIVATE matrixgame-native-core nx)
    target_link_directories(matrixgame-native-math-link-probe PRIVATE "${MATRIXGAME_DEVKITPRO}/libnx/lib")
    target_link_options(matrixgame-native-math-link-probe PRIVATE
        "-specs=${MATRIXGAME_DEVKITPRO}/libnx/switch.specs" -fPIE -Wl,--gc-sections -Wl,--build-id=sha1)
    set_target_properties(matrixgame-native-math-link-probe PROPERTIES SUFFIX ".elf")
endif()
