#ifdef NDEBUG
#error Shadow texture verification requires assertions
#endif
#include "init_runtime.hpp"
#include "MatrixGame.h"
#include "VectorObject.hpp"
#include "ShadowProj.hpp"
#include "Texture.hpp"
#include "unmanaged_texture_backend.hpp"
#include "shadow_texture_backend.hpp"
#include <cassert>
#include <cstdio>
#include <cmath>
using namespace srhd_awa::matrixgame;
void VerifyBootstrapShadowTexture(NativeInitRuntime& runtime,CVectorObject& object){
    assert(runtime.CpuReady() && !runtime.GpuReady() && !g_MatrixMap);
    const auto registry=legacy_texture::RegistrySize(),bytes=buffers::Current().AllocatedBytes();
    CVectorObjectAnim animation;animation.Init(&object,nullptr);CVectorObjectAnim* objects[]{&animation};
    D3DXMATRIX identity;D3DXMatrixIdentity(&identity);int hits=0;
    for(int frame:{0,object.GetFramesCnt()-1}){
        const auto* f=object.GetFrame(frame);
        for(int axis=0;axis<3;++axis)for(float sign:{-1.f,1.f}){
            auto origin=f->m_GeoCenter;D3DXVECTOR3 direction{};
            if(axis==0){origin.x+=sign*(f->m_Radius*3+1);direction.x=-sign;}
            if(axis==1){origin.y+=sign*(f->m_Radius*3+1);direction.y=-sign;}
            if(axis==2){origin.z+=sign*(f->m_Radius*3+1);direction.z=-sign;}
            float t=-123;const bool hit=object.PickFull(frame,identity,identity,origin,direction,&t);
            assert(hit?(std::isfinite(t)&&t>=0):t==-123);hits+=hit;
        }
    }
    assert(hits>0);
    const auto center=object.GetFrameGeoCenter(0);const D3DXVECTOR3 away{1,0,0};
    const D3DXVECTOR3 miss{center.x+object.GetFrame(0)->m_Radius*4+1,center.y,center.z};
    float untouched=-123;
    assert(!object.Pick(0,identity,identity,miss,away,&untouched) && untouched==-123);
    assert(!animation.PickFull(identity,identity,miss,away,&untouched) && untouched==-123);
    std::printf("original VO PickFull on first/last frames: 12 axis rays, %d hits; Pick/animator miss preserves output EXECUTED\n",hits);
    const int frames[]{0};D3DXVECTOR3 light{1,2,-3};SProjData projection{};
    animation.CalcShadowProjMatrix(0,projection,light,6);
    for(int repeat=0;repeat<2;++repeat){
        auto* existing=legacy_texture::CreateUnmanaged();
        for(int update=0;update<2;++update){
            bool matrix=false,viewport=false;
            try{auto* unexpected=CVectorObject::CalcShadowTexture(1,objects,frames,nullptr,projection,64,nullptr,update?existing:nullptr);if(unexpected)CCache::Destroy(unexpected);}
            catch(const platform3g::Unsupported&){matrix=true;}
            try{auto* unexpected=CVectorObject::CalcShadowTextureWOMat(1,objects,frames,nullptr,64,nullptr,update?existing:nullptr);if(unexpected)CCache::Destroy(unexpected);}
            catch(const platform3g::Unsupported&){viewport=true;}
            assert(matrix && viewport && !existing->IsLoaded() && !existing->m_Cache);
            assert(legacy_texture::RegistrySize()==registry+1);
        }
        CCache::Destroy(existing);
    }
    assert(legacy_texture::RegistrySize()==registry && buffers::Current().AllocatedBytes()==bytes);
    std::puts("original CalcShadowTexture/CalcShadowTextureWOMat new/update cold refusal and caller ownership/retry EXECUTED; no GPU device or fabricated draw");
}
