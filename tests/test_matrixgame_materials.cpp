// SPDX-License-Identifier: GPL-2.0-or-later
// Original callbacks with controlled texture inputs. This is not a game device.
#ifdef NDEBUG
#error Material checks require assertions
#endif
#include "material_combiner.hpp"
#include "fixed_vertex.hpp"
#include "platform_3g.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <map>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#include <d3d9.h>
#endif
namespace mg=srhd_awa::matrixgame;
namespace mat=mg::materials;
namespace rp3g=mg::platform3g;
namespace original_renderer_ops {} // Original ordinary helpers declared below.
using mg::RasterRegister;using mg::SamplerRegister;
using CTextureManaged=struct FixtureTexture;
struct Matrix {float _11{1},_12{},_13{},_14{},_21{},_22{1},_23{},_24{},_31{},_32{},_33{1},_34{},_41{},_42{},_43{},_44{1};};
struct FixtureTexture {void* texture{};unsigned flags{};void* Tex(){return texture;}unsigned Flags(){return flags;}};
enum GSParam {GSP_SIDE,GSP_ORDINAL,GSP_SIDE_NOALPHA};
struct SSkin {
    void (*m_SetupTex)(const SSkin*,uintptr_t,int){};
    bool (*m_SetupStages)(const SSkin*,uintptr_t,int){};
    void (*m_SetupClear)(const SSkin*,uintptr_t){};
    void (*m_SetupTexShadow)(const SSkin*){};
};
struct SMatrixSkin;
struct CMatrixMapObject {unsigned m_BurnSkinVis{};SMatrixSkin* m_BurnSkin{};unsigned GetTerrainColor(){return 0x7192a3;}};
struct FixtureWater {mg::buffers::VertexBuffer* m_VB{};mg::buffers::IndexBuffer* m_IB{};};
struct FixtureMap {Matrix identity;FixtureTexture* reflection{},*m_Macrotexture{};unsigned m_WaterColor{};FixtureWater* m_Water{};const Matrix& GetIdentityMatrix(){return identity;}FixtureTexture* GetReflectionTexture(){return reflection;}} fixture_map;
struct CBottomTextureUnion {FixtureTexture* texture{};FixtureTexture* GetTexture(){return texture;}static CBottomTextureUnion& Get(int){static CBottomTextureUnion value;return value;}};
struct SMatrixMapWaterVertex {float data[10];};
#define MATRIX_WATER_VERTEX_FORMAT 0x212u
#define GET_VB(x) (x)
#define GET_IB(x) (x)
#define DTRACE() ((void)0)
static auto* g_MatrixMap=&fixture_map;
static unsigned fixture_caps;
static constexpr unsigned TF_ALPHATEST=1;
struct TraceDevice final:rp3g::Device {
    mat::Combiner combiner;
    std::array<void*,8> textures{};
    std::array<Matrix,8> transforms{};
    std::map<std::pair<unsigned,unsigned>,unsigned> coordinates;
    rp3g::DeviceCaps ReadCaps()override{throw std::logic_error("test trace never negotiates caps");}
    rp3g::GammaRamp ReadGamma(unsigned)override{throw std::logic_error("test trace has no gamma");}
    void RestoreGamma(unsigned,const rp3g::GammaRamp&)noexcept override{std::terminate();}
    std::uint64_t AvailableTextureBytes()override{throw std::logic_error("test trace has no GPU budget");}
    int SetTextureStageState(unsigned s,rp3g::TextureStageRegister reg,unsigned v)override{
        auto r=unsigned(reg);if(r==11 || r==24)coordinates[{s,r}]=v;else combiner.Set(s,r,v);return 0;
    }
    int SetRenderState(RasterRegister r,unsigned v)override{if(unsigned(r)==60)combiner.Factor(v);return 0;}
    int SetSamplerState(unsigned,SamplerRegister,unsigned)override{return 0;}
    int BindBorrowedTexture(unsigned stage,void* t)override{textures.at(stage)=t;combiner.Bind(stage,t!=nullptr);return 0;}
    int SetFixedVertexLayout(unsigned)override{return 0;}
    int BindStagingVertex(unsigned,mg::buffers::VertexBuffer*,unsigned,unsigned)override{return 0;}
    int BindStagingIndex(mg::buffers::IndexBuffer*)override{return 0;}
};
static TraceDevice trace;
static int TextureTransform(unsigned stage,const Matrix* m){trace.transforms.at(stage)=*m;return 0;}
#define ASSERT_DX(x) rp3g::CheckDeviceResult((x),#x)
#define ERROR_S(x) throw std::runtime_error("original rejected fixture caps")
#include "original_skin_materials.inc"
#include "original_terrain_materials.inc"

