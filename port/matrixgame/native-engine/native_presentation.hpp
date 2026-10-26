// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "platform_3g.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <span>
namespace srhd_awa::matrixgame::platform3g::presentation {
inline constexpr DeviceResult InvalidCall=static_cast<DeviceResult>(0x8876086cu);
inline constexpr DeviceResult NotAvailable=static_cast<DeviceResult>(0x8876086au);
inline constexpr DeviceResult DeviceLost=static_cast<DeviceResult>(0x88760868u);
inline constexpr DeviceResult OutOfMemory=static_cast<DeviceResult>(0x8876017cu);
// VI exposes resolution and a VSync event, not a fabricated desktop refresh.
// Observe successive VBlanks; reject a stalled or unreliable observation.
inline std::uint32_t RefreshFromVblanks(std::span<const std::uint64_t> nanoseconds){
    if(nanoseconds.size()!=9)throw std::invalid_argument("nine display VBlank timestamps required");
    std::array<std::uint64_t,8> intervals{};
    for(unsigned i=0;i<8;++i){if(nanoseconds[i+1]<=nanoseconds[i])throw std::invalid_argument("nonmonotonic display VBlank");intervals[i]=nanoseconds[i+1]-nanoseconds[i];}
    std::sort(intervals.begin(),intervals.end());const auto median=double(intervals[3])/2+double(intervals[4])/2;
    if(median<4'000'000 || median>100'000'000 || intervals.front()<median*.85 || intervals.back()>median*1.15)
        throw Unsupported("display VBlank observation unstable or missed; refresh not inferred");
    return std::uint32_t(std::lround(1e9/median));
}
inline DeviceResult Multisample(std::uint32_t adapter,std::uint32_t type,PixelFormat format,bool,std::uint32_t samples)noexcept{
    if(adapter || type!=1 || samples>16)return InvalidCall;
    if(format!=PixelFormat::Argb8 && format!=PixelFormat::Xrgb8 && format!=PixelFormat::Rgb565)return NotAvailable;
    // These presentation targets and the native pipeline are single-sample.
    return samples==0?DeviceSuccess:NotAvailable;
}
inline Presentation Plan(Presentation value,void* identity,std::uint32_t refresh){
    if(!identity || value.hDeviceWindow!=identity || !value.BackBufferWidth || !value.BackBufferHeight ||
       value.BackBufferWidth>8192 || value.BackBufferHeight>8192)throw std::invalid_argument("native presentation extent/window");
    if(value.MultiSampleType || value.MultiSampleQuality || Multisample(0,1,value.BackBufferFormat,value.Windowed,0)!=0 ||
       value.SwapEffect!=1 || (value.Flags&~2u) || !value.EnableAutoDepthStencil ||
       (value.AutoDepthStencilFormat!=PixelFormat::Depth24Stencil8 && value.AutoDepthStencilFormat!=PixelFormat::Depth24 && value.AutoDepthStencilFormat!=PixelFormat::Depth16) ||
       (value.PresentationInterval!=0 && value.PresentationInterval!=1 && value.PresentationInterval!=0x80000000u) ||
       (value.FullScreen_RefreshRateInHz && (value.Windowed || value.FullScreen_RefreshRateInHz!=refresh)))
        throw Unsupported("native presentation format/samples/depth/interval/physical refresh unsupported");
    if(value.BackBufferCount>3)throw Unsupported("native presentation buffer count");
    // NWindow enforces VSync with fewer than three buffers. Expose the actual
    // image count to the typed caller; never claim immediate mode with two.
    value.BackBufferCount=std::max(value.BackBufferCount,value.PresentationInterval==0x80000000u?3u:2u);
    return value;
}
struct Targets {Presentation value;explicit Targets(const Presentation& p):value(p){}virtual ~Targets()=default;};
class Driver {
public:
    virtual ~Driver()=default;
    virtual bool Healthy()const noexcept=0;
    virtual bool Busy()const noexcept=0;
    virtual bool DefaultResourcesLive()const noexcept=0;
    virtual std::unique_ptr<Targets> Allocate(const Presentation&)=0;
    virtual void Quiesce()=0;
    virtual void Attach(Targets&)=0;
    virtual void Detach(Targets&)=0;
    virtual void Commit(Targets&)noexcept=0;
};
// One render thread. Old backing allocations remain owned until replacement
// registration succeeds; rollback reattaches them without reallocation.
class Transaction {
    Driver& driver_;std::unique_ptr<Targets> active_;bool lost_{};
public:
    explicit Transaction(Driver& driver):driver_(driver){}
    const Targets* Active()const noexcept{return active_.get();}
    bool Ready()const noexcept{return active_ && !lost_ && driver_.Healthy();}
    DeviceResult Reset(const Presentation& planned)noexcept{
        if(!planned.BackBufferWidth || !planned.BackBufferHeight || planned.BackBufferWidth>8192 || planned.BackBufferHeight>8192 ||
           planned.BackBufferCount<2 || planned.BackBufferCount>3)return InvalidCall;
        if(!driver_.Healthy())return DeviceLost;
        if(driver_.Busy() || driver_.DefaultResourcesLive())return InvalidCall;
        std::unique_ptr<Targets> next;
        try{next=driver_.Allocate(planned);if(!next)throw std::bad_alloc();driver_.Quiesce();}
        catch(const std::bad_alloc&){return OutOfMemory;}catch(...){return DeviceLost;}
        bool touched=false;DeviceResult failure=DeviceSuccess;
        try{
            touched=true;if(active_)driver_.Detach(*active_);
            driver_.Attach(*next);
        }catch(const std::bad_alloc&){failure=OutOfMemory;}catch(...){failure=NotAvailable;}
        if(failure){
            try{if(touched)driver_.Detach(*next);if(active_)driver_.Attach(*active_);lost_=!active_;}
            catch(...){lost_=true;}
            return lost_?DeviceLost:failure;
        }
        active_=std::move(next);lost_=false;driver_.Commit(*active_);return DeviceSuccess;
    }
    DeviceResult Restore(const Presentation& baseline)noexcept{
        if(driver_.Busy())return InvalidCall;
        if(!Ready() || active_->value!=baseline)return Reset(baseline);
        return DeviceSuccess;
    }
    void Release(){driver_.Quiesce();if(active_)driver_.Detach(*active_);active_.reset();lost_=false;}
};
struct Lifetime {Transaction* owner{};};
class RestoreLease final:public DeviceStateLease {
    std::shared_ptr<Lifetime> lifetime_;Presentation baseline_;std::unique_ptr<DeviceStateLease> state_;
public:
    RestoreLease(std::shared_ptr<Lifetime> lifetime,const Presentation& baseline,std::unique_ptr<DeviceStateLease> state)
        :lifetime_(std::move(lifetime)),baseline_(baseline),state_(std::move(state)){
        if(!lifetime_ || !lifetime_->owner || !state_)throw std::invalid_argument("native presentation restore lease");
    }
    DeviceResult Restore()noexcept override{
        if(!lifetime_->owner)return DeviceLost;
        const auto result=lifetime_->owner->Restore(baseline_);return result?result:state_->Restore();
    }
};
}
