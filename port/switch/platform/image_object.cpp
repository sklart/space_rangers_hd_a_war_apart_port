#include "image_object.hpp"
#include "package.hpp"
#include <type_traits>
#include <limits>
#include <vector>
namespace srhd_awa::platform::image_object {namespace{bool F(std::string*e,const char*s){if(e)*e=s;return false;}}
std::int32_t PortableImageObject::natural_width()const{return std::visit([](const auto&v)->std::int32_t{using T=std::decay_t<decltype(v)>;if constexpr(std::is_same_v<T,std::monostate>)return 0;else return v.width();},data_);}std::int32_t PortableImageObject::natural_height()const{return std::visit([](const auto&v)->std::int32_t{using T=std::decay_t<decltype(v)>;if constexpr(std::is_same_v<T,std::monostate>)return 0;else return v.height();},data_);}
std::int32_t PortableImageObject::natural_origin_x()const{return kind_==Kind::GI && std::holds_alternative<gi_image_cpu::GiImage>(data_)?std::get<gi_image_cpu::GiImage>(data_).origin_x():0;}
std::int32_t PortableImageObject::natural_origin_y()const{return kind_==Kind::GI && std::holds_alternative<gi_image_cpu::GiImage>(data_)?std::get<gi_image_cpu::GiImage>(data_).origin_y():0;}
bool PortableImageObject::Load(Kind kind,const std::string& resource,const std::string& option,std::string* error){
  data_=std::monostate{};
  if(!package_)return F(error,"image object package is null");
  const auto*entry=package_->Resolve(resource);
  std::vector<std::uint8_t> bytes;
  if(!entry)return F(error,"image resource is missing");
  if(kind==Kind::GAI)return F(error,"GAI requires UI playback control");
  if(kind==Kind::GI && entry->data_size>256ull*1024*1024)return F(error,"raw GI source exceeds limit");
  if(!package_->ReadPayload(*entry,&bytes,error))return false;
  return LoadBytes(kind,bytes.data(),bytes.size(),resource,option,error);
}
bool PortableImageObject::LoadBytes(Kind kind,const std::uint8_t* bytes,std::size_t size,
                                    const std::string& resource,const std::string& option,
                                    std::string* error){
  data_=std::monostate{};
  if(kind==Kind::GAI)return F(error,"GAI requires UI playback control");
  if(!bytes||size==0||size>static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
    return F(error,"image object source bytes are invalid");
  bool ok=false;
  if(kind==Kind::Simple){
    simple_bitmap_cpu::SimpleBitmap b;
    ok=b.Load(bytes,static_cast<std::int32_t>(size),option=="RGBA",error);
    if(ok)data_=std::move(b);
  }else if(kind==Kind::Trans){
    trans_bitmap_cpu::TransBitmap b;
    ok=b.Load(bytes,static_cast<std::int32_t>(size),option,error);
    if(ok)data_=std::move(b);
  }else if(kind==Kind::GI){
    gi_image_cpu::GiImage b;
    ok=b.LoadBytes(bytes,size,error);
    if(ok)data_=std::move(b);
  }else{
    alpha_bitmap_cpu::AlphaBitmap b;
    ok=b.Load(bytes,static_cast<std::int32_t>(size),error);
    if(ok)data_=std::move(b);
  }
  if(!ok)return false;
  kind_=kind;resource_=resource;
  if(id_.empty())id_="image-object:"+resource;
  if(client_width_<=0)client_width_=natural_width();
  if(client_height_<=0)client_height_=natural_height();
  return true;
}
bool PortableImageObject::DrawFramebuffer(std::uint16_t*p,std::int32_t w,std::int32_t h,std::int32_t pitch,const okgf_rle_bridge::Rect&clip,std::string*e)const{return DrawFramebufferAt(p,w,h,pitch,x_-origin_x_,y_-origin_y_,client_width_,client_height_,clip,e);}
bool PortableImageObject::DrawFramebufferAt(std::uint16_t*p,std::int32_t w,std::int32_t h,std::int32_t pitch,std::int32_t x,std::int32_t y,std::int32_t client_width,std::int32_t client_height,const okgf_rle_bridge::Rect&clip,std::string*e)const{if(!loaded()||!p||w<=0||h<=0||pitch<w)return F(e,"invalid image object framebuffer");if(!visible_)return true;const okgf_rle_bridge::Rect bounds{x,y,x+client_width,y+client_height};if(kind_==Kind::GI)return std::get<gi_image_cpu::GiImage>(data_).Draw(p,w,h,pitch,bounds,clip,x_mode_,y_mode_,alpha_,e);const auto plan=image_layout::MakePlan(bounds,clip,natural_width(),natural_height(),x_mode_,y_mode_);if(!plan.supported)return true;for(const auto&t:plan.tiles){bool ok=std::visit([&](const auto&v){using T=std::decay_t<decltype(v)>;if constexpr(std::is_same_v<T,std::monostate>||std::is_same_v<T,gi_image_cpu::GiImage>)return false;else if constexpr(std::is_same_v<T,alpha_bitmap_cpu::AlphaBitmap>)return v.Draw(p,w,h,pitch,t.x,t.y,clip,e);else return v.Draw(p,w,h,pitch,t.x,t.y,half_alpha_,clip,e);},data_);if(!ok)return false;}return true;}
bool PortableImageObject::HitTest(std::int32_t px,std::int32_t py,std::string*e)const{if(!loaded() || !visible_)return false;const okgf_rle_bridge::Rect bounds{x_-origin_x_,y_-origin_y_,x_-origin_x_+client_width_,y_-origin_y_+client_height_};if(kind_==Kind::GI)return std::get<gi_image_cpu::GiImage>(data_).HitTestPixel(px,py,bounds,bounds,x_mode_,y_mode_,alpha_);if(kind_!=Kind::Alpha||px<bounds.left||px>=bounds.right||py<bounds.top||py>=bounds.bottom)return false;const auto plan=image_layout::MakePlan(bounds,bounds,natural_width(),natural_height(),x_mode_,y_mode_);std::uint16_t pixel{};const okgf_rle_bridge::Rect one{0,0,1,1};for(const auto&t:plan.tiles){auto&alpha=std::get<alpha_bitmap_cpu::AlphaBitmap>(data_);if(!alpha.Draw(&pixel,1,1,1,t.x-px,t.y-py,one,e))return false;if(pixel)return true;}return false;}
image_layout::Point PortableImageObject::GetVisualCenter(const okgf_rle_bridge::Rect& clip)const{if(kind_!=Kind::GI||!loaded())return {};const okgf_rle_bridge::Rect bounds{x_-origin_x_,y_-origin_y_,x_-origin_x_+client_width_,y_-origin_y_+client_height_};return std::get<gi_image_cpu::GiImage>(data_).GetVisualCenter(bounds,clip,x_mode_,y_mode_,alpha_);}
bool PortableImageObject::GetSimpleNative565(const std::uint8_t** pixels,std::size_t* bytes,std::int32_t* pitch,std::string* error)const{if(!pixels||!bytes||!pitch||kind_!=Kind::Simple||!std::holds_alternative<simple_bitmap_cpu::SimpleBitmap>(data_))return F(error,"image object is not a native Simple bitmap");const auto& simple=std::get<simple_bitmap_cpu::SimpleBitmap>(data_);if(simple.source_rgba()||!simple.loaded()||simple.pitch()!=simple.width()*2)return F(error,"Simple bitmap is not loaded native RGB565");*pixels=simple.pixels().data();*bytes=simple.pixels().size();*pitch=simple.pitch();return true;}
}  // namespace srhd_awa::platform::image_object