static mat::Color Unpack(unsigned c){return {float((c>>16)&255)/255,float((c>>8)&255)/255,float(c&255)/255,float(c>>24)/255};}
static unsigned diffuse_argb=0xcc336699,factor_argb=0x708090a0;
static std::map<void*,mat::Color> sample_colors;
#ifdef _WIN32
template<class T> struct Owned {T* p{};~Owned(){if(p)p->Release();}T** Put(){assert(!p);return &p;}};
static void HR(HRESULT h){if(FAILED(h))throw std::runtime_error("D3D9 reference HRESULT "+std::to_string(unsigned(h)));}
struct Reference {
    HWND window{};Owned<IDirect3D9> api;Owned<IDirect3DDevice9> device;
    Owned<IDirect3DSurface9> target,readback;
    std::vector<IDirect3DTexture9*> textures;
    Reference(){
        window=CreateWindowW(L"STATIC",L"MatrixGame material reference",WS_OVERLAPPEDWINDOW,0,0,32,32,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        if(!window)throw std::runtime_error("reference window creation failed");
        api.p=Direct3DCreate9(D3D_SDK_VERSION);if(!api.p)throw std::runtime_error("D3D9 unavailable");
        D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window;pp.BackBufferWidth=16;pp.BackBufferHeight=16;pp.BackBufferFormat=D3DFMT_X8R8G8B8;
        HR(api.p->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,device.Put()));
        HR(device.p->CreateRenderTarget(16,16,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,target.Put(),nullptr));
        HR(device.p->CreateOffscreenPlainSurface(16,16,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,readback.Put(),nullptr));
        HR(device.p->SetRenderTarget(0,target.p));
        D3DADAPTER_IDENTIFIER9 adapter{};HR(api.p->GetAdapterIdentifier(0,0,&adapter));
        std::printf("reference adapter=%s driver=%s version=%08x:%08x\n",adapter.Description,adapter.Driver,unsigned(adapter.DriverVersion.HighPart),unsigned(adapter.DriverVersion.LowPart));
    }
    ~Reference(){for(auto* t:textures)t->Release();if(window)DestroyWindow(window);}
    void* Texture(unsigned argb){
        IDirect3DTexture9* t{};HR(device.p->CreateTexture(1,1,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&t,nullptr));
        textures.push_back(t);D3DLOCKED_RECT lock;HR(t->LockRect(0,&lock,nullptr,0));std::memcpy(lock.pBits,&argb,4);HR(t->UnlockRect(0));return t;
    }
    void Update(void* texture,unsigned argb){auto* t=static_cast<IDirect3DTexture9*>(texture);D3DLOCKED_RECT lock;HR(t->LockRect(0,&lock,nullptr,0));std::memcpy(lock.pBits,&argb,4);HR(t->UnlockRect(0));}
    unsigned Draw(const mat::Uniforms& u){
        auto* d=device.p;
        HR(d->SetRenderState(D3DRS_LIGHTING,FALSE));HR(d->SetRenderState(D3DRS_ZENABLE,FALSE));HR(d->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE));
        HR(d->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE));HR(d->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE));
        HR(d->SetRenderState(D3DRS_TEXTUREFACTOR,factor_argb));
        for(unsigned i=0;i<8;++i){const auto& s=u.stages[i];
            HR(d->SetTexture(i,static_cast<IDirect3DBaseTexture9*>(trace.textures[i])));
            for(unsigned r=1;r<=6;++r)HR(d->SetTextureStageState(i,D3DTEXTURESTAGESTATETYPE(r),r<=3?s.color[r-1]:s.alpha[r-4]));
            HR(d->SetTextureStageState(i,D3DTSS_RESULTARG,s.color[3]));
            HR(d->SetTextureStageState(i,D3DTSS_TEXCOORDINDEX,0));HR(d->SetTextureStageState(i,D3DTSS_TEXTURETRANSFORMFLAGS,0));
            HR(d->SetSamplerState(i,D3DSAMP_MINFILTER,D3DTEXF_POINT));HR(d->SetSamplerState(i,D3DSAMP_MAGFILTER,D3DTEXF_POINT));
        }
        struct V {float x,y,z,rhw;unsigned color;float u,v;};
        const V vertices[]{{-.5f,-.5f,0,1,diffuse_argb,.5f,.5f},{15.5f,-.5f,0,1,diffuse_argb,.5f,.5f},{-.5f,15.5f,0,1,diffuse_argb,.5f,.5f},{15.5f,15.5f,0,1,diffuse_argb,.5f,.5f}};
        HR(d->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1));
        HR(d->Clear(0,nullptr,D3DCLEAR_TARGET,0x01fefdfe,1,0));HR(d->BeginScene());HR(d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,vertices,sizeof(V)));HR(d->EndScene());
        HR(d->GetRenderTargetData(target.p,readback.p));D3DLOCKED_RECT lock;HR(readback.p->LockRect(&lock,nullptr,D3DLOCK_READONLY));
        unsigned pixel;std::memcpy(&pixel,static_cast<unsigned char*>(lock.pBits)+8*lock.Pitch+8*4,4);HR(readback.p->UnlockRect());return pixel;
    }
};
static Reference* reference;
#include "matrixgame_material_gl_reference.hpp"
static MaterialGlReference* gl_reference;
#endif
static unsigned accepted{},disabled_alpha{},null_texture{},differential{},mismatches{};
static unsigned gl_differential{},gl_mismatches{};
static unsigned diagnostic_count{};
static void Diagnose(const char* label){
#ifdef _WIN32
    const auto got=reference->Draw(trace.combiner.State());
    ++diagnostic_count;
    if(diagnostic_count<5)std::printf("unsupported native material: %s actual HAL ARGB=%08x\n",label,got);
#endif
}
static void Check(const char* label){
    trace.combiner.Factor(factor_argb);
    mat::Uniforms u;
    try{u=trace.combiner.Prepare();}
    catch(const mat::Incomplete& e){
        const std::string message=e.what();
        if(message.find("disabled alpha")!=std::string::npos)++disabled_alpha;
        else if(message.find("null texture")!=std::string::npos)++null_texture;
        else throw;
        Diagnose(label);
        return;
    }
    ++accepted;std::array<mat::Color,8> samples{};
    for(unsigned i=0;i<8;++i)if(trace.textures[i])samples[i]=sample_colors.at(trace.textures[i]);
    const auto expected=mat::Evaluate(u,Unpack(diffuse_argb),samples);
    for(float x:expected)assert(x>=0 && x<=1);
#ifdef _WIN32
    if(gl_reference){const auto gl=gl_reference->Draw(u,samples,trace.combiner.Bound(),Unpack(diffuse_argb));++gl_differential;
        for(unsigned c=0;c<4;++c)if(std::abs(gl[c]-expected[c])*255>2.1f){++gl_mismatches;std::printf("GLSL MISMATCH %s channel=%u scalar=%f GLSL=%f\n",label,c,expected[c],gl[c]);break;}}
    const auto got=Unpack(reference->Draw(trace.combiner.State()));++differential;
    for(unsigned i=0;i<4;++i)if(std::abs(got[i]-expected[i])*255>2.1f){
        ++mismatches;std::printf("MISMATCH %s channel=%u cpu=%f HAL=%f\n",label,i,expected[i],got[i]);
        for(unsigned j=0;j<8 && u.stages[j].color[0]!=1;++j){const auto& s=u.stages[j];std::printf("stage %u rgb=%u,%u,%u result=%u alpha=%u,%u,%u bound=%u\n",j,s.color[0],s.color[1],s.color[2],s.color[3],s.alpha[0],s.alpha[1],s.alpha[2],unsigned(trace.textures[j]!=nullptr));}break;
    }
#endif
}
#include "matrixgame_fixed_vertex_reference.hpp"
int main(int argc,char** argv){
    std::setvbuf(stdout,nullptr,_IONBF,0);
    rp3g::Current().device=&trace;rp3g::Current().ready=true;
#ifdef _WIN32
    Reference gpu;reference=&gpu;
#endif
    std::array<FixtureTexture,6> inputs{};
    constexpr unsigned colors[]{0xb14b87d3,0x63d12b79,0xc55fdf31,0x9a7139e7,0x573f95af,0xdb9d5f23};
    for(unsigned i=0;i<inputs.size();++i){
#ifdef _WIN32
        inputs[i].texture=gpu.Texture(colors[i]);
#else
        inputs[i].texture=&inputs[i]; // Explicit CPU-only texture identity.
#endif
        sample_colors[inputs[i].texture]=Unpack(colors[i]);
    }
    fixture_map.reflection=&inputs[5];
    fixture_map.m_Macrotexture=&inputs[3];CBottomTextureUnion::Get(0).texture=&inputs[0];
    FixtureWater water;fixture_map.m_Water=&water;fixture_map.m_WaterColor=0x85776747;
#ifdef _WIN32
    if(argc>1 && std::string(argv[1])=="--diagnose"){
        for(unsigned mask=0;mask<8;++mask)for(unsigned a0=0;a0<3;++a0)for(unsigned a1=0;a1<3;++a1)for(unsigned a2=0;a2<3;++a2){
            trace.combiner=mat::Combiner{};trace.textures.fill(nullptr);const unsigned ops[]{a0,a1,a2};
            for(unsigned s=0;s<3;++s){
                trace.BindBorrowedTexture(s,(mask&(1u<<s))?inputs[s].texture:nullptr);
                trace.combiner.Set(s,1,2);trace.combiner.Set(s,2,1);
                trace.combiner.Set(s,4,ops[s]==0?1:2);trace.combiner.Set(s,5,ops[s]==1?2:1);
            }
            std::printf("ALPHA mask=%u ops=%u%u%u argb=%08x\n",mask,a0,a1,a2,gpu.Draw(trace.combiner.State()));
        }
        rp3g::Current()=rp3g::State{};return 0;
    }
    std::unique_ptr<MaterialGlReference> gl;
    if(argc>2 && std::string(argv[1])=="--fragment"){gl=std::make_unique<MaterialGlReference>(argv[2]);gl_reference=gl.get();}
#endif
    // All datasets reuse the exact original callback selection and pass order.
    for(unsigned dataset=0;dataset<4;++dataset){
        diffuse_argb=std::array<unsigned,4>{0xcc336699,0x00374f93,0xffb94d71,0x7f97b351}[dataset];
        factor_argb=std::array<unsigned,4>{0x708090a0,0xff3169b7,0x00c59753,0xb317c957}[dataset];
        for(unsigned i=0;i<inputs.size();++i){
            const auto value=dataset==0?colors[i]:dataset==1?0u:dataset==2?0xffffffffu:(colors[i]^0xa5f073c9u);
            sample_colors[inputs[i].texture]=Unpack(value);
#ifdef _WIN32
            gpu.Update(inputs[i].texture,value);
#endif
        }
    // Independent operation/modifier cases, including saturation. A real
    // D3D9 draw/readback is compared on Windows; no software GPU is substituted.
    for(unsigned op=2;op<=16;++op)for(unsigned modifier:{0u,16u,32u}){
        trace.combiner=mat::Combiner{};trace.textures.fill(nullptr);
        trace.BindBorrowedTexture(0,inputs[0].texture);
        trace.combiner.Set(0,1,op);trace.combiner.Set(0,2,2|modifier);trace.combiner.Set(0,3,0);
        trace.combiner.Set(0,4,op);trace.combiner.Set(0,5,2|modifier);trace.combiner.Set(0,6,0);
        char label[80];std::snprintf(label,sizeof(label),"equation op=%u modifier=%u",op,modifier);Check(label);
    }
    for(unsigned caps:{2u,3u,4u,6u})for(unsigned bits=0;bits<8;++bits)for(unsigned group=0;group<3;++group)for(unsigned alpha=0;alpha<2;++alpha){
        fixture_caps=caps;trace.combiner=mat::Combiner{};trace.textures.fill(nullptr);inputs[0].flags=alpha?TF_ALPHATEST:0;
        SMatrixSkin skin{};skin.m_Tex=&inputs[0];skin.m_TexGloss=bits&1?&inputs[1]:nullptr;skin.m_TexMask=bits&2?&inputs[2]:nullptr;skin.m_TexBack=bits&4?&inputs[3]:nullptr;
        skin.m_tu=.25f;skin.m_tv=-.5f;skin.Prepare(GSParam(group));
        bool more=true;for(int pass=0;more;++pass){assert(pass<4);more=skin.m_SetupStages(&skin,0,pass);skin.m_SetupTex(&skin,group==GSP_ORDINAL?0:reinterpret_cast<uintptr_t>(inputs[4].texture),pass);char label[128];std::snprintf(label,sizeof(label),"skin caps=%u bits=%u group=%u alpha=%u pass=%d",caps,bits,group,alpha,pass);Check(label);}
        skin.m_SetupClear(&skin,0);
        skin.m_SetupTexShadow(&skin); // Original alpha-test shadow binding.
    }
    struct TerrainCase {const char* name;void(*stages)(int,bool);void(*textures)(CTextureManaged*,CTextureManaged*,int);void(*clear)();};
    const TerrainCase terrain[]{
        {"TerSurf",TerSurf,TerSurfTex,TerSurfClear},{"TerSurfW",TerSurfW,TerSurfTex,TerSurfClear},
        {"TerSurfM",TerSurfM,TerSurfTexM,TerSurfClear},{"TerSurfMW",TerSurfMW,TerSurfTexM,TerSurfClear},
        {"TerSurfGloss",TerSurfGloss,TerSurfTexGloss,TerSurfClearGloss},{"TerSurfGlossW",TerSurfGlossW,TerSurfTexGloss,TerSurfClearGloss},
        {"TerSurfGlossM",TerSurfGlossM,TerSurfTexGlossM,TerSurfClearGlossM},{"TerSurfGlossMW",TerSurfGlossMW,TerSurfTexGlossM,TerSurfClearGlossM}};
    for(const auto& t:terrain)for(bool wrap:{false,true}){
        trace.combiner=mat::Combiner{};trace.textures.fill(nullptr);
        t.stages(0,wrap);t.textures(&inputs[0],&inputs[1],0);Check(t.name);t.clear();
    }
    for(bool macro:{false,true}){
        trace.combiner=mat::Combiner{};trace.textures.fill(nullptr);
        if(macro){TerBotM(0);TerBotTexM(0,0);}else{TerBot(0);TerBotTex(0,0);}
        Check(macro?"TerBotM":"TerBot");TerBotClear();
    }
    for(bool solid:{false,true}){
        trace.combiner=mat::Combiner{};trace.textures.fill(nullptr);trace.BindBorrowedTexture(0,inputs[0].texture);
        if(solid)WaterSolid_t2(&inputs[1],&inputs[5],0);else WaterAlpha_t3(&inputs[1],&inputs[5],0);
        Check(solid?"WaterSolid_t2":"WaterAlpha_t3");
        if(solid)WaterClearSolid_t2();else WaterClearAlpha_t3();
    }
    // TEMP writes must not update CURRENT; both channels read pre-stage data.
    mat::Combiner temp;for(unsigned i=0;i<3;++i){temp.Bind(i,true);temp.Set(i,4,2);temp.Set(i,5,1);}
    temp.Set(0,1,2);temp.Set(0,2,2);temp.Set(0,28,5);
    temp.Set(1,1,4);temp.Set(1,2,5);temp.Set(1,3,1);
    temp.Set(2,1,1);
    std::array<mat::Color,8> samples; samples.fill({.2f,.3f,.4f,.5f});
    const auto color=mat::Evaluate(temp.Prepare(),{.5f,.5f,.5f,.75f},samples);
    assert(std::abs(color[0]-.1f)<1e-6 && std::abs(color[3]-.75f)<1e-6);
    temp.Set(1,1,1); // A final TEMP destination is itself rasterized.
    const auto last_temp=mat::Evaluate(temp.Prepare(),{.5f,.5f,.5f,.75f},samples);
    assert(std::abs(last_temp[0]-.2f)<1e-6 && std::abs(last_temp[3]-.75f)<1e-6);
    bool refused=false;const auto previous=temp.Get(0,1);
    try{temp.Set(0,1,22);}catch(const mat::Incomplete&){refused=true;}assert(refused && temp.Get(0,1)==previous);
    refused=false;try{temp.Set(8,1,2);}catch(const std::out_of_range&){refused=true;}assert(refused);
    mat::Combiner null;assert(null.Prepare().stages[0].color[0]==1 && !null.Bound()[0] && null.Get(0,1)==4);
    }
    RunFixedVertexChecks();
    std::printf("material cases accepted=%u disabled_alpha=%u null_texture=%u D3D9_differential=%u mismatches=%u diagnostic_cases=%u GLSL_differential=%u GLSL_mismatches=%u; deko GPU NOT RUN\n",accepted,disabled_alpha,null_texture,differential,mismatches,diagnostic_count,gl_differential,gl_mismatches);
    assert(accepted && !disabled_alpha && !null_texture && !mismatches && !gl_mismatches);
    rp3g::Current()=rp3g::State{};
}
