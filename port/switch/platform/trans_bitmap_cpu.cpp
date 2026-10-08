#include "trans_bitmap_cpu.hpp"

#include "alpha_bitmap_cpu.hpp"
#include "image_cpu.hpp"

#include <charconv>
#include <limits>
#include <new>

namespace srhd_awa::platform::trans_bitmap_cpu {
namespace {
bool Fail(std::string* error, const char* text) { if (error) *error = text; return false; }
bool RotateLeft(std::vector<std::uint8_t>* pixels, std::int32_t* width, std::int32_t* height) {
  const auto old_w=*width, old_h=*height;
  if (old_w<=0||old_h<=0) return false;
  std::vector<std::uint8_t> out(static_cast<std::size_t>(old_w)*old_h*2);
  for (std::int32_t y=0;y<old_h;++y) for (std::int32_t x=0;x<old_w;++x) {
    const auto dx=y, dy=old_w-1-x;
    out[(static_cast<std::size_t>(dy)*old_h+dx)*2]=(*pixels)[(static_cast<std::size_t>(y)*old_w+x)*2];
    out[(static_cast<std::size_t>(dy)*old_h+dx)*2+1]=(*pixels)[(static_cast<std::size_t>(y)*old_w+x)*2+1];
  }
  *pixels=std::move(out); *width=old_h; *height=old_w; return true;
}
bool Stretch(std::vector<std::uint8_t>* pixels,std::int32_t* width,std::int32_t* height,std::int32_t w,std::int32_t h) {
  if(w<=0||h<=0||*width<=0||*height<=0) return false;
  if(w==*width&&h==*height)return true;
  if(static_cast<std::uint64_t>(w)*h*2>256ull*1024*1024)return false;
  std::vector<std::uint8_t> out(static_cast<std::size_t>(w)*h*2);
  for(std::int32_t y=0;y<h;++y)for(std::int32_t x=0;x<w;++x){const auto sx=static_cast<std::int32_t>((static_cast<std::int64_t>(x)* *width)/w),sy=static_cast<std::int32_t>((static_cast<std::int64_t>(y)* *height)/h);out[(static_cast<std::size_t>(y)*w+x)*2]=(*pixels)[(static_cast<std::size_t>(sy)* *width+sx)*2];out[(static_cast<std::size_t>(y)*w+x)*2+1]=(*pixels)[(static_cast<std::size_t>(sy)* *width+sx)*2+1];}
  *pixels=std::move(out);*width=w;*height=h;return true;
}
bool Number(const std::string& s,std::int32_t* n){const auto r=std::from_chars(s.data(),s.data()+s.size(),*n);return r.ec==std::errc{}&&r.ptr==s.data()+s.size();}
bool Apply(std::vector<std::uint8_t>* pixels,std::int32_t* width,std::int32_t* height,const std::string& ops,std::string* error){std::size_t at=0;while(at<=ops.size()){const auto end=ops.find('&',at);const auto part=ops.substr(at,end==std::string::npos?std::string::npos:end-at);if(part=="270"){if(!RotateLeft(pixels,width,height))return Fail(error,"invalid rotate image");}else if(part.rfind("Stretch=",0)==0){const auto comma=part.find(',',8);std::int32_t w{},h{};if(comma==std::string::npos||!Number(part.substr(8,comma-8),&w)||!Number(part.substr(comma+1),&h)||!Stretch(pixels,width,height,w,h))return Fail(error,"invalid Stretch operation");}else if(!part.empty()){/* upstream ignores unknown tokens. */}if(end==std::string::npos)break;at=end+1;}return true;}
}
void TransBitmap::Clear(){width_=height_=0;rle_.clear();}
bool TransBitmap::Load(const std::uint8_t* source,std::int32_t source_size,const std::string& operations,std::string* error){Clear();image_cpu::Image image;if(!image_cpu::Decode(source,source_size,image_cpu::Format::RGB565,&image,error))return false;auto pixels=std::move(image.pixels);auto w=image.width,h=image.height;if(!Apply(&pixels,&w,&h,operations,error))return false;const auto n=okgf_rle_bridge::BuildTransBufWord(pixels.data(),w*2,w,h,nullptr,0);if(n<=0||static_cast<std::uint64_t>(n)>256ull*1024*1024)return Fail(error,"transparent RLE build failed");try{rle_.resize(n);}catch(const std::bad_alloc&){return Fail(error,"transparent RLE allocation failed");}if(okgf_rle_bridge::BuildTransBufWord(pixels.data(),w*2,w,h,rle_.data(),0)!=n||!alpha_bitmap_cpu::ValidateRle(rle_.data(),rle_.size(),w,h,2)){Clear();return Fail(error,"invalid transparent RLE");}width_=w;height_=h;return true;}
bool TransBitmap::Draw(std::uint16_t* dst,std::int32_t dw,std::int32_t dh,std::int32_t pitch,std::int32_t x,std::int32_t y,bool half,const okgf_rle_bridge::Rect& clip,std::string* error)const{if(!loaded()||!dst||dw<=0||dh<=0||pitch<dw||clip.left<0||clip.top<0||clip.right<clip.left||clip.bottom<clip.top||clip.right>dw||clip.bottom>dh)return Fail(error,"invalid transparent draw");if(half)okgf_rle_bridge::DrawTransBufHalf565Clip(dst,pitch*2,x,y,rle_.data(),clip);else okgf_rle_bridge::DrawTransBuf565Clip(dst,pitch*2,x,y,rle_.data(),clip);return true;}
}  // namespace srhd_awa::platform::trans_bitmap_cpu
