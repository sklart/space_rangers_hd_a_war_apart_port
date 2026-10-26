// SPDX-License-Identifier: GPL-2.0-or-later
#include "native_stored_state.hpp"
#include "native_state_layout.hpp"
namespace srhd_awa::matrixgame::platform3g {
// A changed original enum must fail compilation rather than remap state silently.
static_assert(unsigned(StoredRenderRegister::ZENABLE)==0);
static_assert(unsigned(StoredRenderRegister::FILLMODE)==1);
static_assert(unsigned(StoredRenderRegister::SHADEMODE)==2);
static_assert(unsigned(StoredRenderRegister::ZWRITEENABLE)==3);
static_assert(unsigned(StoredRenderRegister::ALPHATESTENABLE)==4);
static_assert(unsigned(StoredRenderRegister::LASTPIXEL)==5);
static_assert(unsigned(StoredRenderRegister::SRCBLEND)==6);
static_assert(unsigned(StoredRenderRegister::DESTBLEND)==7);
static_assert(unsigned(StoredRenderRegister::CULLMODE)==8);
static_assert(unsigned(StoredRenderRegister::ZFUNC)==9);
static_assert(unsigned(StoredRenderRegister::ALPHAREF)==10);
static_assert(unsigned(StoredRenderRegister::ALPHAFUNC)==11);
static_assert(unsigned(StoredRenderRegister::DITHERENABLE)==12);
static_assert(unsigned(StoredRenderRegister::ALPHABLENDENABLE)==13);
static_assert(unsigned(StoredRenderRegister::FOGENABLE)==14);
static_assert(unsigned(StoredRenderRegister::SPECULARENABLE)==15);
static_assert(unsigned(StoredRenderRegister::FOGCOLOR)==16);
static_assert(unsigned(StoredRenderRegister::FOGTABLEMODE)==17);
static_assert(unsigned(StoredRenderRegister::FOGSTART)==18);
static_assert(unsigned(StoredRenderRegister::FOGEND)==19);
static_assert(unsigned(StoredRenderRegister::FOGDENSITY)==20);
static_assert(unsigned(StoredRenderRegister::RANGEFOGENABLE)==21);
static_assert(unsigned(StoredRenderRegister::STENCILENABLE)==22);
static_assert(unsigned(StoredRenderRegister::STENCILFAIL)==23);
static_assert(unsigned(StoredRenderRegister::STENCILZFAIL)==24);
static_assert(unsigned(StoredRenderRegister::STENCILPASS)==25);
static_assert(unsigned(StoredRenderRegister::STENCILFUNC)==26);
static_assert(unsigned(StoredRenderRegister::STENCILREF)==27);
static_assert(unsigned(StoredRenderRegister::STENCILMASK)==28);
static_assert(unsigned(StoredRenderRegister::STENCILWRITEMASK)==29);
static_assert(unsigned(StoredRenderRegister::TEXTUREFACTOR)==30);
static_assert(unsigned(StoredRenderRegister::WRAP0)==31);
static_assert(unsigned(StoredRenderRegister::WRAP1)==32);
static_assert(unsigned(StoredRenderRegister::WRAP2)==33);
static_assert(unsigned(StoredRenderRegister::WRAP3)==34);
static_assert(unsigned(StoredRenderRegister::WRAP4)==35);
static_assert(unsigned(StoredRenderRegister::WRAP5)==36);
static_assert(unsigned(StoredRenderRegister::WRAP6)==37);
static_assert(unsigned(StoredRenderRegister::WRAP7)==38);
static_assert(unsigned(StoredRenderRegister::CLIPPING)==39);
static_assert(unsigned(StoredRenderRegister::LIGHTING)==40);
static_assert(unsigned(StoredRenderRegister::AMBIENT)==41);
static_assert(unsigned(StoredRenderRegister::FOGVERTEXMODE)==42);
static_assert(unsigned(StoredRenderRegister::COLORVERTEX)==43);
static_assert(unsigned(StoredRenderRegister::LOCALVIEWER)==44);
static_assert(unsigned(StoredRenderRegister::NORMALIZENORMALS)==45);
static_assert(unsigned(StoredRenderRegister::DIFFUSEMATERIALSOURCE)==46);
static_assert(unsigned(StoredRenderRegister::SPECULARMATERIALSOURCE)==47);
static_assert(unsigned(StoredRenderRegister::AMBIENTMATERIALSOURCE)==48);
static_assert(unsigned(StoredRenderRegister::EMISSIVEMATERIALSOURCE)==49);
static_assert(unsigned(StoredRenderRegister::VERTEXBLEND)==50);
static_assert(unsigned(StoredRenderRegister::CLIPPLANEENABLE)==51);
static_assert(unsigned(StoredRenderRegister::POINTSIZE)==52);
static_assert(unsigned(StoredRenderRegister::POINTSIZE_MIN)==53);
static_assert(unsigned(StoredRenderRegister::POINTSPRITEENABLE)==54);
static_assert(unsigned(StoredRenderRegister::POINTSCALEENABLE)==55);
static_assert(unsigned(StoredRenderRegister::POINTSCALE_A)==56);
static_assert(unsigned(StoredRenderRegister::POINTSCALE_B)==57);
static_assert(unsigned(StoredRenderRegister::POINTSCALE_C)==58);
static_assert(unsigned(StoredRenderRegister::MULTISAMPLEANTIALIAS)==59);
static_assert(unsigned(StoredRenderRegister::MULTISAMPLEMASK)==60);
static_assert(unsigned(StoredRenderRegister::PATCHEDGESTYLE)==61);
static_assert(unsigned(StoredRenderRegister::DEBUGMONITORTOKEN)==62);
static_assert(unsigned(StoredRenderRegister::POINTSIZE_MAX)==63);
static_assert(unsigned(StoredRenderRegister::INDEXEDVERTEXBLENDENABLE)==64);
static_assert(unsigned(StoredRenderRegister::COLORWRITEENABLE)==65);
static_assert(unsigned(StoredRenderRegister::TWEENFACTOR)==66);
static_assert(unsigned(StoredRenderRegister::BLENDOP)==67);
static_assert(unsigned(StoredRenderRegister::POSITIONDEGREE)==68);
static_assert(unsigned(StoredRenderRegister::NORMALDEGREE)==69);
static_assert(unsigned(StoredRenderRegister::SCISSORTESTENABLE)==70);
static_assert(unsigned(StoredRenderRegister::SLOPESCALEDEPTHBIAS)==71);
static_assert(unsigned(StoredRenderRegister::ANTIALIASEDLINEENABLE)==72);
static_assert(unsigned(StoredRenderRegister::MINTESSELLATIONLEVEL)==73);
static_assert(unsigned(StoredRenderRegister::MAXTESSELLATIONLEVEL)==74);
static_assert(unsigned(StoredRenderRegister::ADAPTIVETESS_X)==75);
static_assert(unsigned(StoredRenderRegister::ADAPTIVETESS_Y)==76);
static_assert(unsigned(StoredRenderRegister::ADAPTIVETESS_Z)==77);
static_assert(unsigned(StoredRenderRegister::ADAPTIVETESS_W)==78);
static_assert(unsigned(StoredRenderRegister::ENABLEADAPTIVETESSELLATION)==79);
static_assert(unsigned(StoredRenderRegister::TWOSIDEDSTENCILMODE)==80);
static_assert(unsigned(StoredRenderRegister::CCW_STENCILFAIL)==81);
static_assert(unsigned(StoredRenderRegister::CCW_STENCILZFAIL)==82);
static_assert(unsigned(StoredRenderRegister::CCW_STENCILPASS)==83);
static_assert(unsigned(StoredRenderRegister::CCW_STENCILFUNC)==84);
static_assert(unsigned(StoredRenderRegister::COLORWRITEENABLE1)==85);
static_assert(unsigned(StoredRenderRegister::COLORWRITEENABLE2)==86);
static_assert(unsigned(StoredRenderRegister::COLORWRITEENABLE3)==87);
static_assert(unsigned(StoredRenderRegister::BLENDFACTOR)==88);
static_assert(unsigned(StoredRenderRegister::SRGBWRITEENABLE)==89);
static_assert(unsigned(StoredRenderRegister::DEPTHBIAS)==90);
static_assert(unsigned(StoredRenderRegister::WRAP8)==91);
static_assert(unsigned(StoredRenderRegister::WRAP9)==92);
static_assert(unsigned(StoredRenderRegister::WRAP10)==93);
static_assert(unsigned(StoredRenderRegister::WRAP11)==94);
static_assert(unsigned(StoredRenderRegister::WRAP12)==95);
static_assert(unsigned(StoredRenderRegister::WRAP13)==96);
static_assert(unsigned(StoredRenderRegister::WRAP14)==97);
static_assert(unsigned(StoredRenderRegister::WRAP15)==98);
static_assert(unsigned(StoredRenderRegister::SEPARATEALPHABLENDENABLE)==99);
static_assert(unsigned(StoredRenderRegister::SRCBLENDALPHA)==100);
static_assert(unsigned(StoredRenderRegister::DESTBLENDALPHA)==101);
static_assert(unsigned(StoredRenderRegister::BLENDOPALPHA)==102);
static_assert(unsigned(StoredSamplerRegister::ADDRESSU)==0);
static_assert(unsigned(StoredSamplerRegister::ADDRESSV)==1);
static_assert(unsigned(StoredSamplerRegister::ADDRESSW)==2);
static_assert(unsigned(StoredSamplerRegister::BORDERCOLOR)==3);
static_assert(unsigned(StoredSamplerRegister::MAGFILTER)==4);
static_assert(unsigned(StoredSamplerRegister::MINFILTER)==5);
static_assert(unsigned(StoredSamplerRegister::MIPFILTER)==6);
static_assert(unsigned(StoredSamplerRegister::MIPMAPLODBIAS)==7);
static_assert(unsigned(StoredSamplerRegister::MAXMIPLEVEL)==8);
static_assert(unsigned(StoredSamplerRegister::MAXANISOTROPY)==9);
static_assert(unsigned(StoredSamplerRegister::SRGBTEXTURE)==10);
static_assert(unsigned(StoredSamplerRegister::ELEMENTINDEX)==11);
static_assert(unsigned(StoredSamplerRegister::DMAPOFFSET)==12);
static_assert(unsigned(StoredRenderRegister::Count)==NativeRenderWords.size());
static_assert(unsigned(StoredSamplerRegister::Count)==NativeSamplerWords.size());
namespace {
LightColor Color(const std::array<float,4>& v){return {v[0],v[1],v[2],v[3]};}
std::array<float,4> Color(LightColor v){return {v.r,v.g,v.b,v.a};}
}
DeviceResult NativeStoredStateBackend::GetRenderState(StoredRenderRegister r,std::uint32_t* out){
    if(!out)throw std::invalid_argument("null stored render output");
    *out=StoredStates().Read().Render(NativeRenderWords.at(unsigned(r)).id);return DeviceSuccess;
}
DeviceResult NativeStoredStateBackend::SetRenderState(StoredRenderRegister r,std::uint32_t v){
    const auto id=NativeRenderWords.at(unsigned(r)).id;StoredStates().Edit().Render(id,v);return DeviceSuccess;
}
DeviceResult NativeStoredStateBackend::GetSamplerState(std::uint32_t slot,StoredSamplerRegister r,std::uint32_t* out){
    if(!out)throw std::invalid_argument("null stored sampler output");
    *out=StoredStates().Read().Sampler(slot,NativeSamplerWords.at(unsigned(r)).id);return DeviceSuccess;
}
DeviceResult NativeStoredStateBackend::SetSamplerState(std::uint32_t slot,StoredSamplerRegister r,std::uint32_t v){
    const auto id=NativeSamplerWords.at(unsigned(r)).id;StoredStates().Edit().Sampler(slot,id,v);return DeviceSuccess;
}
DeviceResult NativeStoredStateBackend::GetLight(std::uint32_t i,StoredLight* out){
    if(!out)throw std::invalid_argument("null stored light output");
    const auto& l=StoredStates().Read().vertex.Light(i);
    *out={l.Type,Color(l.Diffuse),Color(l.Specular),Color(l.Ambient),{l.Position.x,l.Position.y,l.Position.z},{l.Direction.x,l.Direction.y,l.Direction.z},l.Range,l.Falloff,l.Attenuation0,l.Attenuation1,l.Attenuation2,l.Theta,l.Phi};return DeviceSuccess;
}
DeviceResult NativeStoredStateBackend::GetLightEnable(std::uint32_t i,std::int32_t* out){
    if(!out)throw std::invalid_argument("null stored light enable output");
    *out=StoredStates().Read().vertex.LightEnabled(i)?1:0;return DeviceSuccess;
}
DeviceResult NativeStoredStateBackend::SetLight(std::uint32_t i,const StoredLight* l){
    if(!l)throw std::invalid_argument("null stored light input");
    VideoLight v{l->Type,Color(l->Diffuse),Color(l->Specular),Color(l->Ambient),{l->Position[0],l->Position[1],l->Position[2]},{l->Direction[0],l->Direction[1],l->Direction[2]},l->Range,l->Falloff,l->Attenuation0,l->Attenuation1,l->Attenuation2,l->Theta,l->Phi};
    StoredStates().Edit().vertex.Light(i,v);return DeviceSuccess;
}
DeviceResult NativeStoredStateBackend::LightEnable(std::uint32_t i,std::int32_t value){StoredStates().Edit().vertex.EnableLight(i,value!=0);return DeviceSuccess;}
DeviceResult NativeStoredStateBackend::GetMaterial(StoredMaterial* out){
    if(!out)throw std::invalid_argument("null stored material output");
    const auto& m=StoredStates().Read().vertex.Material();*out={Color(m.Diffuse),Color(m.Ambient),Color(m.Specular),Color(m.Emissive),m.Power};return DeviceSuccess;
}
DeviceResult NativeStoredStateBackend::SetMaterial(const StoredMaterial* m){
    if(!m)throw std::invalid_argument("null stored material input");
    const VideoMaterial v{Color(m->Diffuse),Color(m->Ambient),Color(m->Specular),Color(m->Emissive),m->Power};StoredStates().Edit().vertex.Material(v);return DeviceSuccess;
}
}
