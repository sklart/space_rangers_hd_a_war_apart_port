#ifdef NDEBUG
#error Vector group verification requires assertions
#endif
#include "init_runtime.hpp"
#include "MatrixGame.h"
#include "VectorObject.hpp"
#include "skin_callbacks.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <stdexcept>
using namespace srhd_awa::matrixgame;
namespace {
struct Observation {uintptr_t expected{},vertices{},draw{};unsigned vertex_calls{},draw_calls{};std::uint64_t marker{0x984bca1176de3302ULL};};
Observation* active;
bool Vertex(const SVOVertex&,uintptr_t value) {
    ++active->vertex_calls;active->vertices=value;
    if(value==active->expected)assert(reinterpret_cast<Observation*>(value)->marker==0x984bca1176de3302ULL);
    return false;
}
void User(uintptr_t value) {++active->draw_calls;active->draw=value;if(value==active->expected)assert(reinterpret_cast<Observation*>(value)->marker==0x984bca1176de3302ULL);}
bool Stages(const SSkin*,uintptr_t value,int pass) {assert(pass==0);User(value);return false;}
void Texture(const SSkin*,uintptr_t value,int pass) {assert(pass==0);User(value);}
void Clear(const SSkin*,uintptr_t value) {User(value);}
struct Recorder final:NativeDrawSink {
    unsigned ranges{};
    void SetVertexLayout(std::uint32_t value)override {assert(value==VO_FVF);}
    void Emit(const MeshRange& range)override {assert(range.index_count);++ranges;}
};
}
void VerifyBootstrapVectorGroup(NativeInitRuntime& runtime,CVectorObject& object) {
    assert(runtime.CpuReady() && !runtime.GpuReady() && !g_MatrixMap && object.IsLoaded());
    struct Restore {~Restore(){active=nullptr;}} restore;
    for(int repeat=0;repeat<2;++repeat) {
        CVectorObjectGroup group;D3DXMATRIX world;D3DXMatrixIdentity(&world);world._41=23;world._42=-7;world._43=4;
        group.m_GroupToWorldMatrix=&world;
        auto* unit=group.Add();unit->m_Id=101;unit->m_Name=L"actual VO unit";
        unit->m_Obj->Init(&object,nullptr,nullptr);unit->m_Obj->SetAnimDefault();
        assert(group.GetById(101)==unit && group.GetByName(L"actual VO unit")==unit && group.m_Last==unit);
        assert(!group.GetByIdNE(202) && !group.GetByNameNE(L"missing"));
        group.RNeed(VOUF_MATRIX);assert(unit->m_MatrixWorld._41==23 && unit->m_MatrixWorld._42==-7 && unit->m_MatrixWorld._43==4);
        D3DXVECTOR3 lo,hi,shift_lo,shift_hi;group.BoundGetAllFrame(lo,hi);
        world._41+=100;group.RChange(VOUF_MATRIX);group.BoundGetAllFrame(shift_lo,shift_hi);
        assert(std::abs(shift_lo.x-lo.x-100)<0.001f && std::abs(shift_hi.x-hi.x-100)<0.001f);
        assert(shift_lo.y==lo.y && shift_hi.y==hi.y && shift_lo.z==lo.z && shift_hi.z==hi.z);
        group.Takt(1);
        Observation observation;observation.expected=reinterpret_cast<uintptr_t>(&observation);active=&observation;
        if(sizeof(uintptr_t)==8)assert(observation.expected>0xffffffffULL);
        group.EnumFrameVerts_transform_position(Vertex,observation.expected);
        SSkin skin{};skin.m_SetupStages=Stages;skin.m_SetupTex=Texture;skin.m_SetupClear=Clear;
        unit->m_Obj->Init(&object,nullptr,&skin);unit->m_Obj->SetAnimDefault();
        Recorder recorder;{NativeDrawScope scope(recorder);object.BeforeDraw();CVectorObject::DrawBegin();unit->m_Obj->Draw(observation.expected);CVectorObject::DrawEnd();}
        std::printf("original vector group expected=%llx vertices=%llx anim_draw=%llx vertex_calls=%u skin_calls=%u ranges=%u\n",static_cast<unsigned long long>(observation.expected),static_cast<unsigned long long>(observation.vertices),static_cast<unsigned long long>(observation.draw),observation.vertex_calls,observation.draw_calls,recorder.ranges);
        assert(observation.vertex_calls && observation.draw_calls && recorder.ranges);
        if(observation.vertices!=observation.expected || observation.draw!=observation.expected)throw std::runtime_error("original vector group/anim draw payload truncated");
        group.Delete(unit);assert(!group.m_First && !group.m_Last);group.Clear();
    }
    std::puts("original full vector group Add/lookup/real VO animation/matrix/bounds translation/Delete/Clear and high-address vertex/skin payloads EXECUTED; Load/config, linked units, group Draw and GPU NOT EXECUTED");
}
