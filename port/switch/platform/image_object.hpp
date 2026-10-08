#pragma once
#include "alpha_bitmap_cpu.hpp"
#include "image_layout.hpp"
#include "simple_bitmap_cpu.hpp"
#include "trans_bitmap_cpu.hpp"
#include <cstdint>
#include <string>
#include <variant>
namespace srhd_awa::package { class Package; }
namespace srhd_awa::platform::image_object {
enum class Kind { Simple, Trans, Alpha };
class PortableImageObject {
 public:
  explicit PortableImageObject(package::Package* package=nullptr):package_(package) {}
  PortableImageObject(const PortableImageObject&)=delete; PortableImageObject& operator=(const PortableImageObject&)=delete;
  PortableImageObject(PortableImageObject&&)=default; PortableImageObject& operator=(PortableImageObject&&)=default;
  void SetPackage(package::Package* package){package_=package;} void SetId(std::string id){id_=std::move(id);} void SetPosition(std::int32_t x,std::int32_t y){x_=x;y_=y;} void SetOrigin(std::int32_t x,std::int32_t y){origin_x_=x;origin_y_=y;} void SetSize(std::int32_t w,std::int32_t h){client_width_=w;client_height_=h;} void SetModes(image_layout::XMode x,image_layout::YMode y){x_mode_=x;y_mode_=y;} void SetHalfAlpha(bool value){half_alpha_=value;} void SetVisible(bool value){visible_=value;} void SetLayer(std::int32_t value){layer_=value;}
  bool Load(Kind kind,const std::string& resource,const std::string& load_option,std::string* error=nullptr);
  bool DrawFramebuffer(std::uint16_t* pixels,std::int32_t width,std::int32_t height,std::int32_t pitch,const okgf_rle_bridge::Rect& clip,std::string* error=nullptr)const;
  bool HitTest(std::int32_t x,std::int32_t y,std::string* error=nullptr)const;
  bool loaded()const{return !std::holds_alternative<std::monostate>(data_);} Kind kind()const{return kind_;} const std::string& resource()const{return resource_;} std::int32_t natural_width()const; std::int32_t natural_height()const;
  std::size_t resident_bytes()const;
 private:
  package::Package* package_{}; std::string id_,resource_; Kind kind_{Kind::Simple}; std::variant<std::monostate,simple_bitmap_cpu::SimpleBitmap,trans_bitmap_cpu::TransBitmap,alpha_bitmap_cpu::AlphaBitmap> data_; std::int32_t x_{},y_{},origin_x_{},origin_y_{},client_width_{},client_height_{},layer_{}; image_layout::XMode x_mode_{image_layout::XMode::Center};image_layout::YMode y_mode_{image_layout::YMode::Center};bool half_alpha_{};bool visible_{true};
};
}  // namespace srhd_awa::platform::image_object
