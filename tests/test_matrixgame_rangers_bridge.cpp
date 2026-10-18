// SPDX-License-Identifier: GPL-2.0-or-later
// Callback/ownership contract only. No renderer or battle is simulated here.
#include "rangers_bridge.hpp"
#include "types/Robot.hpp"
#include <array>
#include <cassert>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>
using namespace srhd_awa::matrixgame;
static_assert(!std::is_convertible_v<SoundId, ImageId>);
static_assert(!std::is_convertible_v<void*, SoundId>);
static_assert(sizeof(char16_t) == 2);
namespace {
struct Sound { float volume{1}, pan{}; bool playing{}; };
struct Image { std::array<unsigned char, 24> pixels{}; };
int sounds{}, images{}, calls[16]{}, render_failure{};
float music{.6f};
std::u16string observed_path;
Sound* Native(GR_Sound::TSoundBufferControl* p) { return reinterpret_cast<Sound*>(p); }
void PAS_STDCALL Play(char16_t* p) { ++calls[0]; observed_path=p; if (*p) *p=u'!'; }
GR_Sound::TSoundBufferControl* PAS_STDCALL Create(char16_t* p, std::int32_t group, std::int32_t loop) {
  ++calls[1]; observed_path=p; assert(group==7 && loop==1);
  ++sounds; return reinterpret_cast<GR_Sound::TSoundBufferControl*>(new Sound);
}
void PAS_STDCALL Free(GR_Sound::TSoundBufferControl* p) { ++calls[2]; if(p) { delete Native(p); --sounds; } }
void PAS_STDCALL Start(GR_Sound::TSoundBufferControl* p) { ++calls[3]; if(p) Native(p)->playing=true; }
std::int32_t PAS_STDCALL Playing(GR_Sound::TSoundBufferControl* p) { ++calls[4]; return p && Native(p)->playing; }
void PAS_STDCALL Volume(GR_Sound::TSoundBufferControl* p,float v) { ++calls[5]; if(p) Native(p)->volume=v; }
void PAS_STDCALL Pan(GR_Sound::TSoundBufferControl* p,float v) { ++calls[6]; if(p) Native(p)->pan=v; }
float PAS_STDCALL GetVolume(GR_Sound::TSoundBufferControl* p) { ++calls[7]; return p ? Native(p)->volume : 0; }
float PAS_STDCALL GetPan(GR_Sound::TSoundBufferControl* p) { ++calls[8]; return p ? Native(p)->pan : 0; }
void PAS_STDCALL Text(char16_t* text,char16_t* font,std::uint32_t color,std::int32_t w,std::int32_t h,
    std::int32_t ax,std::int32_t ay,std::int32_t wrap,std::int32_t ox,std::int32_t oy,
    Types::PRect clip,Robot::PRobotTextImage out) {
  ++calls[9]; assert(std::u16string_view(text)==u"Текст \U0001F680");
  assert(std::u16string_view(font)==u"Шрифт"); assert(color==0xA1B2C3D4u);
  assert(w==3 && h==2 && ax==1 && ay==2 && wrap==1 && ox==-3 && oy==4);
  assert(clip->Left==-1 && clip->Top==2 && clip->Right==3 && clip->Bottom==4);
  text[0]=u'!'; font[0]=u'!'; clip->Left=123;
  if(render_failure==1) throw std::bad_alloc();
  auto* image=new Image; ++images;
  out->Buffer=reinterpret_cast<GR_GraphBuf::TGraphBufGR*>(image);
  out->Pixels=image->pixels.data(); out->Pitch=12; out->Width=3; out->Height=2;
  if(render_failure==2) throw std::bad_alloc();
}
void PAS_STDCALL FreeText(Robot::PRobotTextImage p) {
  ++calls[10]; if(p->Buffer) { delete reinterpret_cast<Image*>(p->Buffer); --images; }
  *p={};
}
void PAS_STDCALL Progress(float v) { ++calls[11]; assert(v==.25f); }
void PAS_STDCALL Music() { ++calls[12]; }
void PAS_STDCALL Textures() { ++calls[13]; }
float PAS_STDCALL MusicGet() { ++calls[14]; return music; }
void PAS_STDCALL MusicSet(float v) { ++calls[15]; music=v; }
Robot::TRobotCallbacks Callbacks() {
  return {Play,Create,Free,Start,Playing,Volume,Pan,GetVolume,GetPan,Text,FreeText,Progress,Music,Textures,MusicGet,MusicSet};
}
template<class E,class F> void Reject(F action) {
  bool caught=false; try { action(); } catch(const E&) { caught=true; } assert(caught);
}
}
int main() {
  auto callbacks=Callbacks();
  auto incomplete=callbacks; incomplete.FreeText=nullptr;
  Reject<std::invalid_argument>([&]{RangersBridge invalid(incomplete);});
  TextRequest request{u"Текст \U0001F680",u"Шрифт",0xA1B2C3D4u,3,2,1,2,1,-3,4,{-1,2,3,4}};
  {
    RangersBridge bridge(callbacks), other(callbacks);
    std::u16string path=u"звук/\U0001F680.dat";
    bridge.PlaySound(path); assert(observed_path==path && path[0]==u'з');
    auto sound=bridge.CreateSound(path,7,true); assert(sound.value && sounds==1);
    assert(observed_path==path);
    Reject<std::out_of_range>([&]{other.StartSound(sound);});
    bridge.StartSound(sound); assert(bridge.IsSoundPlaying(sound)==1);
    bridge.SetSoundVolume(sound,.3f); bridge.SetSoundPan(sound,-.7f);
    assert(bridge.GetSoundVolume(sound)==.3f && bridge.GetSoundPan(sound)==-.7f);
    bridge.FreeSound(sound); assert(sounds==0);
    Reject<std::out_of_range>([&]{bridge.StartSound(sound);});
    Reject<std::out_of_range>([&]{bridge.FreeSound(sound);});
    bridge.FreeSound({}); bridge.StartSound({}); assert(bridge.IsSoundPlaying({})==0);
    const auto before=calls[0];
    Reject<std::invalid_argument>([&]{bridge.PlaySound(std::u16string_view(u"a\0b",3));});
    assert(calls[0]==before);
    auto image=bridge.RenderText(request); assert(image.id.value && images==1);
    assert(image.pixels && image.pitch==12 && image.width==3 && image.height==2);
    assert(request.text[0]==u'Т' && request.font[0]==u'Ш' && request.clip.left==-1);
    if constexpr(sizeof(void*)>4) assert(reinterpret_cast<std::uintptr_t>(image.pixels)>UINT32_MAX);
    Reject<std::out_of_range>([&]{other.FreeText(image.id);});
    bridge.FreeText(image.id); assert(images==0);
    Reject<std::out_of_range>([&]{bridge.FreeText(image.id);});
    bridge.FreeText({});
    for(int failure: {1,2}) {
      render_failure=failure;
      Reject<std::bad_alloc>([&]{bridge.RenderText(request);});
      assert(images==0);
    }
    render_failure=0;
    bridge.SetProgress(.25f); bridge.PlayMusic(); bridge.ReleaseTextures();
    assert(bridge.GetMusicVolume()==.6f); bridge.SetMusicVolume(.2f); assert(bridge.GetMusicVolume()==.2f);
    auto stale=bridge.CreateSound(path,7,true);
    bridge.RenderText(request); bridge.Reset(); assert(sounds==0 && images==0);
    auto fresh=bridge.CreateSound(path,7,true); assert(fresh.value!=stale.value);
    Reject<std::out_of_range>([&]{bridge.StartSound(stale);});
    // Four callers exercise table/creation/release serialization.
    std::vector<std::thread> workers;
    for(int i=0;i<4;++i) workers.emplace_back([&]{for(int j=0;j<200;++j) {
      auto id=bridge.CreateSound(u"parallel",7,true);
      bridge.SetSoundVolume(id,.3f); assert(bridge.GetSoundVolume(id)==.3f); bridge.FreeSound(id);
    }});
    for(auto& worker:workers) worker.join();
    bridge.RenderText(request); // Destructor owns both this image and 'fresh'.
  }
  assert(sounds==0 && images==0);
  for(auto count:calls) assert(count>0);
  Robot::TRobotDisplaySettingsPrefix settings{};
  int api{},device{}; settings.Direct3D=&api; settings.Device=&device;
  settings.ShowStencilShadows=1; settings.ShowProjShadows=2; settings.SelectEx=3;
  settings.LandTexturesGloss=4; settings.ObjTexturesGloss=5; settings.SoftwareCursor=6;
  settings.Sky=2; settings.RobotShadow=1; settings.ColorDepth=32;
  settings.ScreenWidth=1280; settings.ScreenHeight=720; settings.RefreshRate=60;
  settings.Brightness=.1f; settings.Contrast=.2f; settings.FSAASamples=4;
  settings.Anisotropy=8; settings.MaxDistance=.9f; settings.VSync=1;
  auto native=ReadSettings(settings);
  assert(native.direct3d==&api && native.device==&device && native.stencil_shadows && native.projected_shadows &&
    native.selection_ex && native.terrain_gloss && native.object_gloss && native.software_cursor && native.sky==2 &&
    native.robot_shadow==1 && native.color_depth==32 && native.width==1280 && native.height==720 && native.refresh_rate==60 &&
    native.brightness==.1f && native.contrast==.2f && native.fsaa_samples==4 && native.anisotropy==8 && native.max_distance==.9f && native.vsync);
  aGalaxyStruct::TPlanetBattleStatistics input{std::numeric_limits<std::int32_t>::min(),2,3,4,5,6},output{};
  auto statistics=ReadStatistics(input); WriteStatistics(statistics,output);
  assert(output.SignedTimeMs==input.SignedTimeMs && output.RobotsBuilt==2 && output.RobotsDestroyed==3 &&
    output.TurretsBuilt==4 && output.TurretsDestroyed==5 && output.BuildingsDestroyed==6);
  std::printf("native Rangers bridge PASS callbacks=16 concurrent_sound_cycles=800 resources_remaining=0 pointer_bytes=%zu battle_executed=false\n",sizeof(void*));
}
