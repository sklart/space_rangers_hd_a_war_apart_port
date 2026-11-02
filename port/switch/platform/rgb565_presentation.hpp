// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "renderer_platform.hpp"
#include <stdexcept>
#include <thread>
namespace srhd_awa::platform::renderer_platform {
// A borrowed synchronous presenter on the UI thread. Never fall back to SDL
// after an installed native backend fails to upload, draw or present.
class Rgb565Presenter {
public:
 virtual ~Rgb565Presenter()=default;
 virtual PresentationDiagnostics Present(const void*,int pitch,int width,int height)=0;
};
namespace rgb565_detail {
inline Rgb565Presenter* presenter{};
inline std::thread::id owner{};
}
class Rgb565PresentationScope {
 Rgb565Presenter* presenter_;
public:
 explicit Rgb565PresentationScope(Rgb565Presenter& value):presenter_(&value){
  if(rgb565_detail::presenter)throw std::logic_error("RGB565 presenter already registered");
  rgb565_detail::owner=std::this_thread::get_id();rgb565_detail::presenter=presenter_;
 }
 ~Rgb565PresentationScope(){
  if(rgb565_detail::presenter!=presenter_ || rgb565_detail::owner!=std::this_thread::get_id())std::terminate();
  rgb565_detail::presenter=nullptr;rgb565_detail::owner={};
 }
 Rgb565PresentationScope(const Rgb565PresentationScope&)=delete;
 Rgb565PresentationScope& operator=(const Rgb565PresentationScope&)=delete;
};
inline Rgb565Presenter* CurrentRgb565Presenter(){
 if(rgb565_detail::presenter && rgb565_detail::owner!=std::this_thread::get_id())
  throw std::logic_error("RGB565 presentation on a foreign thread");
 return rgb565_detail::presenter;
}
}
