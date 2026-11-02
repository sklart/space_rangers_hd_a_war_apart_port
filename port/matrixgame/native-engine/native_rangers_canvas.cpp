// SPDX-License-Identifier: GPL-2.0-or-later
#include "native_rangers_canvas.hpp"
#include "rangers_canvas_pixels.hpp"
#include "platform_switch.hpp"
#include "native_texture.hpp"
#include "native_state_layout.hpp"
#include "native_diagnostic_journal.hpp"
#include "../../switch/platform/rgb565_presentation.hpp"
namespace srhd_awa::matrixgame {
namespace p=platform3g;
namespace renderer=srhd_awa::platform::renderer_platform;
struct NativeRangersCanvas::Impl final:renderer::Rgb565Presenter {
 p::DekoDevice& device;
 struct Texture {IDirect3DTexture9* value{};int width{},height{};~Texture(){if(value)native_texture::Release(value);}} texture;
 renderer::Rgb565PresentationScope registration;
 explicit Impl(p::DekoDevice& d):device(d),registration(*this){}
 renderer::PresentationDiagnostics Present(const void* pixels,int pitch,int width,int height)override{
  if(!pixels)throw std::invalid_argument("native Rangers canvas pixels missing");
  const auto extent=device.DrawableExtent();const auto ow=extent.right-extent.left,oh=extent.bottom-extent.top;
  const auto plan=rangers_canvas::Layout(pitch,width,height,ow,oh);
  auto state=device.CaptureState();if(!state)throw std::runtime_error("native Rangers canvas state capture returned no owner");
  struct Restore {
   p::DekoDevice& device;std::unique_ptr<p::DeviceStateLease>& state;bool done{};
   ~Restore(){if(!done){
    const auto aborted=device.AbortNativeFrame();if(aborted)diagnostic::Record("failed","Rangers.canvas.abort",{},aborted);
    if(state){const auto restored=state->Restore();if(restored)diagnostic::Record("failed","Rangers.canvas.restore",{},restored);}
   }}
  } restore{device,state};
  if(!texture.value || texture.width!=width || texture.height!=height){
   IDirect3DTexture9* next{};p::CheckDeviceResult(native_texture::Create(device.TextureSession(),width,height,1,0,23,1,&next),"Rangers RGB565 managed texture Create");
   if(texture.value)native_texture::Release(texture.value);texture.value=next;texture.width=width;texture.height=height;
  }
  legacy_texture::LockedRect lock{};p::CheckDeviceResult(native_texture::Lock(texture.value,0,&lock,nullptr,0),"Rangers RGB565 Lock");
  // Lock always supplies a tight native allocation. Check before touching it.
  try{
   if(!lock.pBits || lock.Pitch<width*2)throw std::runtime_error("Rangers RGB565 invalid native texture lock");
   rangers_canvas::CopyRows(plan,{static_cast<const std::byte*>(pixels),plan.source_bytes},pitch,
    {static_cast<std::byte*>(lock.pBits),std::size_t(lock.Pitch)*height},lock.Pitch);
  }catch(...){native_texture::Unlock(texture.value,0);throw;}
  p::CheckDeviceResult(native_texture::Unlock(texture.value,0),"Rangers RGB565 Unlock");native_texture::Preload(texture.value);
  for(const auto& word:p::NativeRenderWords)p::CheckDeviceResult(device.SetRenderState(static_cast<RasterRegister>(word.id),word.initial),"Rangers canvas default raster state");
  for(const auto& word:p::NativeSamplerWords)p::CheckDeviceResult(device.SetSamplerState(0,static_cast<SamplerRegister>(word.id),word.initial),"Rangers canvas default sampler state");
  p::CheckDeviceResult(device.SetFixedVertexLayout(0x144),"Rangers canvas XYZRHW diffuse tex1");
  for(const auto [reg,value]:std::array<std::pair<unsigned,unsigned>,8>{{{7,0},{14,0},{22,1},{27,0},{28,0},{137,0},{141,0},{52,0}}})
   p::CheckDeviceResult(device.SetRenderState(static_cast<RasterRegister>(reg),value),"Rangers canvas raster state");
  for(const auto [reg,value]:std::array<std::pair<unsigned,unsigned>,7>{{{1,2},{2,2},{4,2},{5,2},{11,0},{24,0},{28,1}}})
   p::CheckDeviceResult(device.SetTextureStageState(0,static_cast<p::TextureStageRegister>(reg),value),"Rangers canvas texture select");
  p::CheckDeviceResult(device.SetTextureStageState(1,static_cast<p::TextureStageRegister>(1),1),"Rangers canvas disable stage1");
  for(const auto [reg,value]:std::array<std::pair<unsigned,unsigned>,5>{{{1,3},{2,3},{5,1},{6,1},{7,0}}})
   p::CheckDeviceResult(device.SetSamplerState(0,static_cast<SamplerRegister>(reg),value),"Rangers canvas clamp/point sampler");
  p::CheckDeviceResult(device.BindBorrowedTexture(0,texture.value),"Rangers canvas bind RGB565");
  device.BeginNativeFrame(0xff000000);p::CheckDeviceResult(device.NativeBeginScene(),"Rangers canvas BeginScene");
  p::CheckDeviceResult(device.SetVideoViewport({0,0,std::uint32_t(ow),std::uint32_t(oh),0,1}),"Rangers canvas full viewport");
  const float left=float(plan.x)-.5f,top=float(plan.y)-.5f,right=float(plan.x+plan.draw_width)-.5f,bottom=float(plan.y+plan.draw_height)-.5f;
  const std::array<rangers_canvas::Vertex,4> vertices{{{left,top,0,1,0xffffffff,0,0},{left,bottom,0,1,0xffffffff,0,1},{right,top,0,1,0xffffffff,1,0},{right,bottom,0,1,0xffffffff,1,1}}};
  p::CheckDeviceResult(device.NativeDrawUP(5,2,std::as_bytes(std::span{vertices}),sizeof(rangers_canvas::Vertex)),"Rangers canvas draw");
  p::CheckDeviceResult(device.NativeEndScene(),"Rangers canvas EndScene");device.PresentNativeFrame();
  p::CheckDeviceResult(state->Restore(),"Rangers canvas restore caller state");state.reset();restore.done=true;
  return {true,true,true,ow,oh,plan.x,plan.y,plan.draw_width,plan.draw_height};
 }
};
NativeRangersCanvas::NativeRangersCanvas(p::DekoDevice& device):impl_(std::make_unique<Impl>(device)){}
NativeRangersCanvas::~NativeRangersCanvas()=default;
}
