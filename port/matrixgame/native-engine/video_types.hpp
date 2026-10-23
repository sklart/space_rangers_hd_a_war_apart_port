// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
namespace srhd_awa::matrixgame::platform3g {
// Native value contract; SDK providers translate explicitly, without SDK aliases.
enum class PixelFormat:std::uint32_t {Unknown=0,Argb8=21,Xrgb8=22,Rgb565=23,Depth24Stencil8=75,Depth24=77,Depth16=80};
struct DisplayMode {std::uint32_t Width{},Height{},RefreshRate{};PixelFormat Format{};};
struct CreationParameters {std::uint32_t AdapterOrdinal{},DeviceType{},BehaviorFlags{};void* FocusWindow{};};
struct WindowRect {std::int32_t left{},top{},right{},bottom{};bool operator==(const WindowRect&)const=default;};
struct Presentation {
    std::uint32_t BackBufferWidth{},BackBufferHeight{};PixelFormat BackBufferFormat{};
    std::uint32_t BackBufferCount{},MultiSampleType{},MultiSampleQuality{},SwapEffect{};
    void* hDeviceWindow{};bool Windowed{},EnableAutoDepthStencil{};
    PixelFormat AutoDepthStencilFormat{};
    std::uint32_t Flags{},FullScreen_RefreshRateInHz{},PresentationInterval{};
    bool operator==(const Presentation&)const=default;
};
struct VideoViewport {std::uint32_t X{},Y{},Width{},Height{};float MinZ{},MaxZ{};};
struct LightVector {float x{},y{},z{};};
struct LightColor {float r{},g{},b{},a{};};
struct VideoMaterial {LightColor Diffuse{},Ambient{},Specular{},Emissive{};float Power{};};
struct VideoLight {
    std::uint32_t Type{};LightColor Diffuse{},Specular{},Ambient{};
    LightVector Position{},Direction{};
    float Range{},Falloff{},Attenuation0{},Attenuation1{},Attenuation2{},Theta{},Phi{};
};
}
