#include "simple_bitmap_cpu.hpp"

#include "image_cpu.hpp"
#include "software_compositor.hpp"

#include <algorithm>

namespace srhd_awa::platform::simple_bitmap_cpu {
namespace {
bool Fail(std::string* error,const char* text){if(error)*error=text;return false;}
}
void SimpleBitmap::Clear(){width_=height_=pitch_=0;source_rgba_=false;pixels_.clear();}
bool SimpleBitmap::Load(const std::uint8_t* source,std::int32_t source_size,bool source_rgba,std::string* error){Clear();image_cpu::Image image;if(!image_cpu::Decode(source,source_size,source_rgba?image_cpu::Format::BGRA8888:image_cpu::Format::RGB565,&image,error))return false;width_=image.width;height_=image.height;pitch_=image.pitch;source_rgba_=source_rgba;pixels_=std::move(image.pixels);return true;}
bool SimpleBitmap::Draw(std::uint16_t* dst,std::int32_t dw,std::int32_t dh,std::int32_t dp,std::int32_t x,std::int32_t y,bool half,const okgf_rle_bridge::Rect& clip,std::string* error)const{if(!loaded()||!dst||dw<=0||dh<=0||dp<dw||clip.left<0||clip.top<0||clip.right<clip.left||clip.bottom<clip.top||clip.right>dw||clip.bottom>dh)return Fail(error,"invalid simple bitmap draw");if(source_rgba_){const software_compositor::Rect c{clip.left,clip.top,clip.right,clip.bottom};return software_compositor::CompositeBGRA(dst,dw,dh,dp,pixels_.data(),width_,height_,pitch_,x,y,software_compositor::BlendMode::Alpha,&c,error);}const auto left=std::max(x,clip.left),top=std::max(y,clip.top),right=std::min(x+width_,clip.right),bottom=std::min(y+height_,clip.bottom);for(std::int32_t py=top;py<bottom;++py)for(std::int32_t px=left;px<right;++px){const auto src=static_cast<std::uint16_t>(pixels_[(static_cast<std::size_t>(py-y)*pitch_+(px-x)*2)])|static_cast<std::uint16_t>(pixels_[(static_cast<std::size_t>(py-y)*pitch_+(px-x)*2)+1])<<8;auto& out=dst[py*dp+px];out=half?static_cast<std::uint16_t>(((src>>1)&0x7bef)+((out>>1)&0x7bef)):src;}return true;}
}  // namespace srhd_awa::platform::simple_bitmap_cpu
