// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "texture_asset.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace srhd_awa::matrixgame::native_texture::bitmap_filter {
// Integrated triangle footprint, with the original wrap boundary condition.
// Keep intermediate arithmetic in binary32 on both x86 and ARM64; a fused
// multiply/add changes quantization at the ordered-dither thresholds.
inline float Round(float value){volatile float rounded=value;return rounded;}
struct Weight {unsigned source;float value;};
inline auto Axis(unsigned source,unsigned destination){
    std::vector<std::vector<Weight>> result(destination);
    const float scale=float(destination)/float(source),inverse=.5f/scale;
    for(unsigned i=0;i<source;++i){
        unsigned previous=0;float accumulated=0;
        const auto flush=[&]{if(accumulated>1e-5f)result[previous].push_back({i,accumulated});};
        for(unsigned side=0;side<2;++side){
            const float center=float(i+side)-.5f;
            const float first=Round(center*scale),last=Round(first+scale);
            for(int k=int(std::floor(first));float(k)<last;++k){
                const auto target=unsigned((k+int(destination))%int(destination));
                if(target!=previous){flush();accumulated=0;previous=target;}
                const float lo=std::max(float(k),first),hi=std::min(float(k+1),last);
                const float average=Round(Round(Round(lo+hi)*inverse)-center);
                accumulated=Round(accumulated+Round((hi-lo)*(side?Round(1.f-average):average)));
            }
        }
        flush();
    }
    return result;
}
inline std::uint32_t Word(std::span<const std::byte> data,unsigned index){
    std::uint32_t value;std::memcpy(&value,data.data()+index*4,4);return value;
}
// This entry point consumes only the actual CBitmap::SaveInDDSUncompressed
// protocol. Other DDS/PNG/file loader policies remain separate.
inline TextureAsset Decode(std::span<const std::byte> data,std::uint32_t& format){
    if(data.size()<128)throw std::invalid_argument("short CBitmap DDS");
    const auto w=Word(data,4),h=Word(data,3),bits=Word(data,22);
    if(!w || !h || w>8192 || h>8192 || (bits!=24 && bits!=32))throw std::invalid_argument("CBitmap DDS extent/channels");
    const auto channels=bits/8;
    std::array<std::uint32_t,32> header{};
    header[0]=0x20534444;header[1]=124;header[2]=0x81007;header[3]=h;header[4]=w;header[5]=w*h*channels;
    header[19]=32;header[20]=channels==4?0x41:0x40;header[22]=bits;
    header[23]=0xff0000;header[24]=0xff00;header[25]=0xff;header[26]=channels==4?0xff000000:0;header[27]=0x1000;
    if(data.size()!=128+std::size_t(w)*h*channels || std::memcmp(data.data(),header.data(),128))
        throw std::invalid_argument("unsupported CBitmap DDS layout");
    format=channels==4?21:22;
    constexpr std::size_t budget=64*1024*1024;
    const unsigned dw=std::bit_ceil(w),dh=std::bit_ceil(h);
    const auto working=std::size_t(w)*h*4+std::size_t(dw)*dh*4+
        std::size_t(dw+dh)*(sizeof(std::vector<Weight>)+8*sizeof(Weight));
    if(working>budget)throw std::length_error("CBitmap triangle working byte limit");
    auto asset=TextureAsset::Decode(data);
    if(w==dw && h==dh)return asset; // D3DX's identical-layout copy path.
    auto horizontal=Axis(w,dw),vertical=Axis(h,dh);
    TextureLevel next{dw,dh,dw*4,std::vector<std::byte>(std::size_t(dw)*dh*4)};
    constexpr unsigned bayer[4][4]={{0,8,2,10},{12,4,14,6},{3,11,1,9},{15,7,13,5}};
    constexpr float normalize=1.f/255.f;
    const auto& src=asset.levels.front();
    for(unsigned y=0;y<dh;++y)for(unsigned x=0;x<dw;++x){
        std::array<float,4> sum{};
        for(const auto& wy:vertical[y])for(const auto& wx:horizontal[x]){
            const auto weight=Round(wy.value*wx.value);
            const auto* pixel=src.bytes.data()+std::size_t(wy.source)*src.pitch+wx.source*4;
            for(unsigned c=0;c<4;++c){const float value=Round(float(unsigned(pixel[c]))*normalize);
                sum[c]=Round(sum[c]+Round(value*weight));}
        }
        const auto phase=(y&1?dw-1-x:x)&3;
        const double bias=(31.-2.*bayer[y&3][phase])/32.;
        for(unsigned c=0;c<4;++c)next.bytes[(std::size_t(y)*dw+x)*4+c]=
            std::byte(unsigned(std::clamp(std::floor(double(sum[c])*255.+bias),0.,255.)));
    }
    asset.levels.front()=std::move(next);return asset;
}
}
