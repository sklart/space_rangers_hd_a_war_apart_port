// SDK link proof for the real cache -> deko upload ownership path. Not run offline.
#include "cache_texture_upload.hpp"
#include "mesh_shaders.hpp"
extern "C" void MatrixGameCachedMaterialGpuUpload(const srhd_awa::matrixgame::MaterialTexture& material) {
    using namespace srhd_awa::matrixgame;
    const auto vertices=std::as_bytes(std::span(vertex_dksh));
    const auto fragments=std::as_bytes(std::span(fragment_dksh));
    DekoMeshRenderer renderer(64,64,vertices,fragments);
    UploadCachedMaterialTexture(renderer,1,material);
    // Destructor waits for GPU work and releases the actual device resources.
}
