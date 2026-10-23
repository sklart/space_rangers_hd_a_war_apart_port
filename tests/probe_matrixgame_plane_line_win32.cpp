// Addressed compatibility oracle, not a game/device prototype.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "math_types.hpp"
#include <cstdio>

int main() {
    wchar_t path[MAX_PATH]{};
    if(!GetSystemDirectoryW(path,MAX_PATH)) return 2;
    const auto dll_path=std::wstring(path)+L"\\d3dx9_43.dll";
    const auto dll=LoadLibraryW(dll_path.c_str());
    if(!dll) return 3;
    using Intersect=D3DXVECTOR3* (WINAPI*)(D3DXVECTOR3*,const D3DXPLANE*,const D3DXVECTOR3*,const D3DXVECTOR3*);
    const auto original=reinterpret_cast<Intersect>(GetProcAddress(dll,"D3DXPlaneIntersectLine"));
    if(!original) { FreeLibrary(dll);return 4; }
    unsigned cases=0;float maximum_error=0;
    auto verify=[&](const D3DXPLANE& plane,const D3DXVECTOR3& start,const D3DXVECTOR3& end) {
        for(unsigned alias=0;alias!=3;++alias) {
            D3DXVECTOR3 old_start=start,new_start=start,old_end=end,new_end=end;
            D3DXVECTOR3 old_out{111,222,333},new_out=old_out;
            auto* expected=alias==1?&old_start:alias==2?&old_end:&old_out;
            auto* actual=alias==1?&new_start:alias==2?&new_end:&new_out;
            const auto original_result=original(expected,&plane,&old_start,&old_end);
            const auto native_result=D3DXPlaneIntersectLine(actual,&plane,&new_start,&new_end);
            if(bool(original_result)!=bool(native_result)) return false;
            if((original_result && original_result!=expected) || (native_result && native_result!=actual)) return false;
            for(unsigned i=0;i!=3;++i) {
                const float ref=(*expected)[i],value=(*actual)[i];
                if(!std::isfinite(ref) || !std::isfinite(value)) return false;
                const float error=std::abs(value-ref)/std::max(1.0f,std::abs(ref));
                maximum_error=std::max(maximum_error,error);
                if(error>0.00002f || (!original_result && value!=ref)) return false;
            }
            ++cases;
        }
        return true;
    };
    for(float slope:{0.0f,1.0f,-1.0f,1e-10f,1e-8f,1e-7f,1e-6f}) {
        if(!verify({0,0,1,-1},{0,0,0},{1,0,slope})) return 5;
    }
    if(!verify({0,0,1,-1},{0,0,1},{1,0,1}) ||
       !verify({0,0,1,-1},{2,3,4},{2,3,0}) ||
       !verify({0,0,0,0},{1,2,3},{4,5,6})) return 6;
    unsigned state=0x196519;
    auto sample=[&] { state=1664525u*state+1013904223u;return float(int(state>>24)-128)/4.0f; };
    for(unsigned n=0;n!=128;++n) {
        const D3DXPLANE plane{sample(),sample(),sample(),sample()};
        const D3DXVECTOR3 start{sample(),sample(),sample()},end{sample(),sample(),sample()};
        if(!verify(plane,start,end)) { std::fprintf(stderr,"case %u failed\n",n);return 7; }
    }
    std::printf("{\"oracle\":\"Microsoft D3DX9 system DLL\",\"architecture_bits\":%u,\"cases\":%u,\"maximum_scaled_error\":%.9g,\"status\":\"PASS\"}\n",
                unsigned(8*sizeof(void*)),cases,double(maximum_error));
    FreeLibrary(dll);return 0;
}
