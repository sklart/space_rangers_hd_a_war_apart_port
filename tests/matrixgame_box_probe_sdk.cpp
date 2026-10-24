// SDK oracle lives in a separate TU: SDK and portable D3DX types must not mix.
#define NOMINMAX
#include <d3dx9math.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
extern "C" bool MatrixGamePortableBoxProbe(const float*);

void VerifyMatrixGameBoxProbeSdk() {
    std::uint32_t state=0x519b0a;
    auto value=[&](){state=state*1664525u+1013904223u;return float(int(state%20001)-10000)/128.f;};
    std::size_t count=0;
    auto check=[&](const std::array<float,12>& a){
        D3DXVECTOR3 lo(a[0],a[1],a[2]),hi(a[3],a[4],a[5]);
        D3DXVECTOR3 o(a[6],a[7],a[8]),d(a[9],a[10],a[11]);
        const bool sdk=D3DXBoxBoundProbe(&lo,&hi,&o,&d)!=FALSE;
        if(sdk!=MatrixGamePortableBoxProbe(a.data())){
            std::fprintf(stderr,"box SDK mismatch case=%zu SDK=%d inputs:",count,int(sdk));
            for(float x:a)std::fprintf(stderr," %.9g",x);std::fprintf(stderr,"\n");
            throw std::runtime_error("portable BoxBoundProbe differs from SDK");
        }
        ++count;
    };
    // Inside, zero/parallel directions, boundary origins and both ray signs.
    for(float x:{-2.f,-1.f,0.f,1.f,2.f})for(float y:{-2.f,-1.f,0.f,1.f,2.f})
    for(float z:{-2.f,-1.f,0.f,1.f,2.f})for(float dx:{-2.f,-0.f,0.f,2.f})
    for(float dy:{-2.f,0.f,2.f})for(float dz:{-2.f,0.f,2.f})
        check({-1,-1,-1,1,1,1,x,y,z,dx,dy,dz});
    for(int i=0;i<100000;++i){
        std::array<float,12>a;for(auto&x:a)x=value();
        for(int k=0;k<3;++k)if(a[k]>a[k+3])std::swap(a[k],a[k+3]);
        if(i%7==0)a[9+i%3]=0;
        check(a);
    }
    std::printf("portable BoxBoundProbe versus actual D3DX SDK: %zu finite ray cases PASS\n",count);
}
