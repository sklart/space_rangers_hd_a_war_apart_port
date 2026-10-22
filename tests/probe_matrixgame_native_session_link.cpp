// Actual offscreen device + cache + original VO + texture + draw path.
// SDK link evidence only until explicitly executed on physical Switch.
#include "../port/matrixgame/deko/native_session.hpp"
#include "mesh_shaders.hpp"
extern "C" void MatrixGameNativeSessionDraw(const wchar_t* model,const wchar_t* texture) {
    using namespace srhd_awa::matrixgame;
    NativeDekoSession session;
    for(int cycle=0;cycle<2;++cycle) {
        session.Begin(64,64,std::as_bytes(std::span(vertex_dksh)),std::as_bytes(std::span(fragment_dksh)));
        auto& object=session.LoadObject(model);
        const auto material=session.LoadTexture(texture);
        MeshDraw draw;
        draw.clip_from_model={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
        draw.normal_from_model=draw.clip_from_model;
        std::vector<CachedFrameMaterial> materials(object.NativeSurfaceCount(),{material,{draw,1,{},{}}});
        std::vector<std::byte> readback(64*64*4);
        session.Draw(object,0,materials,readback);
        session.Deinit();
    }
}
extern "C" void MatrixGameNativeSkinDraw(const wchar_t* model) {
    using namespace srhd_awa::matrixgame;
    NativeDekoSession session;
    session.Begin(64,64,std::as_bytes(std::span(vertex_dksh)),std::as_bytes(std::span(fragment_dksh)));
    auto& object=session.LoadObject(model);
    MeshDraw draw;draw.clip_from_model={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    draw.normal_from_model=draw.clip_from_model;
    std::vector<std::byte> output(64*64*4);
    session.TaktSkins(16);
    session.DrawSurfaceTextures(object,0,draw,output);
    session.Deinit();
}
