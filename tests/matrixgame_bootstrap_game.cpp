// Additional real game dependencies in the same bootstrap; no SDK type mixing.
#include "init_runtime.hpp"
#include "game_defaults.hpp"
#include MATRIXGAME_FULL_CONFIG_HEADER
#include MATRIXGAME_FULL_CAMERA_HEADER
#include MATRIXGAME_FULL_KONUS_HEADER
#include MATRIXGAME_FULL_CURSOR_HEADER
#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>
#include "MatrixGame.h"
#include "MatrixLoadProgress.hpp"
#include <vector>
#include <limits>
#include "3g.hpp"
#include "Texture.hpp"
#include "legacy_texture_backend.hpp"
#include "MatrixRenderPipeline.hpp"
#ifdef _WIN32
#include "platform_win32.hpp"
#endif
using namespace srhd_awa::matrixgame;
void VerifyBootstrapMapCamera(NativeInitRuntime& runtime) {
    assert(runtime.GameDefaultsReady() && !runtime.MapCamera() && !g_MatrixMap);
    const auto gpu=runtime.GpuReady();
    SRobotsSettings settings{};
    settings.m_ResolutionX=g_ScreenX>0?g_ScreenX:1280;
    settings.m_ResolutionY=g_ScreenY>0?g_ScreenY:720;
    settings.m_MaxDistance=25;
    const auto old_x=g_ScreenX,old_y=g_ScreenY;const auto old_far=MAX_VIEW_DISTANCE;
    for(int failure=0;failure<3;++failure) {
        auto bad=settings;
        if(failure==0)bad.m_ResolutionX=0;
        if(failure==1)bad.m_MaxDistance=-100;
        if(failure==2)bad.m_MaxDistance=std::numeric_limits<float>::quiet_NaN();
        bool rejected=false;try{runtime.InitializeMapCamera(bad);}catch(const std::invalid_argument&){rejected=true;}
        assert(rejected && !runtime.MapCamera() && g_ScreenX==old_x && g_ScreenY==old_y && MAX_VIEW_DISTANCE==old_far);
    }
    runtime.InitializeMapCamera(settings);auto* camera=runtime.MapCamera();assert(camera);
    assert(MAX_VIEW_DISTANCE==5000 && g_ScreenX==settings.m_ResolutionX && g_ScreenY==settings.m_ResolutionY);
    assert(std::fabs(camera->GetResXInversed()-1.f/settings.m_ResolutionX)<1e-8f);
    assert(std::fabs(camera->GetResYInversed()-1.f/settings.m_ResolutionY)<1e-8f);
    assert(camera->GetAngleZ()==g_Config.m_CamBaseAngleZ);
    const auto& config=g_Config.m_CamParams[CAMERA_STRATEGY];
    assert(std::fabs(camera->GetAngleX()-(config.m_CamRotAngleMin+(config.m_CamRotAngleMax-config.m_CamRotAngleMin)*config.m_CamAngleParam))<1e-6f);
    const auto& projection=camera->GetProjMatrix();
    const float focal=1.f/std::tan(float(CAM_HFOV)/2);
    assert(std::fabs(projection._11-focal*settings.m_ResolutionY/settings.m_ResolutionX)<1e-5f);
    assert(std::fabs(projection._22-focal)<1e-5f);
    // Independent near/far endpoints of the original LH projection.
    assert(std::fabs(projection._33+projection._43)<1e-6f);
    assert(std::fabs(projection._33+projection._43/5000.f-1.f)<1e-6f);
    bool duplicate=false;try{runtime.InitializeMapCamera(settings);}catch(const std::logic_error&){duplicate=true;}
    assert(duplicate && runtime.MapCamera()==camera && runtime.GpuReady()==gpu && !g_MatrixMap);
    runtime.FinishMapCamera();runtime.FinishMapCamera();assert(!runtime.MapCamera());
    runtime.InitializeMapCamera(settings); // Actual destructor is also exercised by shared Deinit.
    std::puts("original SetMaxCameraDistance -> CMatrixCamera ctor/projection EXECUTED; invalid config atomic refusal, duplicate owner, cleanup/reentry PASS; map NOT CONSTRUCTED; camera held until common Deinit");
}
void VerifyBootstrapRenderer(NativeInitRuntime& runtime) {
    assert(!g_Render && !runtime.RenderPipeline());
    if(!runtime.GpuReady()) {
        for(int retry=0;retry<2;++retry) {
            bool rejected=false;try{runtime.InitializeRenderPipeline();}
            catch(const platform3g::Unsupported& error){rejected=std::string(error.what())=="3G device negotiation not complete";}
            assert(rejected && !g_Render && !runtime.RenderPipeline() && runtime.CpuReady());
        }
        runtime.FinishRenderPipeline();runtime.FinishRenderPipeline();
        std::puts("full original CRenderPipeline constructor ENTERED -> first actual caps query Unsupported without negotiated device; original game-heap allocation unwind/retry PASS; callback selection NOT RUN");
        return;
    }
    const auto textures=platform3g::Current().caps.max_simultaneous_textures;
    if(textures<2) {
        bool rejected=false;try{runtime.InitializeRenderPipeline();}catch(const Base::CException&){rejected=true;}
        assert(rejected && !g_Render && !runtime.RenderPipeline());
        std::puts("original renderer rejects actual SDK MaxSimultaneousTextures<2; no callback success synthesized");return;
    }
    runtime.InitializeRenderPipeline();auto* renderer=runtime.RenderPipeline();assert(renderer && g_Render==renderer);
    assert(renderer->m_WaterSolid && renderer->m_WaterAlpha && renderer->m_WaterClearSolid && renderer->m_WaterClearAlpha);
    const auto alpha_pass=textures>=3?1:(platform3g::ReadyDevice().ReadAdapterDisplayMode(0).Format==platform3g::PixelFormat::Xrgb8?2:1);
    assert(renderer->m_WaterPassSolid==1 && renderer->m_WaterPassAlpha==alpha_pass);
    for(unsigned i=0;i<2;++i)assert(renderer->m_TerBot[i] && renderer->m_TerBotTex[i] && renderer->m_TerBotClear[i] && renderer->m_TerBotPass[i]==1);
    for(unsigned i=0;i<SURF_TYPES_COUNT;++i)assert(renderer->m_TerSurf[i] && renderer->m_TerSurfTex[i] && renderer->m_TerSurfClear[i] && renderer->m_TerSurfPass[i]>=1);
    assert(renderer->m_TerSurfPass[SURF_TYPE]==1 && renderer->m_TerSurfPass[SURF_TYPE_WHITE]==1);
    assert(renderer->m_TerSurfPass[SURF_TYPE_MACRO]==(textures>=3?1:2));
    assert(renderer->m_TerSurfPass[SURF_TYPE_GLOSS]==(textures>=4?1:2));
    assert(renderer->m_TerSurfPass[SURF_TYPE_MACRO_GLOSS]==(textures>=4?1:2));
    bool duplicate=false;try{runtime.InitializeRenderPipeline();}catch(const std::logic_error&){duplicate=true;}
    assert(duplicate && runtime.RenderPipeline()==renderer && g_Render==renderer);
    auto& device=platform3g::ReadyDevice();
    platform3g::CheckDeviceResult(device.SetRenderState(RasterRegister::BlendEnable,1),"seed renderer clear blend");
    platform3g::CheckDeviceResult(device.SetRenderState(RasterRegister::DepthWrite,0),"seed renderer clear depth");
    renderer->m_TerBotClear[0](); // Actual original callback -> real SDK provider.
    assert(device.ReadRenderState(RasterRegister::BlendEnable)==0 && device.ReadRenderState(RasterRegister::DepthWrite)==1);
    runtime.FinishRenderPipeline();runtime.FinishRenderPipeline();assert(!g_Render && !runtime.RenderPipeline());
    runtime.InitializeRenderPipeline();assert(g_Render==runtime.RenderPipeline());
    std::puts("original renderer constructor/callback table and TerBotClear EXECUTED on actual SDK caps/state; cleanup/reentry PASS; draw NOT RUN");
}
void VerifyBootstrapOriginalTexture(NativeInitRuntime& runtime,const wchar_t* path) {
    namespace lt=srhd_awa::matrixgame::legacy_texture;
    assert(runtime.CpuReady() && !runtime.GpuReady() && lt::RegistrySize()==0);
    // Observe protected original state without overriding any virtual method.
    struct Inspect: CTextureManaged {std::uint32_t RawFlags()const{return m_Flags;}};
    auto* texture=HNew(g_CacheHeap) Inspect;
    struct Owner {CCacheData* value;~Owner(){if(value)CCache::Destroy(value);}} owner{texture};
    assert(lt::RegistrySize()==1 && texture->m_Type==CacheClass::TextureManaged && !texture->m_Cache);
    texture->m_Name=std::wstring(path)+L"?Alpha?Trans";
    texture->ParseFlags(texture->m_Name);
    assert(texture->RawFlags()==(TF_ALPHABLEND|TF_ALPHATEST));
    for(int retry=0;retry<2;++retry) {
        bool rejected=false;
        try{texture->Load();}catch(const platform3g::Unsupported& error){rejected=std::string(error.what())=="original texture DecodeTexture: 3G device negotiation not complete";}
        assert(rejected && !texture->DX() && !texture->IsLoaded() && lt::RegistrySize()==1);
        assert(texture->RawFlags()==(TF_ALPHABLEND|TF_ALPHATEST));
        texture->Unload(); // Original managed branch intentionally retains storage.
    }
    texture->m_Name=path;
    bool convert_rejected=false;
    try{texture->LoadTextureFromFile(true,TextureMemoryPool::Managed);}
    catch(const platform3g::Unsupported& error){convert_rejected=std::string(error.what())=="original texture CreateTexture: 3G device negotiation not complete";}
    assert(convert_rejected && !texture->DX() && lt::RegistrySize()==1); // Temporary original texture must be destroyed on unwind.
    texture->m_Name=std::wstring(path)+L".missing-original-resource";
    bool missing=false;try{texture->Load();}catch(const platform3g::Unsupported&){assert(false);}catch(...){missing=true;}
    assert(missing && !texture->DX() && lt::RegistrySize()==1 && runtime.CpuReady());
    CCache::Destroy(texture);owner.value=nullptr;
    // CCache::Destroy requires a non-null pointer; release guard explicitly.
    assert(lt::RegistrySize()==0);
    for(bool converted:{false,true}) {
        bool rejected=false;try{runtime.LoadOriginalManagedTexture(path,converted);}
        catch(const platform3g::Unsupported& error){rejected=std::string(error.what())==std::string("original texture ")+(converted?"CreateTexture":"DecodeTexture")+": 3G device negotiation not complete";}
        assert(rejected && !runtime.OriginalManagedTexture() && lt::RegistrySize()==0 && runtime.CpuReady());
    }
    runtime.FinishOriginalManagedTexture();runtime.FinishOriginalManagedTexture();
    std::puts("original CTextureManaged constructor/ParseFlags/Load/Init and real encoded-file prefix EXECUTED; bitmap conversion reaches actual GPU boundary; missing-resource, temporary unwind and registry cleanup PASS; SDK texture creation NOT RUN");
}
void VerifyBootstrapOriginalTextureDevice(NativeInitRuntime& runtime,const wchar_t* path) {
    namespace lt=srhd_awa::matrixgame::legacy_texture;
    assert(runtime.GpuReady() && lt::RegistrySize()==0);
    {
        runtime.LoadOriginalManagedTexture(path);auto* texture=runtime.OriginalManagedTexture();
        assert(texture);
        struct Owner {NativeInitRuntime& runtime;~Owner(){runtime.FinishOriginalManagedTexture();}} owner{runtime};
        assert(lt::RegistrySize()==1);texture->m_Name=path;texture->Load();
        auto* stored=texture->DX();assert(stored && texture->IsLoaded());
        lt::LevelDesc desc{};platform3g::CheckDeviceResult(lt::Describe(stored,0,&desc),"actual texture description");
        assert(desc.Width==texture->GetSizeX() && desc.Height==texture->GetSizeY() && desc.Format==21);
        texture->Load();texture->Init(false);texture->Preload();texture->Unload();
        assert(texture->DX()==stored && lt::RegistrySize()==1); // Original managed retention policy.
        CBitmap input;input.LoadFromPNG(path);
        auto* converted=texture->LoadTextureFromFile(true,TextureMemoryPool::Managed);
        struct ConvertedOwner {IDirect3DTexture9* value;~ConvertedOwner(){lt::Release(value);}} converted_owner{converted};
        assert(converted && lt::RegistrySize()==1);
        platform3g::CheckDeviceResult(lt::Describe(converted,0,&desc),"actual converted texture description");
        assert(desc.Width==input.SizeX() && desc.Height==input.SizeY());
        assert(desc.Format==(input.BytePP()==4?26u:24u));
        lt::LockedRect pixels{};platform3g::CheckDeviceResult(lt::Lock(converted,0,&pixels,nullptr,0),"read actual converted pixel");
        const auto* raw=static_cast<const std::uint8_t*>(pixels.pBits);const auto* source=input.Data();
        if(input.BytePP()==4) {
            assert(raw[0]==((source[1]&0xf0)|(source[2]>>4)));
            assert(raw[1]==((source[3]&0xf0)|(source[0]>>4)));
        }
        platform3g::CheckDeviceResult(lt::Unlock(converted,0),"unlock converted pixel");
    }
    assert(lt::RegistrySize()==0);
    runtime.FinishOriginalManagedTexture();runtime.LoadOriginalManagedTexture(path);
    bool duplicate=false;try{runtime.LoadOriginalManagedTexture(path);}catch(const std::logic_error&){duplicate=true;}
    assert(duplicate && runtime.OriginalManagedTexture() && lt::RegistrySize()==1);
    // Keep the actual owned texture alive for the next real DetachDevice.
    std::puts("original managed texture SDK Load/Init/Preload/Unload and original 16-bit bitmap conversion EXECUTED with actual resource/pixel readback; GPU draw NOT RUN");
}
namespace {
std::vector<float>* captured_progress{};
void RecordProgress(float value){assert(captured_progress);captured_progress->push_back(value);}
void ThrowProgress(float){throw std::runtime_error("progress callback fixture failure");}
}
void VerifyBootstrapProgress(NativeInitRuntime& runtime) {
    assert(!runtime.LoadProgress() && !g_LoadProgress && runtime.OriginalStaticsReady());
    std::vector<float> values;values.reserve(16);
    struct Restore {
        SMGDRangersInterface* iface;std::vector<float>* output;
        ~Restore(){g_RangersInterface=iface;captured_progress=output;}
    } restore{g_RangersInterface,captured_progress};
    captured_progress=&values;SMGDRangersInterface iface{};iface.m_ProgressBar=RecordProgress;
    g_RangersInterface=&iface;
    {
        CLoadProgress foreign;g_LoadProgress=&foreign;
        bool refused=false;try{runtime.InitializeLoadProgress();}catch(const std::logic_error&){refused=true;}
        assert(refused && g_LoadProgress==&foreign && !runtime.LoadProgress());
    }
    assert(!g_LoadProgress); // Original stack publication is now invalidated on destruction.
    runtime.InitializeLoadProgress();auto* progress=runtime.LoadProgress();assert(progress && g_LoadProgress==progress);
    {CLoadProgress unpublished;}
    assert(g_LoadProgress==progress); // Destroying a different object must preserve the real publisher.
    bool duplicate=false;try{runtime.InitializeLoadProgress();}catch(const std::logic_error&){duplicate=true;}
    assert(duplicate && runtime.LoadProgress()==progress && g_LoadProgress==progress);
    progress->SetCurLP(LP_PRELOADROBOTS);progress->InitCurLP(10);progress->SetCurLPPos(0);assert(values.empty());
    progress->SetCurLPPos(10);assert(values.size()==1);
    progress->SetCurLPPos(10);progress->SetCurLPPos(1);assert(values.size()==1);
    progress->SetCurLP(LP_LOADINGMAP);progress->InitCurLP(10);progress->SetCurLPPos(5);progress->SetCurLPPos(10);
    progress->SetCurLP(LP_PREPARINGOBJECTS);progress->InitCurLP(10);progress->SetCurLPPos(10);
    progress->SetCurLP(LP_PREPARININTERFACE);progress->InitCurLP(10);progress->SetCurLPPos(10);
    assert(values.size()==5 && values.back()==1.f);
    const float expected[]{70.f/550,220.f/550,370.f/550,520.f/550,1.f};
    for(std::size_t i=0;i<values.size();++i) {
        assert(std::isfinite(values[i]) && values[i]>=0 && values[i]<=1);
        assert(std::fabs(values[i]-expected[i])<=1.f/LPACCURACY);
        if(i)assert(values[i]>values[i-1]);
    }
    runtime.FinishLoadProgress();runtime.FinishLoadProgress();assert(!g_LoadProgress && !runtime.LoadProgress());
    runtime.InitializeLoadProgress();progress=runtime.LoadProgress();
    const auto old_count=values.size();g_RangersInterface=nullptr;
    progress->SetCurLP(LP_PRELOADROBOTS);progress->InitCurLP(10);progress->SetCurLPPos(10);
    g_RangersInterface=&iface;progress->SetCurLPPos(10);assert(values.size()==old_count);
    runtime.FinishLoadProgress();
    iface.m_ProgressBar=ThrowProgress;bool unwound=false;
    try {
        CLoadProgress local;g_LoadProgress=&local;
        local.SetCurLP(LP_PRELOADROBOTS);local.InitCurLP(1);local.SetCurLPPos(1);
    }catch(const std::runtime_error& e){unwound=std::string(e.what())=="progress callback fixture failure";}
    assert(unwound && !g_LoadProgress && runtime.CpuReady());
    iface.m_ProgressBar=RecordProgress;
    runtime.InitializeLoadProgress();runtime.FinishLoadProgress();
    assert(!g_LoadProgress && !runtime.LoadProgress());
    std::puts("full original CLoadProgress TU EXECUTED: actual progress callback fixture, original 70/300/150/30 weights, monotonic/duplicate/null-interface behavior, foreign publisher refusal, stack/error lifetime and reentry PASS; no real Rangers host UI claimed");
}
void VerifyBootstrapLegacy(NativeInitRuntime& runtime,void* module,platform3g::Window& window,platform3g::Device& device,bool negotiated) {
    namespace p=platform3g;
    assert(&g_HInst==&p::Current().module && &g_Wnd==&p::Current().window_identity && &g_Flags==&p::Current().flags);
    assert(&g_ScreenX==&p::Current().configured_width && &g_ScreenY==&p::Current().configured_height && &g_AvailableTexMem==&p::Current().available_texture_mib);
    Base::CBlockPar unused_original_config;
    auto& config=g_MatrixData?*g_MatrixData->BlockGet(L"Config"):unused_original_config;
    auto* data=g_MatrixData;auto* cache=g_Cache;auto* heap=g_MatrixHeap;
    if(!negotiated) {
        bool unregistered=false;
        try{L3GInitAsDLL(module,config,L"MatrixGame",L"Matrix Game",window.Identity(),0,0);}
        catch(const p::Unsupported& e){unregistered=std::string(e.what())=="legacy DLL entry requires registered borrowed Window/Device providers";}
        assert(unregistered);
        for(int i=0;i<2;++i) {
            runtime.BindLegacyProviders(window,device);
            bool unsupported=false;
            try{L3GInitAsDLL(module,config,L"MatrixGame",L"Matrix Game",window.Identity(),0,0);}
            catch(const p::Unsupported& e){unsupported=std::string(e.what())=="legacy D3D identities unsupported by this provider";}
            assert(unsupported && runtime.CpuReady() && !runtime.GpuReady() && InspectCpu3GState().bound);
            L3GDeinit();L3GDeinit();
        }
        assert(g_MatrixData==data && g_Cache==cache && g_MatrixHeap==heap);
        std::puts("legacy L3GInitAsDLL ABI EXECUTED -> explicit unregistered/missing SDK identity refusal; repeated L3GDeinit preserves real CPU/cache/data owner");return;
    }
    const auto handles=device.ReadLegacyHandles();assert(handles.api && handles.device);
    const auto leases=device.OutstandingStateLeases();const auto world=device.ReadWorldTransform();
    bool wrong_api=false;
    try{L3GInitAsDLL(module,config,L"MatrixGame",L"Matrix Game",window.Identity(),handles.api^1,handles.device);}
    catch(const std::invalid_argument&){wrong_api=true;}
    assert(wrong_api && runtime.GpuReady() && device.OutstandingStateLeases()==leases && device.ReadWorldTransform()==world);
    L3GInitAsDLL(module,config,L"MatrixGame",L"Matrix Game",window.Identity(),handles.api,handles.device);
    assert(runtime.GpuReady() && device.OutstandingStateLeases()==1 && !runtime.Cursor());
    assert(g_HInst==module && g_Wnd==window.Identity() && g_AvailableTexMem==p::Current().available_texture_mib);
    L3GDeinit();L3GDeinit();
    assert(runtime.CpuReady() && !runtime.GpuReady() && device.OutstandingStateLeases()==0 && InspectCpu3GState().bound);
    assert(g_MatrixData==data && g_Cache==cache && g_MatrixHeap==heap);
    runtime.BindLegacyProviders(window,device);
    L3GInitAsDLL(module,config,L"MatrixGame",L"Matrix Game",window.Identity(),handles.api,handles.device);
    assert(runtime.GpuReady() && device.OutstandingStateLeases()==1);
    std::puts("legacy L3GInitAsDLL/L3GDeinit ABI -> actual SDK identities/original DLL algorithm EXECUTED; invalid identity no mutation, live reentry, cleanup, cold rebind PASS");
}
void VerifyBootstrapConfiguration(NativeInitRuntime& runtime,const wchar_t* path,bool negotiated) {
    assert(!g_MatrixData && runtime.GameDefaultsReady());
    auto settings=SRobotsSettings::generate_default_settings();
    settings.m_IzvratMS=true;settings.m_LandTexturesGloss=false;settings.m_ObjTexturesGloss=false;
    settings.m_RobotShadow=1;settings.m_ShowProjShadows=false;settings.m_ShowStencilShadows=false;
    settings.m_SkyBox=3;settings.m_SoftwareCursor=true;settings.m_Brightness=.25f;settings.m_Contrast=.75f;
    runtime.ApplyGameSettings(settings);
    assert(g_Config.m_IzvratMS && !g_Config.m_LandTexturesGloss && !g_Config.m_ObjTexturesGloss);
    assert(g_Config.m_RobotShadow==SHADOW_STENCIL && !g_Config.m_ShowProjShadows && !g_Config.m_ShowStencilShadows);
    assert(g_Config.m_SkyBox==3 && g_Config.m_SoftwareCursor);
    assert(g_Config.m_GammaR.brightness==.25f && g_Config.m_GammaG.contrast==.75f && g_Config.m_GammaB.gamma==1.f);
    settings.m_RobotShadow=0;runtime.ApplyGameSettings(settings);assert(g_Config.m_RobotShadow==SHADOW_OFF);
    std::puts("full original CMatrixConfig::ApplySettings EXECUTED: supplied flags/shadow/gamma copied into real g_Config");
    bool missing=false;try{runtime.LoadGameData(L"__matrixgame_missing_original_configuration__.txt");}catch(...){missing=true;}
    assert(missing && !g_MatrixData && runtime.CpuReady());
    runtime.LoadGameData(path);auto* data=g_MatrixData;assert(data && data->BlockCount(L"Config")==1);
    bool duplicate=false;try{runtime.LoadGameData(path);}catch(const std::logic_error&){duplicate=true;}
    assert(duplicate && g_MatrixData==data);
    for(int attempt=0;attempt<2;++attempt) {
        bool unsupported=false;
        try{runtime.ReadGameParameters();}
        catch(const platform3g::Unsupported& e){unsupported=true;
            if(!negotiated)assert(std::string(e.what())=="3G device negotiation not complete");
            std::printf("full original ReadParams EXECUTED on pinned data.txt through config/cursors -> %s\n",e.what());
        }
        if(!negotiated)assert(unsupported);
        const auto* cursors=data->BlockGet(L"Cursors");
        assert(g_MatrixData==data && runtime.CpuReady() && runtime.GameDefaultsReady());
        assert(g_Config.m_CursorsCnt==cursors->ParCount() && g_Config.m_CursorsCnt==5);
        for(int i=0;i<g_Config.m_CursorsCnt;++i) {
            assert(g_Config.m_Cursors[i].key==cursors->ParGetName(i));
            assert(g_Config.m_Cursors[i].val==cursors->ParGet(i));
        }
        assert(!g_Config.m_SoftwareCursor && g_Config.m_LandTexturesGloss && g_Config.m_ObjTexturesGloss);
        assert(g_Config.m_SkyBox==1 && !g_Config.m_IzvratMS && g_MaxFPS==100);
        assert(g_Config.m_GammaR.brightness==.5f && g_Config.m_GammaG.contrast==.5f && g_Config.m_GammaB.gamma==1.f);
        if(unsupported)assert(!g_Config.m_Labels && !g_Config.m_Descriptions);
    }
    // Cleanup-only fixture for the arrays allocated after gamma in the full
    // parser. This does not claim that its post-gamma gameplay body ran.
    g_Config.m_Labels=static_cast<std::wstring*>(HAlloc(sizeof(std::wstring)*LABELS_LAST,g_MatrixHeap));
    for(int i=0;i<LABELS_LAST;++i)new(g_Config.m_Labels+i)std::wstring();
    g_Config.m_Descriptions=static_cast<std::wstring*>(HAlloc(sizeof(std::wstring)*DESCRIPTIONS_LAST,g_MatrixHeap));
    for(int i=0;i<DESCRIPTIONS_LAST;++i)new(g_Config.m_Descriptions+i)std::wstring();
    g_Config.m_Labels[0]=data->BlockGet(L"Cursors")->ParGetName(0);
    g_Config.m_Descriptions[0]=data->BlockGet(L"Cursors")->ParGet(0);
    g_Config.Clear();g_Config.Clear();
    assert(!g_Config.m_Cursors && !g_Config.m_CursorsCnt && !g_Config.m_Labels && !g_Config.m_Descriptions && g_MatrixData==data);
    std::puts("original CBlockPar real data load + game-heap ownership, missing-file unwind, duplicate refusal and ReadParams retry PASS; direct repeated Clear also frees label/description cleanup fixtures; parser tail NOT EXECUTED on CPU");
}
void VerifyBootstrapTimer(NativeInitRuntime& runtime,platform3g::Window& window,platform3g::Device& device,bool negotiated) {
    namespace p=platform3g;
    assert(!p::Current().timer && !p::Current().timer_period);
    if(!negotiated) {
        bool refused=false;try{p::BeginLoopTimerPeriod(1);}catch(const p::Unsupported&){refused=true;}
        assert(refused && !p::Current().timer && runtime.CpuReady());
        std::puts("original loop timer call contract Unsupported without actual platform; no timer success synthesized");return;
    }
#ifdef _WIN32
    auto& actual=dynamic_cast<p::Win32Window&>(window);
    assert(actual.OutstandingTimerLeases()==0);
    bool invalid=false;
    try{p::BeginLoopTimerPeriod(0xffffffffu);}catch(const p::DeviceError& e){invalid=e.result!=0;}
    assert(invalid && !p::Current().timer && actual.OutstandingTimerLeases()==0);
    p::BeginLoopTimerPeriod(1);
    assert(p::Current().timer && p::Current().timer_period==1 && actual.OutstandingTimerLeases()==1);
    bool duplicate=false;try{p::BeginLoopTimerPeriod(1);}catch(const std::logic_error&){duplicate=true;}
    bool mismatch=false;try{p::EndLoopTimerPeriod(2);}catch(const std::logic_error&){mismatch=true;}
    assert(duplicate && mismatch && actual.OutstandingTimerLeases()==1 && p::Current().timer);
    assert(p::EndLoopTimerPeriod(1)==p::DeviceSuccess);
    assert(!p::Current().timer && actual.OutstandingTimerLeases()==0 && !p::Current().timer_period);
    assert(p::EndLoopTimerPeriod(1)==p::DeviceSuccess); // No unmatched OS release.
    p::BeginLoopTimerPeriod(1);runtime.DetachDevice();
    assert(actual.OutstandingTimerLeases()==0 && !p::Current().timer && runtime.CpuReady());
    runtime.AttachDevice(window,device);
    p::BeginLoopTimerPeriod(1); // Full owner Deinit will release this request.
    std::puts("actual WinMM timer begin/end EXECUTED: invalid/duplicate/mismatched requests, release, detach/reentry PASS; pending timer owned by same platform state");
#endif
}
void VerifyBootstrapVideo(NativeInitRuntime& runtime,platform3g::Window& window,platform3g::Device& device,bool negotiated) {
    namespace p=platform3g;
    auto settings=SRobotsSettings::generate_default_settings();
    if(!negotiated) {
        bool rejected=false;try{runtime.ApplyVideoSettings(settings);}catch(const p::Unsupported& e){rejected=std::string(e.what())=="3G device negotiation not complete";}
        assert(rejected && runtime.CpuReady() && !runtime.GpuReady() && !g_MatrixMap);
        std::puts("full original MatrixGame.cpp CGame::ApplyVideoParams ENTERED -> actual device unavailable; no synthetic display; owner preserved");return;
    }
    window.Show(p::Visibility::Normal);window.Update();
    const auto creation=device.ReadCreationParameters();
    const auto mode=device.ReadAdapterDisplayMode(creation.AdapterOrdinal);
    std::printf("video actual creation: adapter=%u type=%u flags=%u focus=%p bound=%p mode=%ux%u/%u\n",creation.AdapterOrdinal,creation.DeviceType,creation.BehaviorFlags,creation.FocusWindow,window.Identity(),mode.Width,mode.Height,mode.RefreshRate);
    // NULLREF reports a null focus window even when CreateDevice received a
    // HWND. The SDK-only helper checks every returned field against raw SDK.
    assert(window.Identity() && mode.Width>0 && mode.Height>0);
    const auto client=window.ReadClientRect(),outer=window.ReadWindowRect();
    settings.m_ResolutionX=client.right;settings.m_ResolutionY=client.bottom;
    settings.m_BPP=(mode.Format==p::PixelFormat::Xrgb8 || mode.Format==p::PixelFormat::Argb8)?32:16;
    settings.m_RefreshRate=0;settings.m_FSAASamples=0;
    runtime.ApplySamplerSettings(settings);
    const auto flags=p::Current().flags;const auto presentation=p::Current().presentation;
    const auto world=device.ReadWorldTransform();const auto leases=device.OutstandingStateLeases();
    for(int attempt=0;attempt<2;++attempt) {
        bool rejected=false;
        try{runtime.ApplyVideoSettings(settings);}
        catch(const p::Unsupported& e){
            rejected=true;const std::string message=e.what();
            assert(message=="window resize requires reset-safe presentation ownership");
            std::printf("original CGame::ApplyVideoParams EXECUTED actual adapter=%u type=%u mode=%ux%u/%u client=%dx%d -> %s\n",creation.AdapterOrdinal,creation.DeviceType,mode.Width,mode.Height,mode.RefreshRate,client.right,client.bottom,e.what());
        }
        assert(rejected && runtime.GpuReady() && runtime.GameDefaultsReady() && !g_MatrixMap);
        assert(p::Current().flags==flags && p::Current().presentation==presentation);
        assert(window.ReadClientRect()==client && window.ReadWindowRect()==outer);
        assert(device.ReadWorldTransform()==world && device.OutstandingStateLeases()==leases);
    }
    settings.m_FSAASamples=17;
    bool invalid=false;try{runtime.ApplyVideoSettings(settings);}catch(const p::Unsupported&){assert(false);}catch(...){invalid=true;}
    assert(invalid && p::Current().flags==flags && p::Current().presentation==presentation);
    // Real refresh mismatch selects the original fullscreen path. It must
    // reject before changing the caller's style or resetting its device.
    settings.m_FSAASamples=0;settings.m_RefreshRate=mode.RefreshRate+1;
    bool fullscreen_denied=false;
    try{runtime.ApplyVideoSettings(settings);}catch(const p::Unsupported& e){fullscreen_denied=std::string(e.what())=="fullscreen style requires reset-safe presentation ownership";}
    assert(fullscreen_denied && window.ReadWindowRect()==outer && window.ReadClientRect()==client);
    assert(p::Current().flags==flags && p::Current().presentation==presentation && !g_MatrixMap);
    std::puts("CGame video validation/window/fullscreen refusal, shared-state rollback and retry PASS; Reset/map/material/light NOT EXECUTED");
}
static_assert(sizeof(SKonusVertex)==20);
// Protected original cleanup is callable without constructing an effect or
// substituting its virtual gameplay methods. Statics come from original CPP.
struct KonusCleanupAccess:CMatrixEffectKonus {
    using CMatrixEffectKonus::MarkAllBuffersNoNeed;
    using CMatrixEffectKonus::StaticInit;
    static void SetBuffer(buffers::VertexBuffer* resource){m_VB=resource;}
    static bool Empty(){return m_VB==nullptr;}
};
void VerifyBootstrapGameCold() {
    assert(!g_MatrixHeap && !g_MatrixData && !g_Render && srhd_awa::matrixgame::legacy_texture::RegistrySize()==0);
    {
        auto* foreign=srhd_awa::matrixgame::legacy_texture::CreateManaged();
        struct Owner {CCacheData* value;~Owner(){CCache::Destroy(value);}} owner{foreign};
        NativeInitRuntime refused;bool rejected=false;
        try{refused.Begin(nullptr,nullptr);}catch(const std::logic_error&){rejected=true;}
        refused.Deinit();
        assert(rejected && !refused.OriginalStaticsReady() && !foreign->DX() && !g_MatrixHeap && !g_Cache);
        assert(srhd_awa::matrixgame::legacy_texture::RegistrySize()==1);
    }
    assert(srhd_awa::matrixgame::legacy_texture::RegistrySize()==0);
    {
        Base::CBlockPar foreign;g_MatrixData=&foreign;
        NativeInitRuntime refused;bool rejected=false;
        try{refused.Begin(nullptr,nullptr);}catch(const std::logic_error&){rejected=true;}
        refused.Deinit();assert(rejected && g_MatrixData==&foreign && !g_MatrixHeap && !g_Cache);
        g_MatrixData=nullptr;
    }
    {
        Base::CHeap foreign_heap;g_MatrixHeap=&foreign_heap;
        NativeInitRuntime refused;bool refused_owner=false;
        try{refused.Begin(nullptr,nullptr);}catch(const std::logic_error&){refused_owner=true;}
        assert(refused_owner && g_MatrixHeap==&foreign_heap && !g_Cache && !g_CacheHeap);
        refused.Deinit();assert(g_MatrixHeap==&foreign_heap);
        g_MatrixHeap=nullptr;
    }
    assert(!g_Config.m_Cursors && g_Config.m_CursorsCnt==0);
    NativeInitRuntime absent;bool rejected=false;
    try{absent.InitializeGameDefaults();}catch(const platform3g::Unsupported&){rejected=true;}
    assert(rejected && !absent.GameDefaultsReady());
}
namespace {
struct RefusingCursorWindow final:platform3g::Window {
    platform3g::Window& real;
    explicit RefusingCursorWindow(platform3g::Window& w):real(w){}
    void* Identity()const noexcept override{return real.Identity();}
    std::unique_ptr<platform3g::WindowAttachment> Attach(platform3g::MessageHandler h)override{return real.Attach(h);}
    void Show(platform3g::Visibility v)override{real.Show(v);}
    void ReleaseCursor()override{real.ReleaseCursor();}
    std::intptr_t DefaultMessage(std::uint32_t m,std::uintptr_t w,std::intptr_t l)override{return real.DefaultMessage(m,w,l);}
    platform3g::CursorHandle ReadCursor()override{return real.ReadCursor();}
    platform3g::CursorHandle ExchangeCursor(platform3g::CursorHandle)override{throw platform3g::Unsupported("cursor denied by backend");}
};
}
void VerifyBootstrapCursor(NativeInitRuntime& runtime,platform3g::Window& window,platform3g::Device& device,bool negotiated) {
    assert(g_MatrixHeap && g_MatrixHeap!=g_CacheHeap && !runtime.Cursor());
    if(!negotiated) {
        bool rejected=false;try{runtime.InitializeCursor();}catch(const platform3g::Unsupported&){rejected=true;}
        assert(rejected && !runtime.Cursor() && runtime.GameDefaultsReady());
        rejected=false;try{CMatrixCursor missing_window;}catch(const platform3g::Unsupported&){rejected=true;}
        assert(rejected); // Actual constructor unwinds a valid wstring, not memset memory.
        std::puts("original cursor constructor/window prefix Unsupported; CPU game heap preserved");return;
    }
    window.Show(platform3g::Visibility::Normal);window.Update();
    const auto borrowed=window.ReadCursor();assert(borrowed.value!=0);
    runtime.DetachDevice();RefusingCursorWindow denied(window);
    runtime.AttachDevice(denied,device);
    bool rejected=false;try{runtime.InitializeCursor();}catch(const platform3g::Unsupported&){rejected=true;}
    assert(rejected && !runtime.Cursor() && runtime.GpuReady() && runtime.GameDefaultsReady());
    assert(window.ReadCursor()==borrowed);
    runtime.DetachDevice();runtime.AttachDevice(window,device);runtime.InitializeCursor();
    assert(runtime.Cursor() && window.ReadCursor().value==0);
    assert(runtime.Cursor()->GetPosX()==0 && runtime.Cursor()->GetPosY()==0);
    runtime.Cursor()->SetPos(37,-11);
    assert(runtime.Cursor()->GetPosX()==37 && runtime.Cursor()->GetPosY()==-11);
    bool duplicate=false;try{runtime.InitializeCursor();}catch(const std::logic_error&){duplicate=true;}
    assert(duplicate && runtime.Cursor() && window.ReadCursor().value==0);
    runtime.Cursor()->Clear();runtime.Cursor()->Clear();assert(window.ReadCursor()==borrowed);
    runtime.DetachDevice();assert(!runtime.Cursor() && window.ReadCursor()==borrowed && g_MatrixHeap);
    runtime.AttachDevice(window,device);runtime.InitializeCursor();assert(window.ReadCursor().value==0);
    std::puts("original cursor ctor/Clear EXECUTED through actual SetCursor/GetCursor; refusal, repeated Clear and detach/reentry restore PASS; icon/texture selection NOT RUN");
}
void VerifyBootstrapGameDefaults(NativeInitRuntime& runtime) {
    assert(runtime.CpuReady() && !runtime.GameDefaultsReady());
    const auto was_gpu=runtime.GpuReady();
    // A refused new configuration must leave existing real allocated labels
    // untouched, and must not publish ownership to a failed initializer.
    auto* foreign=static_cast<SStringPair*>(HAlloc(sizeof(SStringPair),g_CacheHeap));
    new(foreign)SStringPair{};foreign->key=L"existing cursor";foreign->val=L"existing value";
    g_Config.m_Cursors=foreign;g_Config.m_CursorsCnt=1;
    bool rejected=false;try{runtime.InitializeGameDefaults();}catch(const std::logic_error&){rejected=true;}
    assert(rejected && !runtime.GameDefaultsReady() && g_Config.m_Cursors==foreign && foreign->key==L"existing cursor");
    ClearOriginalGameDefaults();
    runtime.InitializeGameDefaults();
    assert(runtime.GameDefaultsReady() && runtime.GpuReady()==was_gpu && g_MaxFPS==1000);
    assert(!g_Config.m_SoftwareCursor && g_Config.m_ObjTexturesGloss && g_Config.m_LandTexturesGloss);
    assert(g_Config.m_ShowStencilShadows && g_Config.m_ShowProjShadows && g_Config.m_VertexLight);
    assert(g_Config.m_CamParams[CAMERA_STRATEGY].m_CamDistMin==70 && g_Config.m_CamParams[CAMERA_STRATEGY].m_CamDistMax==250);
    assert(g_Config.m_CamParams[CAMERA_STRATEGY].m_CamAngleParam==0.4f && g_Config.m_CamParams[CAMERA_INROBOT].m_CamHeight==40);
    assert(g_Config.m_KeyActions[KA_ROTATE_LEFT]==0x24 && g_Config.m_KeyActions[KA_UNIT_FORWARD]==0x57);
    assert(g_Config.m_CaptureTimeErase==750 && g_Config.m_CaptureTimePaint==500 && g_Config.m_CaptureTimeRolback==1500);
    bool duplicate=false;try{runtime.InitializeGameDefaults();}catch(const std::logic_error&){duplicate=true;}
    assert(duplicate && runtime.GameDefaultsReady());
    // Real strings allocated on the real cache heap must be destroyed by the
    // original Clear during the shared owner's Deinit, not by this test.
    g_Config.m_Cursors=static_cast<SStringPair*>(HAlloc(2*sizeof(SStringPair),g_CacheHeap));
    for(int i=0;i<2;++i)new(g_Config.m_Cursors+i)SStringPair{};
    g_Config.m_CursorsCnt=2;g_Config.m_Cursors[0].key=L"first";g_Config.m_Cursors[1].val=L"second";
    auto& context=buffers::Current();const auto before=context.AllocatedBytes();
    KonusCleanupAccess::StaticInit();assert(KonusCleanupAccess::Empty());
    auto* resource=context.CreateVertex(96,vertex_layout::Position|vertex_layout::Texture1,buffers::Pool::Default,true);
    assert(resource && context.AllocatedBytes()==before+96);
    std::uint32_t* mapped=nullptr;NATIVE_BUF_LOCK_DISCARD(resource,&mapped);mapped[0]=0x12345678;NATIVE_BUF_UNLOCK(resource);
    KonusCleanupAccess::SetBuffer(resource);KonusCleanupAccess::MarkAllBuffersNoNeed();
    assert(KonusCleanupAccess::Empty() && context.AllocatedBytes()==before);
    KonusCleanupAccess::MarkAllBuffersNoNeed();assert(context.AllocatedBytes()==before);
    std::puts("original CMatrixConfig::SetDefaults EXECUTED + ownership refusal/retry; original Konus cleanup released real staging bytes; Clear pending owner Deinit");
    VerifyBootstrapMapCamera(runtime);
}
void VerifyBootstrapProjection() {
    D3DXMATRIX world,view,projection;
    D3DXMatrixIdentity(&world);D3DXMatrixIdentity(&view);
    world._41=.25f;world._42=-.5f;world._43=2.f;
    view._41=-1.f;view._42=2.f;view._43=3.f;
    constexpr float fov=1.0471975512f,aspect=1280.f/720.f;
    D3DXMatrixPerspectiveFovLH(&projection,fov,aspect,1.f,100.f);
    const Viewport viewport{13,17,1280,720,.1f,.9f};
    const D3DXVECTOR3 point{2,1,10};D3DXVECTOR3 screen;
    D3DXVec3Project(&screen,&point,&viewport,&projection,&view,&world);
    const float focal=1.f/std::tan(fov/2);
    const float expected_x=13+(1+focal/aspect*1.25f/15.f)*640;
    const float expected_y=17+(1-focal*2.5f/15.f)*360;
    const float expected_z=.1f+.8f*(100.f/99.f-100.f/(99.f*15.f));
    assert(std::fabs(screen.x-expected_x)<1e-4f && std::fabs(screen.y-expected_y)<1e-4f && std::fabs(screen.z-expected_z)<1e-6f);
    const auto combined=world*view*projection;
    auto transformed=point;D3DXVec3TransformCoord(&transformed,&transformed,&combined);
    assert(std::fabs((transformed.x+1)*640+13-screen.x)<1e-4f && std::fabs(.1f+.8f*transformed.z-screen.z)<1e-6f);
    std::puts("full original Camera header COMPILED; DirectXMath world/view/projection + aliased transform regression PASS; camera object NOT CONSTRUCTED");
}
