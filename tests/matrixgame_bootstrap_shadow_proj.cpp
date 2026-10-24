#ifdef NDEBUG
#error Shadow owner verification requires assertions
#endif
#include "init_runtime.hpp"
#include "MatrixGame.h"
#include "ShadowProj.hpp"
#include "ShadowStencil.hpp"
#include "legacy_texture_backend.hpp"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <utility>
using namespace srhd_awa::matrixgame;
namespace {
// Observe protected original staging; no fields, overrides or fake map/device.
class ShadowProbe final:public CVOShadowProj {
public:
    using CVOShadowProj::CVOShadowProj;
    static bool PoolsEmpty(){return !m_VB && !m_IB;}
    std::pair<int,int> Select(){return {m_preVB.Select(m_VB),m_preIB.Select(m_IB)};}
};
}
void VerifyBootstrapShadowProj(NativeInitRuntime& runtime,CVectorObject& object) {
    assert(runtime.CpuReady() && !runtime.GpuReady() && g_MatrixHeap);
    // The caller may retain earlier bindings. The engine allows one Context;
    // release only those bindings before measuring this owner's allocations.
    auto& context=buffers::Current();context.ClearBindings();
    assert(!context.Vertex().buffer && !context.Index() && ShadowProbe::PoolsEmpty());
    const auto allocated=context.AllocatedBytes(), textures=legacy_texture::RegistrySize();
    const auto& geometry=CVOShadowStencil::ReadSourceGeometry(object);
    assert(geometry.m_Vertices.size>=4*int(sizeof(SVOVertex)));
    std::array<SVOShadowProjVertex,4> vertices{};
    for(int i=0;i<4;++i) {
        vertices[i].v=geometry.m_Vertices.verts[i].v;
        vertices[i].tu=float(i&1);vertices[i].tv=float(i>>1);
    }
    // A lifecycle input from real VO positions; this is not terrain projection.
    std::array<std::uint16_t,4> indices{0,1,2,3};
    for(int order=0;order<2;++order) {
        auto* first=HNew(g_MatrixHeap) ShadowProbe(g_MatrixHeap);
        auto* second=HNew(g_MatrixHeap) ShadowProbe(g_MatrixHeap);
        assert(!ShadowProbe::PoolsEmpty());
        first->DX_Prepare();first->DX_Free();first->RenderMin();
        first->Prepare(1,2,vertices.data(),4,indices.data(),4);
        first->Prepare(3,4,vertices.data(),4,indices.data(),4);
        second->Prepare(5,6,vertices.data(),4,indices.data(),4);
        assert(first->GetDX()==3 && first->GetDY()==4);
        auto* survivor=order?first:second;
        HDelete(ShadowProbe,order?second:first,g_MatrixHeap);
        survivor->DX_Prepare();
        const auto [vbase,ibase]=survivor->Select();assert(vbase>=0 && ibase>=0);
        const auto* vb=context.Vertex().buffer;const auto* ib=context.Index();
        const auto voffset=std::size_t(vbase)*sizeof(SVOShadowProjVertex);
        const auto ioffset=std::size_t(ibase)*sizeof(std::uint16_t);
        assert(vb && ib && vb->Initialized(voffset,sizeof(vertices)) && ib->Initialized(ioffset,sizeof(indices)));
        assert(!std::memcmp(vb->Storage().data()+voffset,vertices.data(),sizeof(vertices)));
        assert(!std::memcmp(ib->Storage().data()+ioffset,indices.data(),sizeof(indices)));
        bool refused=false;
        try{survivor->RenderMin();}catch(const platform3g::Unsupported&){refused=true;}
        assert(refused); // Original draw reaches the genuine absent-device boundary.
        context.ClearBindings();
        survivor->DX_Free();survivor->DX_Free();
        CVOShadowProj::MarkAllBuffersNoNeed();survivor->DX_Prepare();
        auto* texture=legacy_texture::CreateManaged();texture->RefInc();texture->RefInc();
        survivor->SetTexture(texture);
        survivor->Prepare(0,0,nullptr,0,nullptr,0);
        assert(!survivor->IsProjected() && !survivor->GetTexture() && texture->Ref()==1);
        assert(!ShadowProbe::PoolsEmpty());
        survivor->DX_Free();survivor->DX_Prepare();survivor->RenderMin();
        survivor->SetTexture(texture);survivor->DestroyTexture();survivor->DestroyTexture();
        assert(legacy_texture::RegistrySize()==textures);
        survivor->Prepare(7,8,vertices.data(),4,indices.data(),4);
        HDelete(ShadowProbe,survivor,g_MatrixHeap);
        auto* empty=HNew(g_MatrixHeap) ShadowProbe(g_MatrixHeap);
        HDelete(ShadowProbe,empty,g_MatrixHeap);
        assert(ShadowProbe::PoolsEmpty() && context.AllocatedBytes()==allocated);
    }
    assert(legacy_texture::RegistrySize()==textures);
    std::puts("original shadow proj replacement/empty/shared staging/texture refs/cleanup/reentry EXECUTED; actual device refused draw; world NOT EXECUTED");
}
