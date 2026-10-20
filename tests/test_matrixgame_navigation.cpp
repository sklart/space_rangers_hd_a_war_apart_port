// The actual pinned road-network algorithms, not a CGame/battle substitute.
#include "MatrixRoadNetwork.hpp"
#include "CStorage.hpp"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>
using namespace Base;
static std::uint32_t fingerprint(const CBuf& b) {
    std::uint32_t hash = 2166136261u;
    const auto* data = static_cast<const std::uint8_t*>(b.Get());
    for (size_t i=0;i<b.Len();++i) hash=(hash^data[i])*16777619u;
    return hash;
}
static void write_buffer(const std::filesystem::path& path,const CBuf& b) {
    std::ofstream stream(path,std::ios::binary);
    stream.write(static_cast<const char*>(b.Get()),static_cast<std::streamsize>(b.Len()));
    stream.close();assert(stream.good());
}
static void fixture(const char* output) {
    for (int cycle=0;cycle<10;++cycle) {
        CMatrixRoadNetwork graph(nullptr);
        graph.SetZoneCntMax(3); graph.m_ZoneCnt=3;
        for (int i=0;i<3;++i) {
            auto& z=graph.m_Zone[i]; z.m_Center=CPoint(i*10,0); z.m_Road=true; z.m_Size=1;
            z.m_Rect=CRect(i*10,0,i*10+1,1); z.m_Region=i;
            if(i>0) z.m_NearZone[z.m_NearZoneCnt++]=i-1;
            if(i<2) z.m_NearZone[z.m_NearZoneCnt++]=i+1;
            auto* crotch=graph.AddCrotch(); crotch->m_Center=z.m_Center; crotch->m_Zone=i;
            int region=graph.AddRegion(); assert(region==i);
            // The pinned POSIX ReAllocClear does not zero a grown allocation.
            // Initialize every synthetic region explicitly, as a resource Load does.
            auto* r=graph.GetRegion(region); *r=SMatrixRegion{}; r->m_Center=z.m_Center;
            if(i>0) r->m_Near[r->m_NearCnt++]=i-1;
            if(i<2) r->m_Near[r->m_NearCnt++]=i+1;
        }
        int path[16]{};
        assert(graph.CalcPathZoneByRoad(0,2,path)==3 && path[0]==0 && path[1]==1 && path[2]==2);
        assert(graph.CalcDistByRoad(0,2)==20);
        graph.FindPathInRegionInit();
        assert(graph.FindPathInRegionRun(1,0,2,path,16,false)==3);
        assert(path[0]==0 && path[1]==1 && path[2]==2);
        graph.GetRegion(2)->m_NearMove[0]=1;
        graph.FindPathInRegionInit();
        assert(graph.FindPathInRegionRun(1,0,2,path,16,false)==0);
        graph.GetRegion(2)->m_NearMove[0]=0;
        CBuf saved; graph.Save(saved);
        saved.Pointer(0); CMatrixRoadNetwork loaded(nullptr); loaded.Load(saved,27);
        assert(saved.Pointer()==saved.Len() && loaded.m_ZoneCnt==3 && loaded.m_CrotchCnt==3);
        assert(loaded.CalcDistByRoad(0,2)==20);
        CBuf again; loaded.Save(again);
        assert(saved.Len()==again.Len() && std::memcmp(saved.Get(),again.Get(),saved.Len())==0);
        if(cycle==0 && output) write_buffer(std::string(output)+".fixture.bin",saved);
        if(cycle==0) std::printf("fixture bytes=%zu hash=%08x path=0,1,2 distance=20\n",saved.Len(),fingerprint(saved));
    }
}
int main(int argc,char** argv) {
    assert(argc<=3);
    CMain::BaseInit();
    fixture(argc==3 ? argv[2] : nullptr);
    if(argc>=2) {
        std::filesystem::path file(argv[1]);
#ifdef _WIN32
        const auto wide=file.wstring();
#else
        const auto ascii=file.string(); const std::wstring wide(ascii.begin(),ascii.end());
#endif
        CStorage storage; storage.Load(wide.c_str());
        auto* roads=storage.GetBuf(L"roads",L"Data",ST_BYTE); assert(roads && roads->GetArrayLength(0)>4);
        CBuf b; b.Add(roads->GetFirst<std::uint8_t>(0),roads->GetArrayLength(0));b.Pointer(0);
        const auto version=b.Get<std::uint32_t>(); assert(version==27);
        CMatrixRoadNetwork graph(nullptr); graph.Load(b,version); assert(b.Pointer()==b.Len());
        graph.FindPathInRegionInit(); std::vector<int> path(graph.m_RegionCnt+1); int queries=0, reachable=0;
        for(int start=0;start<graph.m_RegionCnt && queries<32;++start)
            for(int j=0;j<graph.GetRegion(start)->m_NearCnt && queries<32;++j) {
                int target=graph.GetRegion(start)->m_Near[j];
                for(auto mask:{std::uint8_t(0),std::uint8_t(1),std::uint8_t(31)}) {
                    graph.FindPathInRegionInit();
                    int count=graph.FindPathInRegionRun(mask,start,target,path.data(),graph.m_RegionCnt+1,false);
                    if(count) { assert(path[0]==start && path[count-1]==target); ++reachable; }
                    ++queries;
                }
            }
        CBuf roundtrip; roundtrip.Add<std::uint32_t>(27);graph.Save(roundtrip);
        // Original Load changes movement masks on cannon places. Check the
        // normalized Save/Load fixed point separately from raw input equality.
        const bool raw_equal=roundtrip.Len()==b.Len() && std::memcmp(roundtrip.Get(),b.Get(),b.Len())==0;
        roundtrip.Pointer(4); CMatrixRoadNetwork reloaded(nullptr);reloaded.Load(roundtrip,27);
        assert(roundtrip.Pointer()==roundtrip.Len());
        CBuf stable;stable.Add<std::uint32_t>(27);reloaded.Save(stable);
        assert(stable.Len()==roundtrip.Len() && std::memcmp(stable.Get(),roundtrip.Get(),stable.Len())==0);
        if(argc==3) {
            write_buffer(std::string(argv[2])+".roads.raw.bin",b);
            write_buffer(std::string(argv[2])+".roads.normalized.bin",roundtrip);
        }
        int cannons=0;for(int i=0;i<graph.m_PlaceCnt;++i) cannons+=graph.GetPlace(i)->m_Cannon!=0;
        std::printf("map zones=%d roads=%d crotches=%d regions=%d bytes=%zu hash=%08x normalized_hash=%08x queries=%d reachable=%d cannons=%d raw_equal=%d normalized_stable=true\n",
                    graph.m_ZoneCnt,graph.m_RoadCnt,graph.m_CrotchCnt,graph.m_RegionCnt,b.Len(),fingerprint(b),fingerprint(roundtrip),queries,reachable,cannons,int(raw_equal));
    }
    std::puts("PASS navigation CPU algorithms and version-27 resource roundtrip; battle_executed=false");
    CMain::BaseDeInit();
}
