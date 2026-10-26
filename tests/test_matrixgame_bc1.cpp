// Genuine CPU encoder/backend tests. Image upload is explicitly a CPU model.
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include "native_texture.hpp"
#include "texture_asset.hpp"
#include "BC.h"
using namespace srhd_awa::matrixgame;
namespace nt=native_texture;
namespace matrixgame_fault {extern long countdown;}
std::vector<std::byte> Read(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary|std::ios::ate);assert(f);std::vector<std::byte> b(std::size_t(f.tellg()));f.seekg(0);f.read(reinterpret_cast<char*>(b.data()),b.size());assert(f);return b;}
void Write(const std::filesystem::path& p,const std::vector<std::byte>& b){std::ofstream f(p,std::ios::binary);f.write(reinterpret_cast<const char*>(b.data()),b.size());assert(f);}
std::vector<std::byte> DDS(const TextureAsset& a){
    std::vector<std::byte> b(128);auto put=[&](unsigned at,unsigned v){for(unsigned i=0;i<4;++i)b[at+i]=std::byte(v>>(i*8));};
    std::memcpy(b.data(),"DDS ",4);put(4,124);put(8,0x21007);put(12,a.levels[0].height);put(16,a.levels[0].width);put(28,a.levels.size());put(76,32);put(108,0x401008);
    if(a.format==TextureFormat::BC1){put(80,4);put(84,0x31545844);}
    else {put(80,0x41);put(88,32);put(92,255);put(96,65280);put(100,16711680);put(104,4278190080u);}
    for(const auto& l:a.levels)b.insert(b.end(),l.bytes.begin(),l.bytes.end());return b;
}
std::vector<std::byte> Pixels(const TextureLevel& l){
    std::vector<std::byte> b(std::size_t(l.width)*l.height*4);
    for(unsigned by=0;by<(l.height+3)/4;++by)for(unsigned bx=0;bx<(l.width+3)/4;++bx){alignas(16) DirectX::XMVECTOR colors[16];DirectX::D3DXDecodeBC1(colors,reinterpret_cast<const std::uint8_t*>(l.bytes.data()+std::size_t(by)*l.pitch+bx*8));
        for(unsigned y=0;y<4 && by*4+y<l.height;++y)for(unsigned x=0;x<4 && bx*4+x<l.width;++x){DirectX::XMFLOAT4 p;DirectX::XMStoreFloat4(&p,colors[y*4+x]);float c[]{p.x,p.y,p.z,p.w};for(unsigned k=0;k<4;++k)b[(std::size_t(by*4+y)*l.width+bx*4+x)*4+k]=std::byte(unsigned(c[k]*255.f+.5f));}}
    return b;
}
template<class F>void Reject(F f){bool bad=false;try{f();}catch(const std::exception&){bad=true;}assert(bad);}
struct CpuImage:nt::Image {nt::Residency& arena;nt::Residency::Range range;explicit CpuImage(nt::Residency& a,std::size_t n):arena(a),range(a.Allocate(n,256)){}~CpuImage(){arena.Release(range);}};
int main(int argc,char** argv){
    assert(argc==3);const std::filesystem::path input=argv[1],output=argv[2];std::filesystem::create_directories(output);
    auto session=std::make_shared<nt::Session>();nt::Residency arena(16*1024*1024);bool fail=false;unsigned uploads=0;
    session->upload=[&](const nt::Texture& t)->std::shared_ptr<nt::Image>{nt::ValidateUpload(t);if(fail)throw std::bad_alloc();++uploads;std::size_t size=0;for(const auto& l:t.levels)size+=l.bytes.size();return std::make_shared<CpuImage>(arena,size);};
    unsigned cases=0;double encode_total=0,mips_total=0;
    for(unsigned case_id=0;case_id<10;++case_id){
        std::vector<std::byte> bytes;const auto name=case_id<6?"union-"+std::to_string(case_id):"synthetic-"+std::to_string(case_id-6);
        if(case_id<6)bytes=Read(input/(name+".dds"));
        else {const unsigned w=case_id==9?4:64,h=case_id==8?32:w;TextureAsset a;TextureLevel l{w,h,w*4,std::vector<std::byte>(w*h*4)};
            for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){auto* p=l.bytes.data()+(y*w+x)*4;p[0]=std::byte(x*255/(w-1));p[1]=std::byte(y*255/(h-1));p[2]=std::byte((x*17+y*13)&255);p[3]=std::byte(case_id==9?0:case_id==7?((x+y)%3?255:0):case_id==8?(x%4==0?0:x%4==1?127:x%4==2?128:255):255);}a.levels.push_back(std::move(l));bytes=DDS(a);Write(output/(name+".dds"),bytes);}
        auto a=TextureAsset::Decode(bytes);a.PrepareMips(TextureMipPolicy::OneLevel);auto t0=std::chrono::steady_clock::now();a.CompressBC1();auto t1=std::chrono::steady_clock::now();const unsigned count=case_id==9?3:6;a.PrepareMips(TextureMipPolicy::CompleteBox,64*1024*1024,count);auto t2=std::chrono::steady_clock::now();
        const auto encode=std::chrono::duration<double,std::milli>(t1-t0).count(),mips=std::chrono::duration<double,std::milli>(t2-t1).count();encode_total+=encode;mips_total+=mips;
        assert(a.format==TextureFormat::BC1 && a.levels.size()==count);std::size_t total=0;
        for(unsigned i=0;i<count;++i){const auto& l=a.levels[i];assert(l.pitch==((l.width+3)/4)*8 && l.bytes.size()==std::size_t(l.pitch)*((l.height+3)/4));total+=l.bytes.size();auto pixels=Pixels(l);
            if(i==0)for(unsigned y=0;y<l.height;++y)for(unsigned x=0;x<l.width;++x){const unsigned alpha=case_id==9?0:case_id==7?((x+y)%3?255:0):case_id==8?((x%4)>=2?255:0):255;assert(unsigned(pixels[(std::size_t(y)*l.width+x)*4+3])==alpha);}
            Write(output/(name+"-"+std::to_string(i)+".rgba"),pixels);}
        if(case_id<6)assert(total==698880 && a.levels[0].width==1024 && a.levels[5].width==32);
        auto compressed=DDS(a);Write(output/(name+"-native.dds"),compressed);auto roundtrip=TextureAsset::Decode(compressed);assert(roundtrip.levels.size()==count);for(unsigned i=0;i<count;++i)assert(roundtrip.levels[i].bytes==a.levels[i].bytes);
        IDirect3DTexture9* token{};fail=true;Reject([&]{nt::Decode(session,bytes.data(),bytes.size(),count,0x31545844,1,&token);});assert(!token && nt::HandleCount()==0 && arena.Free()==arena.Capacity());fail=false;
        nt::Decode(session,bytes.data(),bytes.size(),count,0x31545844,1,&token);auto texture=nt::Find(token);assert(texture->levels.size()==count);
        for(unsigned i=0;i<count;++i)assert(texture->levels[i].bytes==a.levels[i].bytes);
        auto packet=nt::Resident(texture);const auto calls=uploads;assert(nt::Resident(texture)==packet && uploads==calls);
        session->Invalidate();assert(!texture->image && arena.Free()==arena.Capacity()-total);
        fail=true;Reject([&]{nt::Resident(texture);});assert(!texture->image);fail=false;
        auto recreated=nt::Resident(texture);assert(recreated!=packet && arena.Free()==arena.Capacity()-total*2);recreated.reset();
        nt::Release(token);token=nullptr;texture.reset();assert(arena.Free()==arena.Capacity()-total);packet.reset();assert(arena.Free()==arena.Capacity());
        std::printf("case=%s encode_ms=%.3f box_mips_ms=%.3f levels=%u BC1_bytes=%zu OOM_cleanup=PASS packet_retention=PASS\n",name.c_str(),encode,mips,count,total);++cases;
    }
    // Edge blocks, bad dimensions/rows, peak budget and strong exception safety.
    TextureAsset edge;edge.levels.push_back({7,5,28,std::vector<std::byte>(140,std::byte{255})});edge.CompressBC1();assert(edge.levels[0].pitch==16 && edge.levels[0].bytes.size()==32);
    TextureAsset bad;bad.levels.push_back({4,4,16,std::vector<std::byte>(63)});Reject([&]{bad.CompressBC1();});assert(bad.format==TextureFormat::RGBA8 && bad.levels[0].bytes.size()==63);
    bad.levels[0].bytes.resize(64);Reject([&]{bad.CompressBC1(64);});assert(bad.format==TextureFormat::RGBA8);
    Reject([&]{bad.PrepareMips(TextureMipPolicy::CompleteBox,64*1024*1024,4);});
    nt::Texture invalid;invalid.format=0x31545844;invalid.levels.push_back({4,4,8,1,std::vector<std::byte>(7)});Reject([&]{nt::ValidateUpload(invalid);});
    invalid.levels[0].bytes.resize(8);nt::ValidateUpload(invalid);invalid.levels[0].pitch=9;Reject([&]{nt::ValidateUpload(invalid);});
    TextureAsset small;small.levels.push_back({8,8,32,std::vector<std::byte>(256,std::byte{255})});auto source=DDS(small);unsigned faults=0;
    for(long position=0;position<128;++position){IDirect3DTexture9* token{};matrixgame_fault::countdown=position;
        try{nt::Decode(session,source.data(),source.size(),4,0x31545844,1,&token);matrixgame_fault::countdown=-1;nt::Release(token);break;}
        catch(const std::bad_alloc&){matrixgame_fault::countdown=-1;assert(!token && nt::HandleCount()==0 && arena.Free()==arena.Capacity());++faults;}}
    assert(faults>15 && faults<128);std::printf("ordinary_new_OOM_positions=%u cleanup=PASS\n",faults);
    for(auto length:{std::size_t(4),std::size_t(127),std::size_t(140)}){IDirect3DTexture9* token{};Reject([&]{nt::Decode(session,source.data(),length,4,0x31545844,1,&token);});assert(!token && nt::HandleCount()==0);}
    for(unsigned width:{0u,8193u,0xffffffffu}){auto corrupted=source;for(unsigned i=0;i<4;++i)corrupted[16+i]=std::byte(width>>(8*i));IDirect3DTexture9* token{};Reject([&]{nt::Decode(session,corrupted.data(),corrupted.size(),4,0x31545844,1,&token);});assert(!token && nt::HandleCount()==0);}
    session->Shutdown();assert(nt::HandleCount()==0 && arena.Free()==arena.Capacity());
    std::printf("BC1 CPU PASS cases=%u encode_total_ms=%.3f mips_total_ms=%.3f assertions=ON GPU_UPLOAD_MODEL_ONLY SWITCH_HARDWARE_NOT_RUN\n",cases,encode_total,mips_total);
}
