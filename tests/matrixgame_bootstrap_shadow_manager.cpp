#ifdef NDEBUG
#error Shadow manager verification requires assertions
#endif
#include "init_runtime.hpp"
#include "MatrixGame.h"
#include "ShadowProj.hpp"
#include "MatrixShadowManager.hpp"
#include "ShadowStencil.hpp"
#include "VectorObject.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
using namespace srhd_awa::matrixgame;
namespace {
using V=std::array<double,3>;
double Dot(const V& a,const V& b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
V Cross(const V& a,const V& b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
V Unit(V a){const auto length=std::sqrt(Dot(a,a));for(auto& x:a)x/=length;return a;}
void Near(const D3DXVECTOR3& actual,const V& expected){
    const V values{actual.x,actual.y,actual.z};
    for(int i=0;i<3;++i)assert(std::fabs(values[i]-expected[i])<=5e-4+2e-5*std::fabs(expected[i]));
}
// Scalar camera axes and corner projection, independent of DirectXMath,
// inverse matrices and the production GetBound implementation.
void VerifyBasis(const SProjData& actual,const SVOGeometry& geometry,
                 const int* frames,const D3DXMATRIX* worlds,int count,const D3DXVECTOR3& light){
    const V forward=Unit({light.x,light.y,light.z}),right=Unit(Cross({0,0,1},forward));
    const V up=Cross(forward,right),eye{-light.x,-light.y,-light.z};
    const std::array<V,3> axes{right,up,forward};
    V lo{1e300,1e300,1e300},hi{-1e300,-1e300,-1e300};
    for(int item=0;item<count;++item){
        const auto& frame=geometry.m_Frames[frames[item]];
        for(int corner=0;corner<8;++corner){
            V p{corner&1?frame.m_Max.x:frame.m_Min.x,corner&2?frame.m_Max.y:frame.m_Min.y,corner&4?frame.m_Max.z:frame.m_Min.z};
            if(worlds){
                const auto& m=worlds[item];
                p={p[0]*m._11+p[1]*m._21+p[2]*m._31+m._41,
                   p[0]*m._12+p[1]*m._22+p[2]*m._32+m._42,
                   p[0]*m._13+p[1]*m._23+p[2]*m._33+m._43};
            }
            for(int i=0;i<3;++i){
                const V relative{p[0]-eye[0],p[1]-eye[1],p[2]-eye[2]};const auto projected=Dot(relative,axes[i]);
                lo[i]=std::min(lo[i],projected-6.0);hi[i]=std::max(hi[i],projected+6.0);
            }
        }
    }
    V origin=eye,vx{},vy{},vz{};
    for(int i=0;i<3;++i){
        origin[i]+=right[i]*lo[0]+up[i]*lo[1]+forward[i]*lo[2];
        vx[i]=right[i]*(hi[0]-lo[0]);vy[i]=up[i]*(hi[1]-lo[1]);vz[i]=forward[i]*(hi[2]-lo[2]);
    }
    Near(actual.vpos,origin);Near(actual.vx,vx);Near(actual.vy,vy);Near(actual.vz,vz);
}
}
void VerifyBootstrapShadowManager(NativeInitRuntime& runtime,CVectorObject& object) {
    assert(runtime.CpuReady() && !runtime.GpuReady() && g_MatrixHeap && !g_MatrixMap);
    const auto bytes=buffers::Current().AllocatedBytes();
    const auto& geometry=CVOShadowStencil::ReadSourceGeometry(object);
    CVectorObjectAnim first,second;first.Init(&object,nullptr);second.Init(&object,nullptr);
    CVectorObjectAnim* objects[]{&first,&second};const int frames[]{0,object.GetFramesCnt()-1};
    std::array<D3DXMATRIX,2> worlds{};D3DXMatrixIdentity(&worlds[0]);D3DXMatrixRotationZ(&worlds[1],0.6f);
    worlds[0]._11=1.25f;worlds[0]._22=0.5f;worlds[0]._33=2;worlds[0]._41=13;worlds[0]._42=-9;
    worlds[1]._31=0.15f;worlds[1]._41=-20;worlds[1]._43=7;
    for(const D3DXVECTOR3 light:std::array<D3DXVECTOR3,2>{{{1,2,-3},{-3,0.5f,2}}}){
        auto direction=light;SProjData single{},combined{};
        first.CalcShadowProjMatrix(frames[0],single,direction,6);
        VerifyBasis(single,geometry,frames,nullptr,1,light);
        CVectorObject::CalcShadowProjMatrix(2,objects,frames,worlds.data(),combined,direction,6);
        VerifyBasis(combined,geometry,frames,worlds.data(),2,light);
        assert(direction.x==light.x && direction.y==light.y && direction.z==light.z);
    }
    for(int repeat=0;repeat<2;++repeat) {
        CVOShadowProj projection(g_MatrixHeap);
        CMatrixShadowCliper clipper(&projection);
        D3DXMatrixIdentity(clipper.WorldMatrix());
        bool layout_refused=false,transform_refused=false;
        try{clipper.BeforeRender();}catch(const platform3g::Unsupported&){layout_refused=true;}
        try{clipper.Render();}catch(const platform3g::Unsupported&){transform_refused=true;}
        assert(layout_refused && transform_refused && !projection.IsProjected());
    }
    assert(!g_MatrixMap && buffers::Current().AllocatedBytes()==bytes);
    std::puts("original full shadow manager clipper and real VO animator/single/multiple shadow basis scalar oracle EXECUTED; genuine device refused layout/transform; terrain projection and GPU NOT EXECUTED");
}
