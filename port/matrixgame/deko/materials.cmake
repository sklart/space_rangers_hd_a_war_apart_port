# Texture cascade backend. This target does not advertise full-engine caps.
set(MATRIXGAME_MATERIAL_DIR "${CMAKE_CURRENT_LIST_DIR}")
add_library(matrixgame-material-combiner STATIC "${MATRIXGAME_MATERIAL_DIR}/material_combiner.cpp" "${MATRIXGAME_MATERIAL_DIR}/fixed_vertex.cpp")
target_compile_features(matrixgame-material-combiner PUBLIC cxx_std_20)
target_include_directories(matrixgame-material-combiner PUBLIC "${MATRIXGAME_MATERIAL_DIR}")
if(MATRIXGAME_DEVKITPRO)
    target_sources(matrixgame-material-combiner PRIVATE "${MATRIXGAME_MATERIAL_DIR}/material_program.cpp" "${MATRIXGAME_MATERIAL_DIR}/texture_store.cpp")
    target_include_directories(matrixgame-material-combiner PUBLIC "${MATRIXGAME_DEVKITPRO}/libnx/include")
    target_link_directories(matrixgame-material-combiner PUBLIC "${MATRIXGAME_DEVKITPRO}/libnx/lib")
    target_link_libraries(matrixgame-material-combiner PUBLIC deko3d nx)
    if(NOT Python3_EXECUTABLE)
        find_package(Python3 REQUIRED COMPONENTS Interpreter)
    endif()
    set(MATRIXGAME_MATERIAL_SHADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/material-shaders")
    target_include_directories(matrixgame-material-combiner PUBLIC "${MATRIXGAME_MATERIAL_SHADER_DIR}")
    add_custom_command(OUTPUT "${MATRIXGAME_MATERIAL_SHADER_DIR}/material_shaders.hpp"
        "${MATRIXGAME_MATERIAL_SHADER_DIR}/material.frag.dksh" "${MATRIXGAME_MATERIAL_SHADER_DIR}/material.vert.dksh"
        COMMAND "${Python3_EXECUTABLE}" "${PORT_ROOT}/tools/matrixgame_material_shader_build.py"
            --sdk "${MATRIXGAME_DEVKITPRO}" --output "${MATRIXGAME_MATERIAL_SHADER_DIR}"
        DEPENDS "${MATRIXGAME_MATERIAL_DIR}/material.frag.glsl" "${MATRIXGAME_MATERIAL_DIR}/material.vert.glsl"
            "${PORT_ROOT}/tools/matrixgame_material_shader_build.py" VERBATIM)
    add_custom_target(matrixgame-material-shaders DEPENDS "${MATRIXGAME_MATERIAL_SHADER_DIR}/material_shaders.hpp")
    add_dependencies(matrixgame-material-combiner matrixgame-material-shaders)
endif()
if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    set(MATRIXGAME_MATERIAL_FIXTURE "${CMAKE_CURRENT_BINARY_DIR}/material-callbacks")
    add_custom_command(OUTPUT "${MATRIXGAME_MATERIAL_FIXTURE}/original_skin_materials.inc"
            "${MATRIXGAME_MATERIAL_FIXTURE}/original_terrain_materials.inc"
        COMMAND "${Python3_EXECUTABLE}" "${PORT_ROOT}/tools/matrixgame_material_callbacks_prepare.py" "${MATRIXGAME_MATERIAL_FIXTURE}"
        DEPENDS "${PORT_ROOT}/tools/matrixgame_material_callbacks_prepare.py"
            "${PORT_ROOT}/tools/matrixgame_render_pipeline_overrides.py"
            "${PORT_ROOT}/tools/matrixgame_vector_object_overrides.py"
            "${PORT_ROOT}/upstream/matrixgame/MatrixGame/src/MatrixSkinManager.cpp"
            "${PORT_ROOT}/upstream/matrixgame/MatrixGame/src/MatrixSkinManager.hpp"
            "${PORT_ROOT}/upstream/matrixgame/MatrixGame/src/MatrixRenderPipeline.cpp"
            "${PORT_ROOT}/upstream/matrixgame/MatrixLib/3G/3g.hpp" VERBATIM)
    add_executable(matrixgame-material-contract "${PORT_ROOT}/tests/test_matrixgame_materials.cpp"
        "${MATRIXGAME_MATERIAL_DIR}/../native-engine/platform_3g.cpp"
        "${MATRIXGAME_MATERIAL_FIXTURE}/original_skin_materials.inc"
        "${MATRIXGAME_MATERIAL_FIXTURE}/original_terrain_materials.inc")
    target_include_directories(matrixgame-material-contract PRIVATE "${MATRIXGAME_MATERIAL_FIXTURE}"
        "${MATRIXGAME_MATERIAL_DIR}/../native-engine" "${MATRIXGAME_MATERIAL_DIR}/../native-core")
    target_link_libraries(matrixgame-material-contract PRIVATE matrixgame-material-combiner)
    target_compile_options(matrixgame-material-contract PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
    if(WIN32)
        target_compile_definitions(matrixgame-material-contract PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
        target_link_libraries(matrixgame-material-contract PRIVATE d3d9 user32 gdi32 opengl32)
    endif()
    if(WIN32)
        add_test(NAME matrixgame-material-contract COMMAND matrixgame-material-contract --fragment "${MATRIXGAME_MATERIAL_DIR}/material.frag.glsl")
    else()
        add_test(NAME matrixgame-material-contract COMMAND matrixgame-material-contract)
    endif()
endif()
