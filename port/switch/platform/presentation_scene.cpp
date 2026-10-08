#include "presentation_scene.hpp"

#include "gi_object.hpp"
#include "image_object.hpp"
#include "software_compositor.hpp"

#include <algorithm>
#include <limits>

namespace srhd_awa::platform::presentation_scene {
namespace {
bool Fail(std::string* error, const char* message) { if (error) *error = message; return false; }
std::uint32_t Crc32(const std::uint8_t* bytes, std::size_t size) { std::uint32_t crc=0xffffffffu; for(std::size_t i=0;i<size;++i){crc^=bytes[i];for(unsigned bit=0;bit!=8;++bit)crc=(crc>>1)^(0xedb88320u&(0u-(crc&1u)));}return ~crc; }
std::uint64_t Fnv64(const std::uint8_t* bytes, std::size_t size) { std::uint64_t value=UINT64_C(14695981039346656037);for(std::size_t i=0;i<size;++i)value=(value^bytes[i])*UINT64_C(1099511628211);return value; }
}

bool PresentationScene::AddGI(const std::string& id, const gi_object::GIObject* object, std::string* error) {
  if (id.empty() || !object || !object->IsLoaded()) return Fail(error, "invalid GI presentation entry");
  if (std::any_of(entries_.begin(),entries_.end(),[&](const Entry& item){return item.id==id;})) return Fail(error,"presentation entry id is duplicated");
  entries_.push_back({id,object,nullptr}); return true;
}
bool PresentationScene::AddImage(const std::string& id, const image_object::PortableImageObject* object, std::string* error) {
  if (id.empty() || !object || !object->loaded()) return Fail(error, "invalid image presentation entry");
  if (std::any_of(entries_.begin(),entries_.end(),[&](const Entry& item){return item.id==id;})) return Fail(error,"presentation entry id is duplicated");
  entries_.push_back({id,nullptr,object}); return true;
}
void PresentationScene::Clear() { entries_.clear(); }
bool PresentationScene::Render(const scene_compositor::Framebuffer& target, std::string* error) const {
  if (!target.pixels || target.width<=0 || target.height<=0 || target.pitch_pixels<target.width) return Fail(error,"presentation framebuffer is invalid");
  std::vector<const Entry*> ordered; ordered.reserve(entries_.size()); for(const auto& entry:entries_) ordered.push_back(&entry);
  std::stable_sort(ordered.begin(),ordered.end(),[](const Entry* a,const Entry* b){return (a->gi?a->gi->Layer():a->image->layer()) < (b->gi?b->gi->Layer():b->image->layer());});
  const okgf_rle_bridge::Rect clip{0,0,target.width,target.height};
  for(const Entry* entry:ordered) {
    if (!(entry->gi?entry->gi->Visible():entry->image->visible())) continue;
    if (entry->image) { if(!entry->image->DrawFramebuffer(target.pixels,target.width,target.height,target.pitch_pixels,clip,error)) return false; continue; }
    const auto& image=entry->gi->Image();
    const auto mode=entry->gi->Alpha()==255 ? software_compositor::BlendMode::Opaque : software_compositor::BlendMode::Alpha;
    std::vector<std::uint8_t> alpha_pixels;
    const std::uint8_t* pixels=image.pixels.data();
    if (mode==software_compositor::BlendMode::Alpha) { alpha_pixels=image.pixels; for(std::size_t i=3;i<alpha_pixels.size();i+=4) alpha_pixels[i]=static_cast<std::uint8_t>((static_cast<std::uint16_t>(alpha_pixels[i])*entry->gi->Alpha()+127u)/255u); pixels=alpha_pixels.data(); }
    const software_compositor::Rect compositor_clip{clip.left,clip.top,clip.right,clip.bottom};
    if (!software_compositor::CompositeBGRA(target.pixels,target.width,target.height,target.pitch_pixels,pixels,image.width,image.height,image.pitch,entry->gi->X(),entry->gi->Y(),mode,&compositor_clip,error)) return false;
  }
  return true;
}
bool PresentationScene::ComputeFramebufferFingerprint(const scene_compositor::Framebuffer& target, FramebufferFingerprint* result, std::string* error) const {
  if (!result || !target.pixels || target.width<=0 || target.height<=0 || target.pitch_pixels<target.width || static_cast<std::uint64_t>(target.height)*target.pitch_pixels>std::numeric_limits<std::size_t>::max()/sizeof(std::uint16_t)) return Fail(error,"presentation fingerprint framebuffer is invalid");
  const auto bytes=static_cast<std::size_t>(target.height)*target.pitch_pixels*sizeof(std::uint16_t); const auto* raw=reinterpret_cast<const std::uint8_t*>(target.pixels); result->bytes=bytes;result->crc32=Crc32(raw,bytes);result->fnv64=Fnv64(raw,bytes);return true;
}
}  // namespace srhd_awa::platform::presentation_scene
