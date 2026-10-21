// Execute actual pinned CVectorObject methods, with no GPU/texture substitute.
#include "VectorObjectCpu.hpp"
#include "vector_geometry.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>

Base::CHeap* g_CacheHeap{}; // Explicit real heap context; full CacheInit is not called.
using srhd_awa::matrixgame::VectorGeometry;
struct Enumeration {const VectorGeometry* geometry;std::span<const std::uint32_t> ids;std::size_t count{};};
static bool vertex(const SVOVertex& v,uintptr_t opaque) {
    auto& e=*reinterpret_cast<Enumeration*>(opaque);
    const auto& expected=e.geometry->Vertices()[e.ids[e.count++]];
    static_assert(sizeof(expected)==sizeof(v));
    assert(std::memcmp(&expected,&v,sizeof(v))==0);
    return false;
}
static void check(CVectorObject& object,const VectorGeometry& geometry) {
    assert(object.IsLoaded() && object.GetFramesCnt()==int(geometry.Frames().size()));
    assert(object.GetAnimCount()==int(geometry.Animations().size()));
    assert(object.GetMatrixCount()==int(geometry.MatrixNames().size()));
    for(int i=0;i<object.GetAnimCount();++i) {
        const auto& a=geometry.Animations()[i];
        assert(object.GetAnimById(a.id)==i);
        assert(object.GetAnimByName(object.GetAnimName(i))==i);
        assert(object.GetAnimFramesCount(i)==int(a.count));
        for(unsigned j=0;j<a.count;++j) {
            const auto& f=geometry.AnimationFrames()[a.first+j];
            assert(object.GetAnimFrameIndex(i,j)==f.frame);
            assert(object.GetAnimFrameTime(i,j)==std::abs(f.time));
        }
    }
    for(int i=0;i<object.GetFramesCnt();++i) {
        const auto& f=geometry.Frames()[i];
        assert(std::memcmp(object.GetFrame(i),&f,sizeof(f))==0);
        Enumeration e{&geometry,geometry.FrameVertices().subspan(f.m_VerStart,f.m_VerCnt)};
        object.EnumFrameVerts(i,vertex,reinterpret_cast<uintptr_t>(&e));
        assert(e.count==std::size_t(f.m_VerCnt));
        D3DXMATRIX transform{};
        transform._11=2;transform._22=3;transform._33=4;transform._44=1;
        transform._41=10;transform._42=-20;transform._43=30;
        D3DXVECTOR3 lo,hi;object.GetBound(i,transform,lo,hi);
        for(unsigned k=0;k<3;++k) {
            const float scale=float(k+2),offset=k==0?10.f:k==1?-20.f:30.f;
            assert(std::abs(lo[k]-(f.m_Min[k]*scale+offset))<0.002f);
            assert(std::abs(hi[k]-(f.m_Max[k]*scale+offset))<0.002f);
        }
        for(int m=0;m<object.GetMatrixCount();++m) {
            const auto names=geometry.MatrixNames();
            auto id_match=std::find_if(names.begin(),names.end(),[&](const auto& n){return n.id==names[m].id;});
            auto name_match=std::find_if(names.begin(),names.end(),[&](const auto& n){return n.name==names[m].name;});
            const auto& expected=geometry.Matrices()[id_match->first+i];
            assert(std::memcmp(object.GetMatrixById(i,object.GetMatrixId(m)),&expected,sizeof(expected))==0);
            const auto& named=geometry.Matrices()[name_match->first+i];
            assert(std::memcmp(object.GetMatrixByName(i,object.GetMatrixName(m)),&named,sizeof(named))==0);
        }
    }
}
int main(int argc,char** argv) {
    std::setvbuf(stdout,nullptr,_IOLBF,0);
    if(argc<3) {std::fprintf(stderr,"usage: probe MODEL... SHORT_RESOURCE\n");return 2;}
    try {
        Base::CHeap heap;g_CacheHeap=&heap;
        srhd_awa::matrixgame::buffers::Context buffers;
        srhd_awa::matrixgame::buffers::Scope scope(buffers);
        {
            CVectorObject failed;
            failed.m_Name=std::filesystem::path(argv[argc-1]).wstring();
            bool rejected=false;try {failed.Load();}catch(...){rejected=true;}
            assert(rejected && !failed.IsLoaded());
        }
        for(int cycle=0;cycle<2;++cycle) {
            CVectorObject::StaticInit();
            std::puts("CALL CVectorObject::StaticInit (original inline CPU reset)");
            {
                CVectorObject object;
                for(int arg=1;arg<argc-1;++arg) {
                    object.m_Name=std::filesystem::path(argv[arg]).wstring();
                    const auto checked=VectorGeometry::Load(object.m_Name.c_str());
                    object.Load(); // Original virtual Load -> LoadSpecial(OLF_NO_TEX).
                    check(object,checked);
                    std::printf("CALL Load/LoadSpecial/EnumFrameVerts/GetBound frames=%d animations=%d matrices=%d\n",
                        object.GetFramesCnt(),object.GetAnimCount(),object.GetMatrixCount());
                    object.Unload();assert(!object.IsLoaded());
                    std::puts("CALL CVectorObject::Unload");
                }
                for(const auto& path:{std::string("missing-vector-object.vo"),std::string(argv[argc-1])}) {
                    object.m_Name=std::filesystem::path(path).wstring();bool rejected=false;
                    try {object.Load();} catch(...) {rejected=true;}
                    assert(rejected && !object.IsLoaded());
                    std::puts("PASS missing/short resource rejected before geometry allocation");
                }
                bool unsupported=false;
                try {object.LoadSpecial(OLF_AUTO,nullptr,0);}catch(const std::runtime_error&){unsupported=true;}
                assert(unsupported);
                // A failed load remains usable; the constructor's shared buffers
                // must survive unload of the previous last geometry source.
                object.m_Name=std::filesystem::path(argv[1]).wstring();object.Load();
                assert(object.IsLoaded());
            }
            assert(buffers.AllocatedBytes()==0);
            std::puts("CALL ~CVectorObject; PASS cycle cleanup");
        }
        g_CacheHeap=nullptr;
        std::puts("HOST_RUNTIME_PASS original CVectorObject CPU slice; CGame::Init/Run NOT CALLED; GPU UNSUPPORTED");
    } catch(const Base::CException& e) {std::fprintf(stderr,"MatrixLib exception\n");return 1;}
      catch(const std::exception& e) {std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
}
