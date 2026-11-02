// SPDX-License-Identifier: GPL-2.0-or-later
#include "native_screenshot.hpp"
#include "platform_3g.hpp"
#include "output_paths_backend.hpp"
#include "MatrixGamePathUtils.hpp"
#include "CBitmap.hpp"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <limits>
namespace srhd_awa::matrixgame::native_form {
void ValidateScreenshotPixels(const ScreenshotPixels& image){
    const unsigned bpp=image.format==ScreenshotFormat::Rgba8?4:image.format==ScreenshotFormat::Rgb565?2:0;
    if(!bpp || !image.width || !image.height || image.width>16384 || image.height>16384 ||
       image.pitch<std::uint64_t(image.width)*bpp || std::uint64_t(image.pitch)*image.height>256*1024*1024 ||
       image.bytes.size()!=std::uint64_t(image.pitch)*image.height)
        throw std::invalid_argument("screenshot pixel extent/format/pitch/bytes");
}
void CopyScreenshotPixels(CBitmap& bitmap,const ScreenshotPixels& image){
    ValidateScreenshotPixels(image);
    const unsigned bpp=image.format==ScreenshotFormat::Rgba8?4:2;
    bitmap.CreateRGB(int(image.width),int(image.height));
    for(unsigned y=0;y<image.height;++y){
        auto* destination=bitmap.Data()+std::size_t(y)*bitmap.Pitch();
        const auto* source=image.bytes.data()+std::size_t(y)*image.pitch;
        for(unsigned x=0;x<image.width;++x){
            if(bpp==4){for(unsigned c=0;c<3;++c)destination[x*3+c]=std::to_integer<unsigned char>(source[x*4+c]);}
            else {
                const unsigned packed=std::to_integer<unsigned char>(source[x*2])|(unsigned(std::to_integer<unsigned char>(source[x*2+1]))<<8);
                const unsigned r=(packed>>11)&31,g=(packed>>5)&63,b=packed&31;
                destination[x*3]=(r<<3)|(r>>2);destination[x*3+1]=(g<<2)|(g>>4);destination[x*3+2]=(b<<3)|(b>>2);
            }
        }
    }
}
std::wstring SaveScreenshot(const ScreenshotPixels& image){
    if(!output_paths::NativeBound())throw platform3g::Unsupported("screenshot requires private native output scope");
    CBitmap bitmap;CopyScreenshotPixels(bitmap,image);
    const auto folder=PathToOutputFiles(L"Screenshots");
    output_paths::CreateFolder(folder.c_str());
    const auto directory=output_paths::FilesystemPath(folder);
    if(!std::filesystem::is_directory(directory))throw std::runtime_error("screenshot output directory unavailable");
    static std::atomic<std::uint64_t> sequence{};
    const auto stamp=std::chrono::system_clock::now().time_since_epoch().count();
    // Never delete/replace an existing screenshot. A clock rollback/restart can
    // only cause another candidate to be tried, not overwrite the old file.
    for(unsigned attempt=0;attempt<1024;++attempt){
        const auto id=sequence.fetch_add(1,std::memory_order_relaxed);
        const auto name=folder+L"/Shot-"+std::to_wstring(stamp)+L"-"+std::to_wstring(id)+L".png";
        if(std::filesystem::exists(output_paths::FilesystemPath(name)))continue;
        if(!bitmap.SaveInPNG(name))throw std::runtime_error("original CBitmap screenshot PNG encoding failed");
        return name;
    }
    throw std::runtime_error("screenshot filename collision limit");
}
std::wstring Screenshot(){
    if(!output_paths::NativeBound())throw platform3g::Unsupported("screenshot requires private native output scope");
    auto* provider=dynamic_cast<ScreenshotProvider*>(&platform3g::ReadyDevice());
    if(!provider)throw platform3g::Unsupported("native screenshot capture provider unavailable");
    return SaveScreenshot(provider->CaptureScreenshot());
}
}
