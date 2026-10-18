// SPDX-License-Identifier: GPL-2.0-or-later
#include "engine_callbacks.hpp"
#include "config_unicode.hpp"
#include <mutex>
#include <stdexcept>
namespace srhd_awa::matrixgame {
struct EngineCallbackContext { RangersBridge bridge; explicit EngineCallbackContext(const Robot::TRobotCallbacks& c):bridge(c){} };
namespace {
std::mutex binding_mutex;
std::weak_ptr<EngineCallbackContext> active;
auto Context(){std::lock_guard lock(binding_mutex);auto value=active.lock();if(!value)throw std::logic_error("MatrixGame callbacks are not bound");return value;}
std::u16string Wide(const wchar_t* value){if(!value)throw std::invalid_argument("null MatrixGame string");return ::matrixgame::native::Utf16Units(value);}
void Sound(wchar_t* path){auto c=Context();c->bridge.PlaySound(Wide(path));}
std::uint32_t Create(wchar_t* path,int group,int loop){auto c=Context();return c->bridge.CreateSound(Wide(path),group,loop!=0).value;}
void Destroy(std::uint32_t id){auto c=Context();c->bridge.FreeSound({id});}
void Play(std::uint32_t id){auto c=Context();c->bridge.StartSound({id});}
int IsPlay(std::uint32_t id){auto c=Context();return c->bridge.IsSoundPlaying({id});}
void Volume(std::uint32_t id,float v){auto c=Context();c->bridge.SetSoundVolume({id},v);}
void Pan(std::uint32_t id,float v){auto c=Context();c->bridge.SetSoundPan({id},v);}
float GetVolume(std::uint32_t id){auto c=Context();return c->bridge.GetSoundVolume({id});}
float GetPan(std::uint32_t id){auto c=Context();return c->bridge.GetSoundPan({id});}
void Text(wchar_t* text,wchar_t* font,std::uint32_t color,int sx,int sy,int ax,int ay,int wrap,int ox,int oy,Base::CRect* clip,SMGDRangersInterfaceText* out){
 if(!out)throw std::invalid_argument("null MatrixGame image output");*out={};
 auto c=Context();auto t=Wide(text),f=Wide(font);TextRequest request{};
 request.text=t;request.font=f;request.color=color;request.width=sx;request.height=sy;request.align_x=ax;request.align_y=ay;request.wrap=wrap;request.offset_x=ox;request.offset_y=oy;request.has_clip=clip!=nullptr;
 if(clip)request.clip={clip->left,clip->top,clip->right,clip->bottom};
 auto image=c->bridge.RenderText(request);
 if(image.pitch<0||image.width<0||image.height<0){c->bridge.FreeText(image.id);throw std::range_error("negative MatrixGame raster extent");}
 *out={image.id.value,static_cast<std::uint8_t*>(image.pixels),std::uint32_t(image.pitch),image.width,image.height};
}
void Clear(SMGDRangersInterfaceText* image){if(!image)throw std::invalid_argument("null MatrixGame image");if(image->m_Image){auto c=Context();c->bridge.FreeText({image->m_Image});}*image={};}
void Progress(float value){auto c=Context();c->bridge.SetProgress(value);}
void Takt(){auto c=Context();c->bridge.PlayMusic();}
void Begin(){auto c=Context();c->bridge.ReleaseTextures();}
float MusicGet(){auto c=Context();return c->bridge.GetMusicVolume();}
void MusicSet(float value){auto c=Context();c->bridge.SetMusicVolume(value);}
SMGDRangersInterface table{Sound,Create,Destroy,Play,IsPlay,Volume,Pan,GetVolume,GetPan,Text,Clear,Progress,Takt,Begin,MusicGet,MusicSet};
static_assert(sizeof(SMGDRangersInterface)==16*sizeof(void(*)()));
static_assert(sizeof(SRobotGameState)==6*sizeof(std::int32_t));
}
EngineCallbacks::EngineCallbacks(const Robot::TRobotCallbacks& callbacks){std::lock_guard lock(binding_mutex);if(!active.expired())throw std::logic_error("MatrixGame callbacks already bound");context_=std::make_shared<EngineCallbackContext>(callbacks);active=context_;}
EngineCallbacks::~EngineCallbacks(){std::lock_guard lock(binding_mutex);if(active.lock()==context_)active.reset();}
SMGDRangersInterface* EngineCallbacks::Table() noexcept{return &table;}
SRobotsSettings ToEngineSettings(const DisplaySettings& v){SRobotsSettings out{};
 out.FDirect3D=v.direct3d;out.FD3DDevice=v.device;out.m_ShowStencilShadows=v.stencil_shadows;out.m_ShowProjShadows=v.projected_shadows;out.m_IzvratMS=v.selection_ex;out.m_LandTexturesGloss=v.terrain_gloss;out.m_ObjTexturesGloss=v.object_gloss;out.m_SoftwareCursor=v.software_cursor;out.m_SkyBox=v.sky;out.m_RobotShadow=v.robot_shadow;
 out.m_BPP=v.color_depth;out.m_ResolutionX=v.width;out.m_ResolutionY=v.height;out.m_RefreshRate=v.refresh_rate;out.m_Brightness=v.brightness;out.m_Contrast=v.contrast;out.m_FSAASamples=v.fsaa_samples;out.m_AFDegree=v.anisotropy;out.m_MaxDistance=v.max_distance;out.m_VSync=v.vsync;return out;
}
BattleStatistics ReadEngineStatistics(const SRobotGameState& v){return {v.m_Time,v.m_BuildRobot,v.m_KillRobot,v.m_BuildTurret,v.m_KillTurret,v.m_KillBuilding};}
}
