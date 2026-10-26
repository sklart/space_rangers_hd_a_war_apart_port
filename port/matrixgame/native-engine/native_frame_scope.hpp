// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "video_types.hpp"
#include <algorithm>
#include <cmath>
#include <span>
#include <stdexcept>
#include <vector>
namespace srhd_awa::matrixgame::platform3g {
struct ClearRectangle {std::int32_t left,top,right,bottom;bool operator==(const ClearRectangle&)const=default;};
static_assert(sizeof(ClearRectangle)==16);
inline std::vector<ClearRectangle> PlanNativeClear(const VideoViewport& viewport,unsigned width,unsigned height,
    std::span<const ClearRectangle> rectangles,unsigned flags,float depth,unsigned stencil){
    if(!width || !height || width>16384 || height>16384 || !viewport.Width || !viewport.Height ||
       !flags || flags>7 || ((flags&2) && (!std::isfinite(depth) || depth<0 || depth>1)) ||
       ((flags&4) && stencil>255) || rectangles.size()>4096)throw std::invalid_argument("native clear extent/flags/depth/stencil");
    const auto left=std::min<std::uint64_t>(viewport.X,width),top=std::min<std::uint64_t>(viewport.Y,height);
    const auto right=std::min<std::uint64_t>(std::uint64_t(viewport.X)+viewport.Width,width),bottom=std::min<std::uint64_t>(std::uint64_t(viewport.Y)+viewport.Height,height);
    const ClearRectangle bounds{int(left),int(top),int(right),int(bottom)};
    std::vector<ClearRectangle> result;result.reserve(std::max(std::size_t(1),rectangles.size()));
    const auto append=[&](ClearRectangle r){if(r.right<r.left || r.bottom<r.top)throw std::invalid_argument("reversed native clear rectangle");
        r.left=std::max(r.left,bounds.left);r.top=std::max(r.top,bounds.top);r.right=std::min(r.right,bounds.right);r.bottom=std::min(r.bottom,bounds.bottom);
        if(r.left<r.right && r.top<r.bottom)result.push_back(r);};
    if(rectangles.empty())append(bounds);else for(auto r:rectangles)append(r);
    return result;
}
class NativeFrameScope {
    enum class Phase {Idle,Acquired,Scene,Submitted};Phase phase_{Phase::Idle};
public:
    explicit operator bool()const noexcept{return phase_!=Phase::Idle;}
    void Acquire(){if(phase_!=Phase::Idle)throw std::logic_error("native frame already acquired");phase_=Phase::Acquired;}
    void BeginScene(){if(phase_!=Phase::Acquired)throw std::logic_error("native BeginScene requires acquired frame without scene");phase_=Phase::Scene;}
    void EndScene(){if(phase_!=Phase::Scene)throw std::logic_error("native EndScene requires open scene");phase_=Phase::Acquired;}
    void RequireDraw(bool compositor=false)const{if(phase_!=Phase::Scene && !(compositor && phase_==Phase::Acquired))throw std::logic_error("native draw outside scene");}
    void RequirePresent()const{if(phase_!=Phase::Acquired)throw std::logic_error("native Present requires acquired frame with closed scene");}
    void RequireClear()const{if(phase_!=Phase::Acquired && phase_!=Phase::Scene)throw std::logic_error("native clear requires unsubmitted acquired frame");}
    void RequireBatch()const{if(phase_!=Phase::Acquired && phase_!=Phase::Scene)throw std::logic_error("native batch requires acquired image before presentation submission");}
    void Submitted(){RequirePresent();phase_=Phase::Submitted;}
    void Presented(){if(phase_!=Phase::Submitted)throw std::logic_error("native completion without submission");phase_=Phase::Idle;}
    // Caller must first wait for GPU completion and cancel any dequeued image.
    void Abort()noexcept{phase_=Phase::Idle;}
};
// Optional runtime teardown contract, independent of scene submission.
class NativeFrameRollback {
public:
    virtual ~NativeFrameRollback()=default;
    virtual std::int32_t AbortNativeFrame()noexcept=0;
};
class NativeFrameProvider {
public:
    virtual ~NativeFrameProvider()=default;
    virtual std::int32_t NativeClear(std::span<const ClearRectangle>,std::uint32_t,std::uint32_t,float,std::uint32_t)=0;
    virtual std::int32_t NativeBeginScene()=0;
    virtual std::int32_t NativeEndScene()=0;
    virtual std::int32_t NativePresent()=0;
};
}
